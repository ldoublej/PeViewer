project(workspace LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    workspace_manager.cpp
    workspace_manager.h
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        parse
)
