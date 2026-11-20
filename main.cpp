#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[]) {
    // QApplication (not QGuiApplication) so that platform themes such as
    // KDE Plasma can show their native file and message dialogs. Without it,
    // Qt Quick falls back to its own dialogs, which e.g. don't show the
    // suggested file name in the Export dialog.
    QApplication app(argc, argv);
    // Used by QSettings: on Linux settings go to
    // ~/.config/TickCalendar/TickCalendar.conf.
    QGuiApplication::setOrganizationName(QStringLiteral("TickCalendar"));
    QGuiApplication::setApplicationName(QStringLiteral("TickCalendar"));
    // Lets Wayland compositors match the window to TickCalendar.desktop.
    QGuiApplication::setDesktopFileName(QStringLiteral("TickCalendar"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icon/check.svg")));

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("TickCalendar", "Main");

    return QApplication::exec();
}
