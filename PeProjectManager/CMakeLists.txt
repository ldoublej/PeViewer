project(PeProjectManager LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    PeProjectManager.cpp
    PeProjectManager.h
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        PeParse
)
