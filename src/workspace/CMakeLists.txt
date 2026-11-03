project(workspace LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    workspace.cpp
    workspace.h
    workspace_manager.cpp
    workspace_manager.h
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        parse
        datasource
        dependencies
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
