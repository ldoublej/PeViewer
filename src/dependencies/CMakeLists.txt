project(dependencies LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    api_set.cpp
    api_set.h
    file_module_locator.cpp
    file_module_locator.h
    module_locator.h
    pe_file_store.cpp
    pe_file_store.h
    dependency_graph.cpp
    dependency_graph.h
)

target_link_libraries(${PROJECT_NAME} PUBLIC parse)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
