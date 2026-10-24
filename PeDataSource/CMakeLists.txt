
project(PeDataSource LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    PeDataSource.cpp
    PeDataSource.h
    PeFileDataSource.cpp
    PeFileDataSource.h
)

target_link_libraries(${PROJECT_NAME} PUBLIC Qt::Core)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
