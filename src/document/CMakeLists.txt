project(document LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    document.cpp
    document.h
    document_manager.cpp
    document_manager.h
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        parse
        datasource
        dependencies
        editor
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
