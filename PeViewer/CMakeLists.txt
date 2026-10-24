cmake_minimum_required(VERSION 3.16)
project(PeViewer LANGUAGES CXX)


set(PROJECT_SOURCES
    main.cpp
    PeViewer.ui
    PeViewer.h
    PeViewer.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        PeParse
        PeProjectManager
        PeDataSource
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

    # Keep the prefix unevaluated until installation to support --prefix.
    install(CODE "
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                $<IF:$<CONFIG:Debug>,--debug,--release>
                \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/$<TARGET_FILE_NAME:${PROJECT_NAME}>\"
            RESULT_VARIABLE deploy_result
        )
        if(NOT \"\${deploy_result}\" STREQUAL \"0\")
            message(FATAL_ERROR \"windeployqt failed: \${deploy_result}\")
        endif()
    ")
endif()

