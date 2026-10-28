
project(datasource LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    pe_data_source.cpp
    pe_data_source.h
    pe_file_data_source.cpp
    pe_file_data_source.h
)

target_link_libraries(${PROJECT_NAME} PUBLIC Qt::Core)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
