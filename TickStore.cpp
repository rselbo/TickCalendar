#include "TickStore.h"

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

QString TickStore::monthAsText(int year, int month) const {
    QList<QDate> dates;
    for (const QDate &date : m_ticks) {
        if (date.year() == year && date.month() == month)
            dates.append(date);
    }
    std::ranges::sort(dates);

    QStringList lines;
    lines.reserve(dates.size() * 2);
    for (const QDate &date : std::as_const(dates)) {
        const QString text = date.toString(QStringLiteral("dd.MM.yyyy"));
        lines << text << text;
    }
    return lines.join(u'\n');
}

void TickStore::copyMonth(int year, int month) const {
    QGuiApplication::clipboard()->setText(monthAsText(year, month));
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
