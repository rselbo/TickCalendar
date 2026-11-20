# TickCalendar

A small desktop app for ticking off days in a calendar. It shows one month at
a time. Click a day to tick it and click it again to un-tick it. Ticks are
saved to an SQLite file that you choose.

Built with Qt 6 Quick (QML) and C++.

## Features

- One month at a time, weeks always starting on Monday, with buttons to go
  to the previous and next month.
- Click a day in the month to tick it; click again to remove the tick.
  Today is outlined.
- **File** menu: New, Open, Save, Save As, Export, Settings and Exit. If
  there are unsaved changes, you are asked whether to save them before New,
  Open or closing the window. Unsaved changes are shown with a `*` in the
  window title.
- **File → Export…** fills in the template workbook from Settings with the
  ticked days of the month on screen and saves it under a new name. The
  dialog suggests `Kjørebok <yyyy><MM>.xlsm` (e.g. `Kjørebok 202611.xlsm`)
  and starts in the folder of the last export, or in your Documents folder
  the first time. See [Export](#export).
- **Edit → Copy** (Ctrl+C) copies the ticked days of the month on screen to
  the clipboard, earliest first. Each date is in `dd.MM.yyyy` format and
  appears twice, one per line:

  ```
  03.11.2026
  03.11.2026
  20.11.2026
  20.11.2026
  ```

- **File → Settings…** (Ctrl+,) sets a template file (typed in or
  chosen with Browse), employee name and number, route, description and KM
  for both the trip to the customer and the trip back (the "To Customer" and
  "From Customer" columns), and a toll amount.
  KM and toll amount are numbers typed in your locale's format, e.g. `12,5`
  in Norwegian. Changes are saved when you press OK.
- The file that was open when the app closed is reopened on the next start.

## Building

Requirements:

- CMake 4.2 or newer
- A C++23 compiler
- Qt 6.5 or newer with the Qt Quick, Qt Quick Controls, Qt Quick Dialogs,
  Qt Sql and Qt Widgets modules, and the Qt SQLite driver (`QSQLITE`).
  Excel files are read and written with Qt's private zip classes, so the
  Qt Core private headers must be installed too (they are part of the
  normal Qt packages on most Linux distributions).

```sh
cmake -S . -B build
cmake --build build
./build/TickCalendar
```

The build type defaults to `Release`, which is optimized (`-O3`, link-time
optimization) and has `qDebug()` output turned off. For a debug build, add
`-DCMAKE_BUILD_TYPE=Debug` to the first command.

All builds turn on compiler warnings and treat them as errors. To build anyway
when there are warnings, add `--compile-no-warning-as-error` to the first
command.

## Installing

```sh
cmake --install build --prefix ~/.local/share/TickCalendar
ln -s ~/.local/share/TickCalendar/share/applications/TickCalendar.desktop \
      ~/.local/share/applications/
```

This installs the program to `bin/TickCalendar`, its icon to
`share/icons/hicolor/scalable/apps/tickcalendar.svg` and a desktop entry to
`share/applications/TickCalendar.desktop` under the prefix. The link makes
TickCalendar show up in the application launcher; it is only needed once.

## File format

A TickCalendar file is an SQLite database with one table:

```sql
CREATE TABLE ticks (date TEXT PRIMARY KEY);
```

Each row is one ticked day as an ISO date, e.g. `2026-11-20`. New files get
the `.tick` extension by default. The Open dialog also lists `.sqlite` and
`.db` files.

Ticks are kept in memory while you work, and the file is only written when
you choose Save or Save As.

## Export

Export writes to the first worksheet of the template:

| Cell      | Value                                         |
|-----------|-----------------------------------------------|
| `D2`      | Employee number                               |
| `D3`      | Employee name                                 |

Then two rows per ticked day, earliest first, starting at row 10: the trip
to the customer, then the trip back.

| Column | Row 10, 12, …            | Row 11, 13, …              |
|--------|--------------------------|----------------------------|
| `C`    | Route to customer        | Route from customer        |
| `D`    | Description to customer  | Description from customer  |
| `E`    | KM to customer           | KM from customer           |
| `H`    | Date (`dd.MM.yyyy`)      | Date (`dd.MM.yyyy`)        |
| `L`    | Toll amount              | Toll amount                |

Empty text settings and numbers that are 0 are left out, so the template's
own content stays in those cells. The trip table ends at row 75, which is
room for 33 days. Cells keep the template's formatting, the macros and
everything else in the workbook are copied unchanged, and Excel recalculates
the formulas (sums etc.) when the file is opened.

## Settings

The path of the last opened file and the values from the Settings dialog are
stored with `QSettings`. On Linux this is
`~/.config/TickCalendar/TickCalendar.conf` (or under `$XDG_CONFIG_HOME` if
set):

```ini
[General]
lastFile=file:///path/to/calendar.tick

[trip]
templateFile=/path/to/template.xlsx
employeeName=Kari Nordmann
employeeNumber=1234
routeToCustomer=Oslo
descriptionToCustomer=Home
kmToCustomer=0
routeFromCustomer=Drammen
descriptionFromCustomer=Office
kmFromCustomer=42
tollAmount=57.5
```

Empty KM and toll fields are stored as `0`.

If that file no longer exists when the app starts, the app opens an empty
calendar and forgets the path.

## Source overview

| File            | Purpose                                                          |
|-----------------|------------------------------------------------------------------|
| `main.cpp`      | Starts the app and loads the QML module.                         |
| `Main.qml`      | Window, menus, month navigation, calendar grid and dialogs.      |
| `SettingsDialog.qml` | The Settings dialog and the stored trip settings.           |
| `TickStore.h`   | `TickStore`, the QML type holding the ticked days.               |
| `TickStore.cpp` | Loading and saving SQLite files, clipboard copy, export, last-file setting. |
| `XlsxExport.h`, `XlsxExport.cpp` | Filling in cells of an Excel workbook.             |
