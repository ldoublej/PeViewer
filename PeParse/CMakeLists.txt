project(PeParse LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    PeParse.cpp
    PeParse.h
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        PeDataSource
)
