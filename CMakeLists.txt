cmake_minimum_required(VERSION 3.5)

project(protocolShaper LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED Xml AxContainer)

add_executable(protocolShaper
    main.cpp
    xmltoh.cpp
    xmltoh.h
    htodocx.cpp
    htodocx.h
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_link_libraries(protocolShaper PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Xml Qt${QT_VERSION_MAJOR}::AxContainer)
