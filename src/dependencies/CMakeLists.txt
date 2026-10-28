project(dependencies LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    api_set.cpp
    api_set.h
    file_module_searcher.cpp
    file_module_searcher.h
    module_searcher.h
    dependencies_tree.cpp
    dependencies_tree.h
)

target_link_libraries(${PROJECT_NAME} PUBLIC parse)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
