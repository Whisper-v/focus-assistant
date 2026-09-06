cmake_minimum_required(VERSION 3.16)
project(focus-assistant VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)
find_package(Dtk6Core REQUIRED)
find_package(Dtk6Gui REQUIRED)
find_package(Dtk6Widget REQUIRED)

add_executable(focus-assistant
    src/main.cpp
    src/systemlinker.cpp
    src/focusmanager.cpp
    src/petwidget.cpp
    src/mainwindow.cpp
    src/statsdialog.cpp
    resources/resources.qrc
)

target_include_directories(focus-assistant PRIVATE src)

target_link_libraries(focus-assistant PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

install(TARGETS focus-assistant RUNTIME DESTINATION bin)
install(FILES data/focus-assistant.desktop DESTINATION share/applications)
install(FILES resources/focus-assistant.svg DESTINATION share/icons/hicolor/scalable/apps)

# make uninstall
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

# simple .deb generation
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "focus-assistant")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Focus Assistant Developer <dev@example.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libdtk6core, libdtk6gui, libdtk6widget, libqt6core6, libqt6gui6, libqt6widgets6, libqt6dbus6")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Focus Assistant - a cute pomodoro companion living on your desktop")
set(CPACK_PACKAGE_FILE_NAME "focus-assistant-${PROJECT_VERSION}-amd64")
include(CPack)
