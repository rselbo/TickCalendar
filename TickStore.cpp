#include "TickStore.h"
#include "XlsxExport.h"

#include <algorithm>

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {

constexpr auto kDateFormat = Qt::ISODate;
constexpr auto kLastFileKey = "lastFile";

// Dates in the clipboard text and in exported workbooks.
const auto kExportDateFormat = QStringLiteral("dd.MM.yyyy");
// The rows of the template's trip table (C10:L75); everything below is sums.
constexpr qsizetype kExportFirstRow = 10;
constexpr qsizetype kExportLastRow = 75;

// Opens a private SQLite connection for the lifetime of the object and
// removes it again afterwards.
class ScopedDb {
public:
    explicit ScopedDb(const QString &path)
        : m_name(QUuid::createUuid().toString()) {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        db.setDatabaseName(path);
        m_ok = db.open();
        if (!m_ok)
            m_error = db.lastError().text();
    }

    ~ScopedDb() {
        {
            auto db = QSqlDatabase::database(m_name, false);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_name);
    }

    ScopedDb(const ScopedDb &) = delete;
    ScopedDb &operator=(const ScopedDb &) = delete;

    [[nodiscard]] bool ok() const { return m_ok; }
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] QSqlDatabase db() const { return QSqlDatabase::database(m_name, false); }

private:
    QString m_name;
    QString m_error;
    bool m_ok = false;
};

} // namespace

TickStore::TickStore(QObject *parent) : QObject(parent) {}

QString TickStore::fileName() const {
    if (m_fileUrl.isEmpty())
        return tr("Untitled");
    return QFileInfo(m_fileUrl.toLocalFile()).fileName();
}

bool TickStore::isTicked(int year, int month, int day) const {
    return m_ticks.contains(QDate(year, month, day));
}

void TickStore::toggle(int year, int month, int day) {
    const QDate date(year, month, day);
    if (!date.isValid())
        return;
    if (!m_ticks.remove(date))
        m_ticks.insert(date);
    ++m_revision;
    emit ticksChanged();
    setModified(true);
}

void TickStore::newFile() {
    m_ticks.clear();
    ++m_revision;
    emit ticksChanged();
    setFileUrl({});
    setModified(false);
}

bool TickStore::open(const QUrl &url) {
    const QString path = url.toLocalFile();
    if (!QFileInfo::exists(path)) {
        emit errorOccurred(tr("File does not exist: %1").arg(path));
        return false;
    }

    QSet<QDate> loaded;
    {
        ScopedDb scoped(path);
        if (!scoped.ok()) {
            emit errorOccurred(tr("Could not open %1: %2").arg(path, scoped.error()));
            return false;
        }
        QSqlQuery query(scoped.db());
        if (!query.exec(QStringLiteral("SELECT date FROM ticks"))) {
            emit errorOccurred(tr("%1 is not a TickCalendar file: %2")
                                   .arg(path, query.lastError().text()));
            return false;
        }
        while (query.next()) {
            const QDate date = QDate::fromString(query.value(0).toString(), kDateFormat);
            if (date.isValid())
                loaded.insert(date);
        }
    }

    m_ticks = std::move(loaded);
    ++m_revision;
    emit ticksChanged();
    setFileUrl(url);
    setModified(false);
    return true;
}

bool TickStore::save() {
    if (m_fileUrl.isEmpty()) {
        emit errorOccurred(tr("No file selected"));
        return false;
    }
    if (!writeTo(m_fileUrl.toLocalFile()))
        return false;
    setModified(false);
    return true;
}

bool TickStore::saveAs(const QUrl &url) {
    if (!writeTo(url.toLocalFile()))
        return false;
    setFileUrl(url);
    setModified(false);
    return true;
}

QList<QDate> TickStore::ticksInMonth(int year, int month) const {
    QList<QDate> dates;
    for (const QDate &date : m_ticks) {
        if (date.year() == year && date.month() == month)
            dates.append(date);
    }
    std::ranges::sort(dates);
    return dates;
}

QString TickStore::monthAsText(int year, int month) const {
    const QList<QDate> dates = ticksInMonth(year, month);
    QStringList lines;
    lines.reserve(dates.size() * 2);
    for (const QDate &date : std::as_const(dates)) {
        const QString text = date.toString(kExportDateFormat);
        lines << text << text;
    }
    return lines.join(u'\n');
}

void TickStore::copyMonth(int year, int month) const {
    QGuiApplication::clipboard()->setText(monthAsText(year, month));
}

bool TickStore::exportTo(const QUrl &url, int year, int month, const QVariantMap &trip) {
    const QString templatePath = trip.value(QStringLiteral("templateFile")).toString();
    if (templatePath.isEmpty()) {
        emit errorOccurred(tr("No template file is set. Choose one in File → Settings."));
        return false;
    }
    if (!QFileInfo::exists(templatePath)) {
        emit errorOccurred(tr("Template file does not exist: %1").arg(templatePath));
        return false;
    }

    const QList<QDate> dates = ticksInMonth(year, month);
    // Each day takes two rows.
    const auto lastRow = kExportFirstRow + 2 * dates.size() - 1;
    if (lastRow > kExportLastRow) {
        emit errorOccurred(tr("Too many ticked days to export: %1. The template has room for %2.")
                               .arg(dates.size())
                               .arg((kExportLastRow - kExportFirstRow + 1) / 2));
        return false;
    }

    QList<XlsxExport::Cell> cells;
    // Text cells are left alone when the setting is empty, and number cells
    // when it is 0, so the template's own content stays.
    auto addText = [&](const QString &ref, const QString &key) {
        const QString text = trip.value(key).toString();
        if (!text.isEmpty())
            cells.append({ref, text});
    };
    auto addNumber = [&](const QString &ref, const QString &key) {
        const double number = trip.value(key).toDouble();
        if (number != 0)
            cells.append({ref, number});
    };
    auto cellRef = [](char column, qsizetype row) {
        return QStringLiteral("%1%2").arg(QChar::fromLatin1(column)).arg(row);
    };

    // An employee number of plain digits is written as a number, so Excel
    // doesn't flag it as a number stored as text.
    const QString number = trip.value(QStringLiteral("employeeNumber")).toString();
    bool isInteger = false;
    const qlonglong numberValue = number.toLongLong(&isInteger);
    if (isInteger && !number.startsWith(u'0') && !number.startsWith(u'+') && !number.startsWith(u'-'))
        cells.append({QStringLiteral("D2"), static_cast<double>(numberValue)});
    else
        addText(QStringLiteral("D2"), QStringLiteral("employeeNumber"));
    addText(QStringLiteral("D3"), QStringLiteral("employeeName"));

    qsizetype row = kExportFirstRow;
    for (const QDate &date : dates) {
        const QString dateText = date.toString(kExportDateFormat);
        // First the trip to the customer, then the trip back.
        for (const QString &suffix : {QStringLiteral("ToCustomer"), QStringLiteral("FromCustomer")}) {
            addText(cellRef('C', row), QStringLiteral("route") + suffix);
            addText(cellRef('D', row), QStringLiteral("description") + suffix);
            addNumber(cellRef('E', row), QStringLiteral("km") + suffix);
            cells.append({cellRef('H', row), dateText});
            addNumber(cellRef('L', row), QStringLiteral("tollAmount"));
            ++row;
        }
    }

    const QString path = url.toLocalFile();
    if (const QString error = XlsxExport::fillWorkbook(templatePath, path, cells); !error.isEmpty()) {
        emit errorOccurred(error);
        return false;
    }
    return true;
}

bool TickStore::restoreLastFile() {
    QSettings settings;
    const QUrl url = settings.value(kLastFileKey).toUrl();
    if (url.isEmpty())
        return false;
    if (!QFileInfo::exists(url.toLocalFile())) {
        // The file was moved or deleted since last time; start fresh.
        settings.remove(kLastFileKey);
        return false;
    }
    return open(url);
}

bool TickStore::writeTo(const QString &path) {
    ScopedDb scoped(path);
    if (!scoped.ok()) {
        emit errorOccurred(tr("Could not open %1: %2").arg(path, scoped.error()));
        return false;
    }
    auto db = scoped.db();
    QSqlQuery query(db);

    auto fail = [&](const QSqlQuery &q) {
        emit errorOccurred(tr("Could not save %1: %2").arg(path, q.lastError().text()));
        db.rollback();
        return false;
    };

    if (!db.transaction()) {
        emit errorOccurred(tr("Could not save %1: %2").arg(path, db.lastError().text()));
        return false;
    }
    if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS ticks (date TEXT PRIMARY KEY)")))
        return fail(query);
    if (!query.exec(QStringLiteral("DELETE FROM ticks")))
        return fail(query);
    if (!query.prepare(QStringLiteral("INSERT INTO ticks (date) VALUES (?)")))
        return fail(query);
    for (const QDate &date : std::as_const(m_ticks)) {
        query.addBindValue(date.toString(kDateFormat));
        if (!query.exec())
            return fail(query);
    }
    if (!db.commit()) {
        emit errorOccurred(tr("Could not save %1: %2").arg(path, db.lastError().text()));
        db.rollback();
        return false;
    }
    return true;
}

void TickStore::setModified(bool modified) {
    if (m_modified == modified)
        return;
    m_modified = modified;
    emit modifiedChanged();
}

void TickStore::setFileUrl(const QUrl &url) {
    if (m_fileUrl == url)
        return;
    m_fileUrl = url;
    emit fileUrlChanged();

    // Remember the current file so it is reopened on the next start. Saving
    // it on every change (rather than on exit) also survives crashes.
    QSettings settings;
    if (url.isEmpty())
        settings.remove(kLastFileKey);
    else
        settings.setValue(kLastFileKey, url.toString());
}
