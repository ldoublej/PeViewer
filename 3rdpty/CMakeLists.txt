set(SARIBBON_BUILD_STATIC_LIBS ON CACHE BOOL "Build SARibbon as a static library" FORCE)
set(SARIBBON_USE_FRAMELESS_LIB ON CACHE BOOL "Enable QWindowKit frameless support" FORCE)
set(SARIBBON_BUILD_EXAMPLES OFF CACHE BOOL "Disable SARibbon examples" FORCE)
set(SARIBBON_INSTALL_IN_CURRENT_DIR OFF CACHE BOOL "Use the project install prefix" FORCE)
set(BUILD_TESTS OFF)

set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "Build QWindowKit as static libraries" FORCE)

add_subdirectory(SARibbon)

# SARibbon otherwise silently falls back to its built-in frameless helper.
if(NOT TARGET QWindowKit::Widgets)
    message(FATAL_ERROR "QWindowKit is required. Run git submodule update --init --recursive.")
endif()
