add_executable(PeDataSourceTests PeDataSourceTests.cpp)
target_link_libraries(PeDataSourceTests PRIVATE PeDataSource)

add_test(NAME PeDataSourceTests COMMAND PeDataSourceTests)

add_executable(PeParseTests PeParseTests.cpp)
target_link_libraries(PeParseTests PRIVATE PeParse)
add_test(NAME PeParseTests COMMAND PeParseTests)

add_executable(PeViewerUiTests
    PeViewerUiTests.cpp
    ../PeViewer/PeViewer.cpp
    ../PeViewer/PeViewer.h
    ../PeViewer/PeViewer.ui
    ../PeViewer/LanguageManager.cpp
    ../PeViewer/LanguageManager.h
    "${PROJECT_BINARY_DIR}/PeViewer/translations.qrc"
)
set_target_properties(PeViewerUiTests PROPERTIES AUTORCC ON)
target_include_directories(PeViewerUiTests PRIVATE "${PROJECT_SOURCE_DIR}/PeViewer")
target_link_libraries(PeViewerUiTests PRIVATE Qt::Widgets PeParse)
add_dependencies(PeViewerUiTests PeViewer_translations)
add_test(NAME PeViewerUiTests COMMAND PeViewerUiTests)
set_tests_properties(PeViewerUiTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
