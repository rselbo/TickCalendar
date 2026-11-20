#include "XlsxExport.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>

#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>

// An .xlsx/.xlsm file is a zip archive of XML files. The cells of a worksheet
// live in <sheetData> as <row r="10"><c r="C10" s="26">…</c>…</row>. The
// sheet XML is edited as text so that everything we don't touch (namespaces,
// extension elements, formatting) stays byte for byte the same.

namespace XlsxExport {
namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("XlsxExport", text);
}

struct CellPos {
    int column = 0; // A == 1
    int row = 0;
};

CellPos parseRef(const QString &ref) {
    CellPos pos;
    qsizetype i = 0;
    for (; i < ref.size() && ref[i].isLetter(); ++i)
        pos.column = pos.column * 26 + (ref[i].toUpper().unicode() - u'A' + 1);
    pos.row = ref.mid(i).toInt();
    return pos;
}

QString cellXml(const QString &ref, const QString &styleAttr, const QVariant &value) {
    if (value.typeId() == QMetaType::Double) {
        return QStringLiteral("<c r=\"%1\"%2><v>%3</v></c>")
            .arg(ref, styleAttr, QString::number(value.toDouble(), 'g', 15));
    }
    // Inline strings avoid having to update the shared string table.
    return QStringLiteral("<c r=\"%1\"%2 t=\"inlineStr\"><is><t xml:space=\"preserve\">%3</t></is></c>")
        .arg(ref, styleAttr, value.toString().toHtmlEscaped());
}

// Inserts newCell into the row body, keeping the cells in column order.
void insertIntoRow(QString &rowBody, int column, const QString &newCell) {
    static const QRegularExpression cellStart(QStringLiteral(R"re(<c r="([A-Z]+)\d+")re"));
    auto it = cellStart.globalMatch(rowBody);
    while (it.hasNext()) {
        const auto match = it.next();
        if (parseRef(match.captured(1) + u'1').column > column) {
            rowBody.insert(match.capturedStart(), newCell);
            return;
        }
    }
    rowBody.append(newCell);
}

QString setCell(QString &sheet, const Cell &cell) {
    const CellPos pos = parseRef(cell.ref);

    // An existing cell: replace it, keeping its style.
    const QRegularExpression cellRe(
        QStringLiteral(R"re(<c r="%1"(?=[\s/>])([^>]*?)(/>|>.*?</c>))re").arg(cell.ref),
        QRegularExpression::DotMatchesEverythingOption);
    if (const auto match = cellRe.match(sheet); match.hasMatch()) {
        static const QRegularExpression styleRe(QStringLiteral(R"re(\ss="\d+")re"));
        const QString style = styleRe.match(match.captured(1)).captured(0);
        sheet.replace(match.capturedStart(), match.capturedLength(),
                      cellXml(cell.ref, style, cell.value));
        return {};
    }

    const QString newCell = cellXml(cell.ref, {}, cell.value);

    // An existing row without that cell: add the cell to it.
    const QRegularExpression rowRe(
        QStringLiteral(R"re(<row r="%1"(?=[\s/>])([^>]*?)(/>|>(.*?)</row>))re").arg(pos.row),
        QRegularExpression::DotMatchesEverythingOption);
    if (const auto match = rowRe.match(sheet); match.hasMatch()) {
        QString body = match.captured(3);
        insertIntoRow(body, pos.column, newCell);
        sheet.replace(match.capturedStart(), match.capturedLength(),
                      QStringLiteral("<row r=\"%1\"%2>%3</row>")
                          .arg(QString::number(pos.row), match.captured(1), body));
        return {};
    }

    // No such row: add one, keeping the rows in order.
    const QString newRow = QStringLiteral("<row r=\"%1\">%2</row>").arg(QString::number(pos.row), newCell);
    if (const qsizetype empty = sheet.indexOf(QStringLiteral("<sheetData/>")); empty >= 0) {
        sheet.replace(empty, 12, QStringLiteral("<sheetData>%1</sheetData>").arg(newRow));
        return {};
    }
    static const QRegularExpression rowStart(QStringLiteral(R"re(<row r="(\d+)")re"));
    auto it = rowStart.globalMatch(sheet);
    while (it.hasNext()) {
        const auto match = it.next();
        if (match.captured(1).toInt() > pos.row) {
            sheet.insert(match.capturedStart(), newRow);
            return {};
        }
    }
    const qsizetype end = sheet.indexOf(QStringLiteral("</sheetData>"));
    if (end < 0)
        return tr("The worksheet has no cell data.");
    sheet.insert(end, newRow);
    return {};
}

// Finds the zip path of the first worksheet via xl/workbook.xml and its
// relationships file.
QString firstSheetPath(const QString &workbook, const QString &rels) {
    static const QRegularExpression sheetRe(QStringLiteral(R"re(<sheet\s[^>]*?r:id="([^"]+)")re"));
    const QString id = sheetRe.match(workbook).captured(1);
    if (id.isEmpty())
        return {};
    const QRegularExpression relRe(
        QStringLiteral(R"re(<Relationship\s[^>]*?Id="%1"[^>]*>)re").arg(QRegularExpression::escape(id)));
    static const QRegularExpression targetRe(QStringLiteral(R"re(Target="([^"]+)")re"));
    const QString target = targetRe.match(relRe.match(rels).captured(0)).captured(1);
    if (target.isEmpty())
        return {};
    return target.startsWith(u'/') ? target.mid(1) : QStringLiteral("xl/") + target;
}

} // namespace

QString fillWorkbook(const QString &templatePath, const QString &outPath, const QList<Cell> &cells) {
    QZipReader reader(templatePath);
    if (!reader.isReadable() || reader.status() != QZipReader::NoError)
        return tr("Could not read the template %1.").arg(templatePath);

    const QString workbookPath = QStringLiteral("xl/workbook.xml");
    QString workbook = QString::fromUtf8(reader.fileData(workbookPath));
    const QString rels = QString::fromUtf8(reader.fileData(QStringLiteral("xl/_rels/workbook.xml.rels")));
    const QString sheetPath = firstSheetPath(workbook, rels);
    if (workbook.isEmpty() || sheetPath.isEmpty())
        return tr("The template %1 is not an Excel workbook.").arg(templatePath);

    QString sheet = QString::fromUtf8(reader.fileData(sheetPath));
    if (sheet.isEmpty())
        return tr("The template %1 has no worksheet.").arg(templatePath);
    for (const Cell &cell : cells) {
        if (QString error = setCell(sheet, cell); !error.isEmpty())
            return error;
    }

    // The cached results of formulas (sums etc.) are now stale; make Excel
    // recalculate everything when the file is opened.
    if (!workbook.contains(QStringLiteral("fullCalcOnLoad"))) {
        if (workbook.contains(QStringLiteral("<calcPr")))
            workbook.replace(QStringLiteral("<calcPr"), QStringLiteral("<calcPr fullCalcOnLoad=\"1\""));
        else
            workbook.replace(QStringLiteral("</workbook>"),
                             QStringLiteral("<calcPr fullCalcOnLoad=\"1\"/></workbook>"));
    }

    QHash<QString, QByteArray> replaced{
        {workbookPath, workbook.toUtf8()},
        {sheetPath, sheet.toUtf8()},
    };

    // Built in memory and then written with QSaveFile, so a failed export
    // never leaves a half-written file behind. (QZipWriter closes its device
    // when done, which QSaveFile does not allow.)
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    {
        QZipWriter writer(&buffer);
        writer.setCompressionPolicy(QZipWriter::AutoCompress);
        const auto entries = reader.fileInfoList();
        for (const QZipReader::FileInfo &entry : entries) {
            if (entry.isDir) {
                writer.addDirectory(entry.filePath);
            } else if (entry.isFile) {
                const auto it = replaced.constFind(entry.filePath);
                writer.addFile(entry.filePath,
                               it != replaced.cend() ? *it : reader.fileData(entry.filePath));
            }
            if (writer.status() != QZipWriter::NoError)
                return tr("Could not write %1.").arg(outPath);
        }
        writer.close();
    }

    QSaveFile out(outPath);
    if (!out.open(QIODevice::WriteOnly) || out.write(buffer.data()) != buffer.data().size())
        return tr("Could not write %1: %2").arg(outPath, out.errorString());
    if (!out.commit())
        return tr("Could not write %1: %2").arg(outPath, out.errorString());
    return {};
}

} // namespace XlsxExport
