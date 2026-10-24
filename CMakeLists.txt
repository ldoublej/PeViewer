cmake_minimum_required(VERSION 3.16)
project("PeViewer")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 在检测编译器之前，统一控制台输出编码。解决VS2026编译输出大量包含文件信息
if(CMAKE_HOST_WIN32 AND CMAKE_GENERATOR MATCHES "Ninja")
    execute_process(COMMAND cmd /c chcp 65001 OUTPUT_QUIET)
endif()

# 警告视为错误
if(MSVC)
    add_compile_options(/utf-8 /WX)
else()
    add_compile_options(-Werror)
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
