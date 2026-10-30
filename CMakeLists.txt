cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0091 NEW)

project(desktopctl VERSION 0.9.0 LANGUAGES CXX)

option(DESKTOPCTL_BUILD_TESTS "Build desktopctl smoke tests" ON)

add_executable(desktopctl)

target_sources(desktopctl
    PRIVATE
        src/main.cpp
        src/activityIndicator.cpp
        src/applicationCatalog.cpp
        src/commandLineApplication.cpp
        src/desktopAutomation.cpp
        src/mcpServer.cpp
        src/screenCapture.cpp
        src/activityIndicator.h
        src/applicationCatalog.h
        src/operationResult.h
        src/mcpServer.h
        src/screenCapture.h
        src/private/commandLineApplicationPrivate.cpp
        src/private/commandLineApplicationPrivate.h
        src/private/jsonCodec.cpp
        src/private/jsonCodec.h
        src/private/jsonValue.cpp
        src/private/jsonValue.h
        src/private/mcpServerPrivate.cpp
        src/private/mcpServerPrivate.h
        src/private/mcpToolDispatcher.cpp
        src/private/mcpToolDispatcher.h
        src/private/activityIndicatorPrivate.h
        src/private/applicationCatalogPrivate.h
        src/private/desktopAutomationPrivate.h
        src/private/screenCapturePrivate.h
    PUBLIC
        src/commandLineApplication.h
        src/desktopAutomation.h
)

target_include_directories(desktopctl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(desktopctl PRIVATE cxx_std_17)
target_compile_definitions(desktopctl PRIVATE DESKTOPCTL_VERSION="${PROJECT_VERSION}")

if(WIN32)
    target_sources(desktopctl PRIVATE
        src/platform/windows/activityIndicatorWindows.cpp
        src/platform/windows/applicationCatalogWindows.cpp
        src/platform/windows/desktopAutomationWindows.cpp
        src/platform/windows/screenCaptureWindows.cpp
        src/platform/windows/windowsText.cpp
        src/platform/windows/windowsText.h
    )
    target_compile_definitions(desktopctl PRIVATE
        NOMINMAX
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
    )
    target_link_libraries(desktopctl PRIVATE dwmapi gdi32 gdiplus ole32 propsys shell32 user32)
elseif(UNIX AND NOT APPLE)
    target_sources(desktopctl PRIVATE
        src/platform/linux/activityIndicatorLinux.cpp
        src/platform/linux/applicationCatalogLinux.cpp
        src/platform/linux/desktopAutomationLinux.cpp
        src/platform/linux/screenCaptureLinux.cpp
    )
else()
    message(FATAL_ERROR "desktopctl currently supports only Windows and Linux targets")
endif()

if(MSVC)
    set_property(TARGET desktopctl PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_compile_options(desktopctl PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(desktopctl PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DESKTOPCTL_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_test(NAME desktopctl_help COMMAND desktopctl --help)
        add_test(NAME desktopctl_help_uppercase COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_window_activate COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_screenshot_json COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_jpeg COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_jpeg_policy COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_session_gate COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_idle_timeout COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_per_monitor COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_mcp COMMAND desktopctl -H)
        add_test(
            NAME desktopctl_mcp_lifecycle
            COMMAND ${CMAKE_COMMAND}
                "-DDESKTOPCTL_EXE=$<TARGET_FILE:desktopctl>"
                "-DDESKTOPCTL_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/test"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/test/mcpLifecycle.cmake"
        )
        if(WIN32)
            add_test(
                NAME desktopctl_session_lifecycle
                COMMAND ${CMAKE_COMMAND}
                    "-DDESKTOPCTL_EXE=$<TARGET_FILE:desktopctl>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/test/sessionLifecycle.cmake"
            )
        endif()
        set_tests_properties(desktopctl_help PROPERTIES
            PASS_REGULAR_EXPRESSION "Browser workflow"
        )
        set_tests_properties(desktopctl_help_uppercase PROPERTIES
            PASS_REGULAR_EXPRESSION "Examples:"
        )
        set_tests_properties(desktopctl_help_window_activate PROPERTIES
            PASS_REGULAR_EXPRESSION "window activate"
        )
        set_tests_properties(desktopctl_help_screenshot_json PROPERTIES
            PASS_REGULAR_EXPRESSION "cleanupRecommended"
        )
        set_tests_properties(desktopctl_help_jpeg PROPERTIES
            PASS_REGULAR_EXPRESSION "JPEG quality"
        )
        set_tests_properties(desktopctl_help_jpeg_policy PROPERTIES
            PASS_REGULAR_EXPRESSION "Use JPG for all routine screenshots"
        )
        set_tests_properties(desktopctl_help_session_gate PROPERTIES
            PASS_REGULAR_EXPRESSION "Mandatory session gate"
        )
        set_tests_properties(desktopctl_help_idle_timeout PROPERTIES
            PASS_REGULAR_EXPRESSION "three minutes without a non-help command"
        )
        set_tests_properties(desktopctl_help_per_monitor PROPERTIES
            PASS_REGULAR_EXPRESSION "Each active display receives its own complete border"
        )
        set_tests_properties(desktopctl_help_mcp PROPERTIES
            PASS_REGULAR_EXPRESSION "desktopctl mcp serve"
        )
    endif()
endif()

install(TARGETS desktopctl RUNTIME DESTINATION bin)
