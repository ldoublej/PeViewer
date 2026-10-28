cmake_minimum_required(VERSION 3.16)
project(peviewer LANGUAGES CXX)


set(PROJECT_SOURCES
    main.cpp
    pe_viewer.ui
    pe_viewer.h
    pe_viewer.cpp
    language_manager.h
    language_manager.cpp
)

set(TS_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/pe_viewer_zh_cn.ts"
)
set(QM_FILES)
set(TRANSLATION_RESOURCE_ENTRIES)
foreach(TS_FILE IN LISTS TS_FILES)
    get_filename_component(TRANSLATION_NAME "${TS_FILE}" NAME_WE)
    set(QM_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TRANSLATION_NAME}.qm")
    add_custom_command(OUTPUT "${QM_FILE}"
        COMMAND Qt${QT_VERSION_MAJOR}::lrelease "${TS_FILE}" -nounfinished -qm "${QM_FILE}"
        DEPENDS "${TS_FILE}"
        VERBATIM
    )
    list(APPEND QM_FILES "${QM_FILE}")
    string(APPEND TRANSLATION_RESOURCE_ENTRIES
        "        <file alias=\"${TRANSLATION_NAME}.qm\">${QM_FILE}</file>\n")
endforeach()
configure_file(translations.qrc.in translations.qrc @ONLY)

add_custom_target(peviewer_translations DEPENDS ${QM_FILES})
add_custom_target(peviewer_lupdate
    COMMAND Qt${QT_VERSION_MAJOR}::lupdate ${PROJECT_SOURCES}
        -no-obsolete -locations relative -ts ${TS_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Updating PeViewer translation catalogs"
    VERBATIM
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES}
    "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    AUTORCC ON
)
add_dependencies(${PROJECT_NAME} peviewer_translations)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        parse
        workspace
        datasource
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

if(WIN32)
    # Use the deployment tool from the same Qt installation as the application.
    get_target_property(QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt
        HINTS "${QT_BIN_DIR}"
        NO_DEFAULT_PATH
    )
    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "windeployqt.exe was not found in ${QT_BIN_DIR}")
    endif()

    # Scan the build executable and deploy dependencies only during installation.
    # Resolve the install prefix at install time to support --prefix.
    install(CODE "
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                $<IF:$<CONFIG:Debug>,--debug,--release>
                --dir \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin\"
                \"$<TARGET_FILE:${PROJECT_NAME}>\"
            RESULT_VARIABLE deploy_result
        )
        if(NOT \"\${deploy_result}\" STREQUAL \"0\")
            message(FATAL_ERROR \"windeployqt failed: \${deploy_result}\")
        endif()
    ")
endif()

