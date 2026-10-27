cmake_minimum_required(VERSION 3.16)
project("PeViewer")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 警告视为错误
if(MSVC)
    add_compile_options(/utf-8 /WX /EHsc)
else()
    add_compile_options(-Werror)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    LinguistTools
)
add_subdirectory("PeStructs")
add_subdirectory("PeDataSource")
add_subdirectory("PeParse")
add_subdirectory("WorkspaceManager")
add_subdirectory("PeViewer")

if(BUILD_TESTING)
    include(CTest)
    add_subdirectory("Tests")
endif()
