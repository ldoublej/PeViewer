project(WorkspaceManager LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    WorkspaceManager.cpp
    WorkspaceManager.h
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        PeParse
)
