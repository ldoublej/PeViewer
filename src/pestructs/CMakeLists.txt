project(pestructs LANGUAGES CXX)

add_library(pestructs INTERFACE)
target_compile_features(pestructs INTERFACE cxx_std_17)

target_include_directories(pestructs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
