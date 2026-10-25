project(PeStructs LANGUAGES CXX)

add_library(PeStructs INTERFACE)

target_include_directories(PeStructs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})