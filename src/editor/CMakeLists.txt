project(editor LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    editor.cpp
    editor.h
    editor_view.h
    editor_view_registry.cpp
    editor_view_registry.h
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Widgets
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
