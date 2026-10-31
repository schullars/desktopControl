cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0091 NEW)

project(desktopctl VERSION 0.10.0 LANGUAGES CXX)

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
        src/platform/windows/private/keyboardInputWindows.cpp
        src/platform/windows/private/keyboardInputWindows.h
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
    find_package(JPEG REQUIRED)
    find_package(PNG REQUIRED)
    find_package(PkgConfig REQUIRED)
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)
    pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0 gio-unix-2.0)
    pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
    if(NOT X11_Xext_FOUND)
        message(FATAL_ERROR "The X11 Xext development library is required")
    endif()
    target_sources(desktopctl PRIVATE
        src/platform/linux/activityIndicatorLinux.cpp
        src/platform/linux/applicationCatalogLinux.cpp
        src/platform/linux/desktopAutomationLinux.cpp
        src/platform/linux/screenCaptureLinux.cpp
        src/platform/linux/private/linuxActivityOverlay.cpp
        src/platform/linux/private/linuxActivityOverlay.h
        src/platform/linux/private/linuxImageCodec.cpp
        src/platform/linux/private/linuxImageCodec.h
        src/platform/linux/private/linuxPortal.cpp
        src/platform/linux/private/linuxPortal.h
        src/platform/linux/private/linuxPortalRemoteDesktop.cpp
        src/platform/linux/private/linuxPortalRemoteDesktop.h
        src/platform/linux/private/linuxPortalVariant.cpp
        src/platform/linux/private/linuxPortalVariant.h
        src/platform/linux/private/linuxPipeWireCapture.cpp
        src/platform/linux/private/linuxPipeWireCapture.h
        src/platform/linux/private/linuxSessionIpc.cpp
        src/platform/linux/private/linuxSessionIpc.h
    )
    target_compile_definitions(desktopctl PRIVATE _FILE_OFFSET_BITS=64)
    target_link_libraries(desktopctl PRIVATE
        JPEG::JPEG
        PNG::PNG
        PkgConfig::GIO
        PkgConfig::PIPEWIRE
        Threads::Threads
        X11::X11
        X11::Xext
        ${CMAKE_DL_LIBS}
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
        add_test(NAME desktopctl_help_temp_screenshot_directory COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_session_gate COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_idle_timeout COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_per_monitor COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_mcp COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_hotkey_expression COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_literal_text COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_caret_placement COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_physical_text COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_text_mode_selection COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_vm_text_workflow COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_guest_caps_lock COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_taskbar_z_order COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_ubuntu_platforms COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_wayland_limits COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_wayland_session_capture COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_keep_alive COMMAND desktopctl -H)
        add_test(NAME desktopctl_help_wayland_overlay_fallback COMMAND desktopctl -H)
        add_test(
            NAME desktopctl_mcp_lifecycle
            COMMAND ${CMAKE_COMMAND}
                "-DDESKTOPCTL_EXE=$<TARGET_FILE:desktopctl>"
                "-DDESKTOPCTL_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/test"
                "-DDESKTOPCTL_EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/test/mcpLifecycle.cmake"
        )
        if(WIN32)
            add_executable(desktopctl_keyboard_input_tests
                test/keyboardInputWindowsTests.cpp
                src/platform/windows/private/keyboardInputWindows.cpp
                src/platform/windows/private/keyboardInputWindows.h
            )
            add_executable(desktopctl_activity_indicator_z_order_tests
                test/activityIndicatorZOrderWindowsTests.cpp
            )
            target_include_directories(desktopctl_keyboard_input_tests
                PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
            )
            target_compile_features(desktopctl_keyboard_input_tests PRIVATE cxx_std_17)
            target_compile_features(desktopctl_activity_indicator_z_order_tests PRIVATE cxx_std_17)
            target_compile_definitions(desktopctl_keyboard_input_tests PRIVATE
                NOMINMAX
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                _WIN32_WINNT=0x0A00
            )
            target_link_libraries(desktopctl_keyboard_input_tests PRIVATE user32)
            target_compile_definitions(desktopctl_activity_indicator_z_order_tests PRIVATE
                NOMINMAX
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                _WIN32_WINNT=0x0A00
            )
            target_link_libraries(desktopctl_activity_indicator_z_order_tests PRIVATE user32)
            if(MSVC)
                set_property(TARGET desktopctl_keyboard_input_tests PROPERTY
                    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
                )
                target_compile_options(desktopctl_keyboard_input_tests
                    PRIVATE /W4 /permissive- /utf-8
                )
                set_property(TARGET desktopctl_activity_indicator_z_order_tests PROPERTY
                    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
                )
                target_compile_options(desktopctl_activity_indicator_z_order_tests
                    PRIVATE /W4 /permissive- /utf-8
                )
            endif()
            add_test(NAME desktopctl_keyboard_input COMMAND desktopctl_keyboard_input_tests)
            add_test(
                NAME desktopctl_activity_indicator_z_order
                COMMAND desktopctl_activity_indicator_z_order_tests $<TARGET_FILE:desktopctl>
            )
            set_tests_properties(desktopctl_activity_indicator_z_order PROPERTIES TIMEOUT 15)
            add_test(
                NAME desktopctl_session_lifecycle
                COMMAND ${CMAKE_COMMAND}
                    "-DDESKTOPCTL_EXE=$<TARGET_FILE:desktopctl>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/test/sessionLifecycle.cmake"
            )
        elseif(UNIX AND NOT APPLE)
            add_executable(desktopctl_linux_image_codec_tests
                test/linuxImageCodecTests.cpp
                src/platform/linux/private/linuxImageCodec.cpp
                src/platform/linux/private/linuxImageCodec.h
            )
            target_include_directories(desktopctl_linux_image_codec_tests
                PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
            )
            target_compile_features(desktopctl_linux_image_codec_tests PRIVATE cxx_std_17)
            target_compile_options(desktopctl_linux_image_codec_tests
                PRIVATE -Wall -Wextra -Wpedantic
            )
            target_link_libraries(desktopctl_linux_image_codec_tests
                PRIVATE JPEG::JPEG PNG::PNG
            )
            add_test(NAME desktopctl_linux_image_codec
                     COMMAND desktopctl_linux_image_codec_tests)
            add_executable(desktopctl_linux_portal_variant_tests
                test/linuxPortalVariantTests.cpp
                src/platform/linux/private/linuxPortalVariant.cpp
                src/platform/linux/private/linuxPortalVariant.h
            )
            target_include_directories(desktopctl_linux_portal_variant_tests
                PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
            )
            target_compile_features(desktopctl_linux_portal_variant_tests PRIVATE cxx_std_17)
            target_compile_options(desktopctl_linux_portal_variant_tests
                PRIVATE -Wall -Wextra -Wpedantic
            )
            target_link_libraries(desktopctl_linux_portal_variant_tests
                PRIVATE PkgConfig::GIO
            )
            add_test(NAME desktopctl_linux_portal_variant
                     COMMAND desktopctl_linux_portal_variant_tests)
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
        set_tests_properties(desktopctl_help_temp_screenshot_directory PROPERTIES
            PASS_REGULAR_EXPRESSION "system temporary"
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
        set_tests_properties(desktopctl_help_hotkey_expression PROPERTIES
            PASS_REGULAR_EXPRESSION "ctrl[+]shift[+]escape"
        )
        set_tests_properties(desktopctl_help_literal_text PROPERTIES
            PASS_REGULAR_EXPRESSION "ASCII text, symbols, and passwords"
        )
        set_tests_properties(desktopctl_help_caret_placement PROPERTIES
            PASS_REGULAR_EXPRESSION "Place the caret explicitly before typing"
        )
        set_tests_properties(desktopctl_help_physical_text PROPERTIES
            PASS_REGULAR_EXPRESSION "desktopctl keyboard type-physical TEXT"
        )
        set_tests_properties(desktopctl_help_text_mode_selection PROPERTIES
            PASS_REGULAR_EXPRESSION "Text input mode selection"
        )
        set_tests_properties(desktopctl_help_vm_text_workflow PROPERTIES
            PASS_REGULAR_EXPRESSION "Virtual-machine text workflow"
        )
        set_tests_properties(desktopctl_help_guest_caps_lock PROPERTIES
            PASS_REGULAR_EXPRESSION "ASCII letter case is inverted while symbols are correct"
        )
        set_tests_properties(desktopctl_help_taskbar_z_order PROPERTIES
            PASS_REGULAR_EXPRESSION "permanently cover a screen edge"
        )
        set_tests_properties(desktopctl_help_ubuntu_platforms PROPERTIES
            PASS_REGULAR_EXPRESSION "Ubuntu display platform compatibility"
        )
        set_tests_properties(desktopctl_help_wayland_limits PROPERTIES
            PASS_REGULAR_EXPRESSION "global window list, arbitrary"
        )
        set_tests_properties(desktopctl_help_wayland_session_capture PROPERTIES
            PASS_REGULAR_EXPRESSION "screenshots and absolute pointer movement use the same"
        )
        set_tests_properties(desktopctl_help_keep_alive PROPERTIES
            PASS_REGULAR_EXPRESSION "session start --keep-alive"
        )
        set_tests_properties(desktopctl_help_wayland_overlay_fallback PROPERTIES
            PASS_REGULAR_EXPRESSION "three-pixel fallback"
        )
    endif()
endif()

install(TARGETS desktopctl RUNTIME DESTINATION bin)
