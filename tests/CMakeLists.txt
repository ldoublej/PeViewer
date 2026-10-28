add_executable(PeDataSourceTests pe_data_source_tests.cpp)
target_link_libraries(PeDataSourceTests PRIVATE datasource)

add_test(NAME PeDataSourceTests COMMAND PeDataSourceTests)

add_executable(PeParseTests pe_parse_tests.cpp)
target_link_libraries(PeParseTests PRIVATE parse)
add_test(NAME PeParseTests COMMAND PeParseTests)

add_executable(PeViewerUiTests
    pe_viewer_ui_tests.cpp
    ../src/peviewer/pe_viewer.cpp
    ../src/peviewer/pe_viewer.h
    ../src/peviewer/pe_viewer.ui
    ../src/peviewer/language_manager.cpp
    ../src/peviewer/language_manager.h
    "${PROJECT_BINARY_DIR}/src/peviewer/translations.qrc"
)
set_target_properties(PeViewerUiTests PROPERTIES AUTORCC ON)
target_include_directories(PeViewerUiTests PRIVATE "${PROJECT_SOURCE_DIR}/src/peviewer")
target_link_libraries(PeViewerUiTests PRIVATE Qt::Widgets parse)
add_dependencies(PeViewerUiTests peviewer_translations)
add_test(NAME PeViewerUiTests COMMAND PeViewerUiTests)
set_tests_properties(PeViewerUiTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
