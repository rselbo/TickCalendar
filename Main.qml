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

    Component.onCompleted: store.restoreLastFile()

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

        // Weekday headings, always Monday first. Names follow the locale.
        RowLayout {
            Layout.fillWidth: true
            spacing: 0

            Repeater {
                // JS weekday numbers: 1 = Monday … 6 = Saturday, 0 = Sunday.
                model: [1, 2, 3, 4, 5, 6, 0]
                delegate: Label {
                    required property int modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    horizontalAlignment: Text.AlignHCenter
                    font.bold: true
                    text: Qt.locale().dayName(modelData, Locale.ShortFormat)
                }
            }
        }

        // Six weeks starting on the Monday on or before the 1st of the month.
        // A custom grid is used instead of MonthGrid because MonthGrid takes
        // the first day of the week from the locale, which can't be overridden.
        GridLayout {
            id: grid

            readonly property int leadingDays: (new Date(shownYear, shownMonth, 1).getDay() + 6) % 7
            readonly property date today: new Date()

            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 7
            rowSpacing: 2
            columnSpacing: 2

            Repeater {
                model: 42

                delegate: Rectangle {
                    id: cell

                    required property int index
                    // new Date() normalises out-of-range days into the
                    // previous/next month.
                    readonly property var date: new Date(shownYear, shownMonth, 1 - grid.leadingDays + index)
                    readonly property int year: date.getFullYear()
                    readonly property int month: date.getMonth() // 0-based
                    readonly property int day: date.getDate()
                    readonly property bool inMonth: month === shownMonth
                    readonly property bool isToday: year === grid.today.getFullYear()
                                                    && month === grid.today.getMonth()
                                                    && day === grid.today.getDate()
                    // store.revision makes this re-evaluate when ticks change.
                    readonly property bool ticked: store.revision >= 0
                                                   && store.isTicked(year, month + 1, day)

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 1
                    radius: 6
                    color: ticked && inMonth ? palette.highlight
                                             : mouse.containsMouse && inMonth ? palette.midlight
                                                                              : "transparent"
                    border.width: isToday ? 2 : 0
                    border.color: palette.highlight

                    Label {
                        anchors.centerIn: parent
                        text: cell.day
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
                        onClicked: store.toggle(cell.year, cell.month + 1, cell.day)
                    }
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
