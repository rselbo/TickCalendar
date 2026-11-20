cmake_minimum_required(VERSION 4.2)
project(TickCalendar)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        REQUIRED)

add_executable(TickCalendar main.cpp)
target_link_libraries(TickCalendar
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

