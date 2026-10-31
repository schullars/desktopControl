if(NOT DEFINED DESKTOPCTL_EXE OR NOT EXISTS "${DESKTOPCTL_EXE}")
    message(FATAL_ERROR "DESKTOPCTL_EXE is missing or invalid")
endif()
if(NOT DEFINED DESKTOPCTL_TEST_DIR)
    message(FATAL_ERROR "DESKTOPCTL_TEST_DIR is missing")
endif()

set(desktopctlCommand)
if(DEFINED DESKTOPCTL_EMULATOR AND NOT DESKTOPCTL_EMULATOR STREQUAL "")
    list(APPEND desktopctlCommand ${DESKTOPCTL_EMULATOR})
endif()
list(APPEND desktopctlCommand "${DESKTOPCTL_EXE}")

# The session is a per-login singleton, so a previous manual run must not affect this test.
execute_process(
    COMMAND ${desktopctlCommand} session stop
    OUTPUT_QUIET
    ERROR_QUIET
)

file(MAKE_DIRECTORY "${DESKTOPCTL_TEST_DIR}")
set(requestFile "${DESKTOPCTL_TEST_DIR}/mcpLifecycle.jsonl")
file(WRITE "${requestFile}"
    "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"server/discover\",\"params\":{}}\n"
    "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-06-18\",\"capabilities\":{},\"clientInfo\":{\"name\":\"desktopctl-test\",\"version\":\"1.0.0\"}}}\n"
    "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\",\"params\":{}}\n"
    "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\",\"params\":{}}\n"
    "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"window_list\",\"arguments\":{}}}\n"
    "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"session_start\",\"arguments\":{\"keep_alive\":\"yes\"}}}\n"
    "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"session_start\",\"arguments\":{\"keep_alive\":false}}}\n"
    "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{\"name\":\"session_stop\",\"arguments\":{}}}\n"
    "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}\n"
)

execute_process(
    COMMAND ${desktopctlCommand} mcp serve
    INPUT_FILE "${requestFile}"
    RESULT_VARIABLE serverResult
    OUTPUT_VARIABLE serverOutput
    ERROR_VARIABLE serverError
    TIMEOUT 30
)
if(NOT serverResult EQUAL 0)
    message(FATAL_ERROR "MCP lifecycle failed: ${serverError}")
endif()
if(NOT serverOutput MATCHES "desktopctl")
    message(FATAL_ERROR "MCP initialize response is missing server information: ${serverOutput}")
endif()
if(NOT serverOutput MATCHES "2026-07-28")
    message(FATAL_ERROR "MCP 2.0 discovery response is missing: ${serverOutput}")
endif()
if(NOT serverOutput MATCHES "screenshot_capture")
    message(FATAL_ERROR "MCP tool list is incomplete: ${serverOutput}")
endif()
if(NOT serverOutput MATCHES "type_physical" OR NOT serverOutput MATCHES "type_unicode")
    message(FATAL_ERROR "MCP keyboard text modes are missing: ${serverOutput}")
endif()
if(NOT serverOutput MATCHES "Session is not started")
    message(FATAL_ERROR "MCP session gate did not reject a pre-session tool call: ${serverOutput}")
endif()
if(NOT serverOutput MATCHES "session_start")
    message(FATAL_ERROR "MCP session start result is missing: ${serverOutput}")
endif()
if(NOT serverOutput MATCHES "keep_alive")
    message(FATAL_ERROR "MCP keep-alive session option is missing: ${serverOutput}")
endif()
if(NOT serverOutput MATCHES "Argument must be a boolean: keep_alive")
    message(FATAL_ERROR "MCP keep-alive validation result is missing: ${serverOutput}")
endif()
