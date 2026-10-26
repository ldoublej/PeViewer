project(PeParse LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    PeFile.cpp
    PeFile.h
    Headers.cpp
    Headers.h
    Sections.cpp
    Sections.h
    Imports.cpp
    Imports.h
    Exports.cpp
    Exports.h
    ParseError.h
    ParseError.cpp
    detail/ParseUtils.cpp
    detail/ParseUtils.h
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        PeDataSource
        PeStructs
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
