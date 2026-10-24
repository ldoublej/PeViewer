
project(PeDataSource LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    PeDataSource.cpp
    PeDataSource.h
)

target_link_libraries(${PROJECT_NAME} PUBLIC Qt::Core)
