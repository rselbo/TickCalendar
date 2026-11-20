cmake_minimum_required(VERSION 4.2)
project(TickCalendar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Sql)

qt_standard_project_setup(REQUIRES 6.5)

# Keep QML module dirs from clashing with the executable name
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml)

qt_add_executable(TickCalendar main.cpp)

qt_add_qml_module(TickCalendar
        URI TickCalendar
        VERSION 1.0
        SOURCES
            TickStore.h
            TickStore.cpp
        QML_FILES
            Main.qml
)

target_link_libraries(TickCalendar PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Qml
        Qt::Quick
        Qt::Sql
)
