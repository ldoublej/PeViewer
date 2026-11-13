cmake_minimum_required(VERSION 3.19)
project("peviewer")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_HOST_WIN32)
    execute_process(
        COMMAND cmd.exe /c "chcp 65001 >nul"
    )
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    LinguistTools
)
add_subdirectory("3rdpty")

# 警告视为错误（仅用于项目自身代码）
if(MSVC)
    add_compile_options(/utf-8 /WX /EHsc)
else()
    add_compile_options(-Werror)
endif()

add_subdirectory("src")

if(BUILD_TESTING)
    include(CTest)
    add_subdirectory("tests")
endif()
