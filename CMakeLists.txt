cmake_minimum_required(VERSION 3.16)
project("PeViewer")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)


if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
)

add_subdirectory("PeDataSource")
add_subdirectory("PeParse")
add_subdirectory("PeProjectManager")
add_subdirectory("PeViewer")
