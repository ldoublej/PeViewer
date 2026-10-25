project(PeStructs LANGUAGES CXX)

add_library(PeStructs INTERFACE)
target_compile_features(PeStructs INTERFACE cxx_std_17)

target_include_directories(PeStructs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
