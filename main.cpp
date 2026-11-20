#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
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

    return QGuiApplication::exec();
}
