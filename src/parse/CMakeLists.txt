project(parse LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    pe_file.cpp
    pe_file.h
    headers.cpp
    headers.h
    sections.cpp
    sections.h
    imports.cpp
    imports.h
    exports.cpp
    exports.h
    parse_error.h
    parse_error.cpp
    detail/parse_utils.cpp
    detail/parse_utils.h
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        datasource
        pestructs
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
