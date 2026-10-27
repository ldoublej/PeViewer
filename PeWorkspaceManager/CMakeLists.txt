project(PeWorkspaceManager LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    PeWorkspaceManager.cpp
    PeWorkspaceManager.h
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        PeParse
)
