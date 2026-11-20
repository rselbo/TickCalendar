#pragma once

#include <QList>
#include <QString>
#include <QVariant>

// Fills in cells of the first worksheet of an Excel workbook (.xlsx/.xlsm)
// and saves the result as a new file. Everything else in the workbook
// (macros, styles, formulas, other sheets) is copied unchanged.
namespace XlsxExport {

struct Cell {
    QString ref;    // e.g. "D3"
    QVariant value; // a QString is written as text, a double as a number
};

// Copies templatePath to outPath with the given cells set. Each cell keeps
// the style it has in the template. Excel recalculates all formulas when the
// file is opened. Returns an empty string on success, or an error message.
[[nodiscard]] QString fillWorkbook(const QString &templatePath, const QString &outPath,
                                   const QList<Cell> &cells);

} // namespace XlsxExport
