pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Edits the trip settings. They are stored with QSettings in the [trip]
// group of the app's config file. The fields are filled from the stored
// values when the dialog opens, and written back only on OK.
Dialog {
    id: dialog

    title: qsTr("Settings")
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    // Covers the whole window, so the calendar is hidden while it is open.
    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : implicitWidth
    height: parent ? parent.height : implicitHeight

    // The stored values, readable from outside the dialog.
    readonly property alias settings: tripSettings

    Settings {
        id: tripSettings
        category: "trip"

        property string templateFile
        property string employeeName
        property string employeeNumber
        property string routeToCustomer
        property string descriptionToCustomer
        property real kmToCustomer
        property string routeFromCustomer
        property string descriptionFromCustomer
        property real kmFromCustomer
        property real tollAmount
    }

    // Numbers are shown and typed in the user's locale, e.g. "12,5" in
    // Norwegian. Empty or unreadable text counts as 0.
    function formatNumber(value: real): string {
        return value === 0 ? "" : value.toLocaleString(Qt.locale(), "g", 15)
    }

    function parseNumber(text: string): real {
        if (text.trim() === "")
            return 0
        try {
            return Number.fromLocaleString(Qt.locale(), text.trim())
        } catch (e) {
            return 0
        }
    }

    // Shows a file:// URL from the file dialog as a plain path.
    function urlToPath(url: url): string {
        const text = url.toString()
        return text.startsWith("file://") ? decodeURIComponent(text.substring(7)) : text
    }

    onAboutToShow: {
        templateField.text = tripSettings.templateFile
        employeeNameField.text = tripSettings.employeeName
        employeeNumberField.text = tripSettings.employeeNumber
        routeToCustomerField.text = tripSettings.routeToCustomer
        descriptionToCustomerField.text = tripSettings.descriptionToCustomer
        kmToCustomerField.text = formatNumber(tripSettings.kmToCustomer)
        routeFromCustomerField.text = tripSettings.routeFromCustomer
        descriptionFromCustomerField.text = tripSettings.descriptionFromCustomer
        kmFromCustomerField.text = formatNumber(tripSettings.kmFromCustomer)
        tollField.text = formatNumber(tripSettings.tollAmount)
    }

    onAccepted: {
        tripSettings.templateFile = templateField.text.trim()
        tripSettings.employeeName = employeeNameField.text.trim()
        tripSettings.employeeNumber = employeeNumberField.text.trim()
        tripSettings.routeToCustomer = routeToCustomerField.text.trim()
        tripSettings.descriptionToCustomer = descriptionToCustomerField.text.trim()
        tripSettings.kmToCustomer = parseNumber(kmToCustomerField.text)
        tripSettings.routeFromCustomer = routeFromCustomerField.text.trim()
        tripSettings.descriptionFromCustomer = descriptionFromCustomerField.text.trim()
        tripSettings.kmFromCustomer = parseNumber(kmFromCustomerField.text)
        tripSettings.tollAmount = parseNumber(tollField.text)
        tripSettings.sync()
    }

    component NumberValidator: DoubleValidator {
        bottom: 0
        notation: DoubleValidator.StandardNotation
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth

        GridLayout {
            width: scroll.availableWidth
            columns: 3
            columnSpacing: 8
            rowSpacing: 6

            Label { text: qsTr("Template file:") }
            RowLayout {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                TextField {
                    id: templateField
                    Layout.fillWidth: true
                    placeholderText: qsTr("No template selected")
                }
                Button {
                    text: qsTr("Browse…")
                    onClicked: templateDialog.open()
                }
            }

            Label { text: qsTr("Employee name:") }
            TextField {
                id: employeeNameField
                Layout.columnSpan: 2
                Layout.fillWidth: true
            }

            Label { text: qsTr("Employee number:") }
            TextField {
                id: employeeNumberField
                Layout.columnSpan: 2
                Layout.fillWidth: true
            }

            // Column headings.
            Item { implicitWidth: 1; implicitHeight: 1 }
            Label {
                Layout.topMargin: 6
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                font.bold: true
                text: qsTr("To Customer")
            }
            Label {
                Layout.topMargin: 6
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                font.bold: true
                text: qsTr("From Customer")
            }

            Label { text: qsTr("Route:") }
            TextField {
                id: routeToCustomerField
                Layout.fillWidth: true
                Layout.preferredWidth: 1
            }
            TextField {
                id: routeFromCustomerField
                Layout.fillWidth: true
                Layout.preferredWidth: 1
            }

            Label { text: qsTr("Description:") }
            TextField {
                id: descriptionToCustomerField
                Layout.fillWidth: true
                Layout.preferredWidth: 1
            }
            TextField {
                id: descriptionFromCustomerField
                Layout.fillWidth: true
                Layout.preferredWidth: 1
            }

            Label { text: qsTr("KM:") }
            TextField {
                id: kmToCustomerField
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                validator: NumberValidator {}
            }
            TextField {
                id: kmFromCustomerField
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                validator: NumberValidator {}
            }

            Label {
                Layout.topMargin: 6
                text: qsTr("Toll amount:")
            }
            TextField {
                id: tollField
                Layout.topMargin: 6
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                validator: NumberValidator { decimals: 2 }
            }
        }
    }

    FileDialog {
        id: templateDialog
        title: qsTr("Choose template file")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("All files (*)")]
        onAccepted: templateField.text = dialog.urlToPath(selectedFile)
    }
}
