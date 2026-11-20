import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import TickCalendar

ApplicationWindow {
    id: window

    width: 480
    height: 460
    minimumWidth: 320
    minimumHeight: 320
    visible: true
    title: (store.modified ? "*" : "") + store.fileName + " — TickCalendar"

    property int shownYear: new Date().getFullYear()
    property int shownMonth: new Date().getMonth() // 0-based, as MonthGrid expects

    // Action to run once unsaved changes have been saved or discarded.
    property var pendingAction: null
    property bool closeConfirmed: false

    TickStore {
        id: store
        onErrorOccurred: message => {
            errorDialog.text = message
            errorDialog.open()
        }
    }

    function previousMonth() {
        if (shownMonth === 0) {
            shownMonth = 11
            shownYear--
        } else {
            shownMonth--
        }
    }

    function nextMonth() {
        if (shownMonth === 11) {
            shownMonth = 0
            shownYear++
        } else {
            shownMonth++
        }
    }

    // Runs action right away, or after asking what to do with unsaved changes.
    function runAfterConfirm(action) {
        if (!store.modified) {
            action()
            return
        }
        pendingAction = action
        unsavedDialog.open()
    }

    function runPendingAction() {
        const action = pendingAction
        pendingAction = null
        if (action)
            action()
    }

    // Saves, asking for a file name if there is none. Calls onSaved afterwards.
    function saveThen(onSaved) {
        if (store.fileUrl.toString() === "") {
            saveAsDialog.onSaved = onSaved
            saveAsDialog.open()
        } else if (store.save() && onSaved) {
            onSaved()
        }
    }

    onClosing: close => {
        if (store.modified && !closeConfirmed) {
            close.accepted = false
            runAfterConfirm(() => {
                closeConfirmed = true
                window.close()
            })
        }
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Action {
                text: qsTr("&New")
                shortcut: StandardKey.New
                onTriggered: runAfterConfirm(() => store.newFile())
            }
            Action {
                text: qsTr("&Open…")
                shortcut: StandardKey.Open
                onTriggered: runAfterConfirm(() => openDialog.open())
            }
            Action {
                text: qsTr("&Save")
                shortcut: StandardKey.Save
                onTriggered: saveThen(null)
            }
            Action {
                text: qsTr("Save &As…")
                shortcut: StandardKey.SaveAs
                onTriggered: {
                    saveAsDialog.onSaved = null
                    saveAsDialog.open()
                }
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Quit")
                shortcut: StandardKey.Quit
                onTriggered: window.close()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: "‹"
                font.pixelSize: 20
                onClicked: previousMonth()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Previous month")
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 18
                font.bold: true
                text: new Date(shownYear, shownMonth, 1).toLocaleDateString(Qt.locale(), "MMMM yyyy")
            }
            Button {
                text: "›"
                font.pixelSize: 20
                onClicked: nextMonth()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Next month")
            }
        }

        DayOfWeekRow {
            Layout.fillWidth: true
            locale: grid.locale
        }

        MonthGrid {
            id: grid

            Layout.fillWidth: true
            Layout.fillHeight: true
            year: shownYear
            month: shownMonth
            locale: Qt.locale()

            delegate: Rectangle {
                id: cell

                required property var model
                readonly property bool inMonth: model.month === grid.month
                // store.revision makes this re-evaluate when ticks change.
                readonly property bool ticked: store.revision >= 0
                                               && store.isTicked(model.year, model.month + 1, model.day)

                radius: 6
                color: ticked && inMonth ? palette.highlight
                                         : mouse.containsMouse && inMonth ? palette.midlight
                                                                          : "transparent"
                border.width: model.today ? 2 : 0
                border.color: palette.highlight

                Label {
                    anchors.centerIn: parent
                    text: cell.model.day
                    font.bold: cell.ticked && cell.inMonth
                    color: cell.ticked && cell.inMonth ? palette.highlightedText : palette.text
                    opacity: cell.inMonth ? 1.0 : 0.35
                }

                Label {
                    visible: cell.ticked && cell.inMonth
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 4
                    text: "✓"
                    color: palette.highlightedText
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    enabled: cell.inMonth
                    hoverEnabled: true
                    cursorShape: cell.inMonth ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: store.toggle(cell.model.year, cell.model.month + 1, cell.model.day)
                }
            }
        }
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open calendar")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("TickCalendar files (*.tick *.sqlite *.db)"), qsTr("All files (*)")]
        onAccepted: store.open(selectedFile)
    }

    FileDialog {
        id: saveAsDialog
        property var onSaved: null
        title: qsTr("Save calendar as")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "tick"
        nameFilters: [qsTr("TickCalendar files (*.tick *.sqlite *.db)"), qsTr("All files (*)")]
        onAccepted: {
            const callback = onSaved
            onSaved = null
            if (store.saveAs(selectedFile) && callback)
                callback()
        }
        onRejected: {
            onSaved = null
            pendingAction = null
        }
    }

    MessageDialog {
        id: unsavedDialog
        title: qsTr("Unsaved changes")
        text: qsTr("The calendar has unsaved changes.")
        informativeText: qsTr("Do you want to save them?")
        buttons: MessageDialog.Save | MessageDialog.Discard | MessageDialog.Cancel
        onButtonClicked: (button, role) => {
            switch (button) {
            case MessageDialog.Save:
                saveThen(runPendingAction)
                break
            case MessageDialog.Discard:
                runPendingAction()
                break
            default:
                pendingAction = null
            }
        }
    }

    MessageDialog {
        id: errorDialog
        title: qsTr("TickCalendar")
        buttons: MessageDialog.Ok
    }
}
