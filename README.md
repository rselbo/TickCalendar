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
- **File** menu: New, Open, Save, Save As and Quit. If there are unsaved
  changes, you are asked whether to save them before New, Open or closing
  the window. Unsaved changes are shown with a `*` in the window title.
- **Edit → Copy** (Ctrl+C) copies the ticked days of the month on screen to
  the clipboard, earliest first. Each date is in `dd.MM.yyyy` format and
  appears twice, one per line:

  ```
  03.11.2026
  03.11.2026
  20.11.2026
  20.11.2026
  ```

- The file that was open when the app closed is reopened on the next start.

## Building

Requirements:

- CMake 4.2 or newer
- A C++23 compiler
- Qt 6.5 or newer with the Qt Quick, Qt Quick Controls, Qt Quick Dialogs and
  Qt Sql modules, and the Qt SQLite driver (`QSQLITE`)

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

## Settings

The path of the last opened file is stored with `QSettings`. On Linux this is
`~/.config/TickCalendar/TickCalendar.conf` (or under `$XDG_CONFIG_HOME` if
set):

```ini
[General]
lastFile=file:///path/to/calendar.tick
```

If that file no longer exists when the app starts, the app opens an empty
calendar and forgets the path.

## Source overview

| File            | Purpose                                                          |
|-----------------|------------------------------------------------------------------|
| `main.cpp`      | Starts the app and loads the QML module.                         |
| `Main.qml`      | Window, menus, month navigation, calendar grid and dialogs.      |
| `TickStore.h`   | `TickStore`, the QML type holding the ticked days.               |
| `TickStore.cpp` | Loading and saving SQLite files, clipboard copy, last-file setting. |
