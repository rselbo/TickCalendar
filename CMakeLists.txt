cmake_minimum_required(VERSION 4.2)
project(TickCalendar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build an optimized release version unless another build type is chosen.
get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

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

# Warnings for all builds, treated as errors. Pass
# --compile-no-warning-as-error to cmake to build anyway.
set_target_properties(TickCalendar PROPERTIES COMPILE_WARNING_AS_ERROR ON)
if(MSVC)
    target_compile_options(TickCalendar PRIVATE /W4 /permissive-)
else()
    target_compile_options(TickCalendar PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

# Release builds: link-time optimization, and no qDebug() output.
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES CXX)
if(ipo_supported)
    set_target_properties(TickCalendar PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
            INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
else()
    message(STATUS "Link-time optimization not supported: ${ipo_error}")
endif()
target_compile_definitions(TickCalendar PRIVATE
        $<$<CONFIG:Release,MinSizeRel>:QT_NO_DEBUG_OUTPUT>)

qt_add_resources(TickCalendar "icon"
        PREFIX /
        FILES icon/check.svg
)

# The QML module and the window icon are compiled into the executable.
include(GNUInstallDirs)
install(TARGETS TickCalendar
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES icon/check.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME tickcalendar.svg)

# Desktop entry pointing at the installed executable and icon. The paths are
# filled in at install time, so they follow --prefix.
install(CODE "
    set(TICKCALENDAR_BIN \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/TickCalendar\")
    set(TICKCALENDAR_ICON \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps/tickcalendar.svg\")
    configure_file(\"${CMAKE_CURRENT_SOURCE_DIR}/TickCalendar.desktop.in\"
            \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/applications/TickCalendar.desktop\"
            @ONLY)
    message(STATUS \"Installing: \${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/applications/TickCalendar.desktop\")
")
