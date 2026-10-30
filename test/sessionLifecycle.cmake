if(NOT DEFINED DESKTOPCTL_EXE OR NOT EXISTS "${DESKTOPCTL_EXE}")
    message(FATAL_ERROR "DESKTOPCTL_EXE is missing or invalid")
endif()

execute_process(
    COMMAND "${DESKTOPCTL_EXE}" session stop
    OUTPUT_QUIET
    ERROR_QUIET
)

execute_process(
    COMMAND "${DESKTOPCTL_EXE}" --version
    RESULT_VARIABLE gateResult
    OUTPUT_VARIABLE gateOutput
    ERROR_VARIABLE gateError
)
if(gateResult EQUAL 0)
    message(FATAL_ERROR "Version command succeeded without a started session")
endif()
string(FIND "${gateError}" "Session is not started." gateMessagePosition)
if(gateMessagePosition EQUAL -1)
    message(FATAL_ERROR "Missing mandatory session message: ${gateError}")
endif()

execute_process(
    COMMAND "${DESKTOPCTL_EXE}" session start
    RESULT_VARIABLE startResult
    OUTPUT_VARIABLE startOutput
    ERROR_VARIABLE startError
)
if(NOT startResult EQUAL 0)
    message(FATAL_ERROR "Session start failed: ${startError}")
endif()

execute_process(
    COMMAND "${DESKTOPCTL_EXE}" session start
    RESULT_VARIABLE repeatedStartResult
    OUTPUT_VARIABLE repeatedStartOutput
    ERROR_VARIABLE repeatedStartError
)
if(NOT repeatedStartResult EQUAL 0)
    execute_process(COMMAND "${DESKTOPCTL_EXE}" session stop OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Repeated session start failed: ${repeatedStartError}")
endif()

execute_process(
    COMMAND "${DESKTOPCTL_EXE}" --version
    RESULT_VARIABLE versionResult
    OUTPUT_VARIABLE versionOutput
    ERROR_VARIABLE versionError
)
if(NOT versionResult EQUAL 0 OR NOT versionOutput MATCHES "desktopctl [0-9]+\\.[0-9]+\\.[0-9]+")
    execute_process(COMMAND "${DESKTOPCTL_EXE}" session stop OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Version command failed inside the session: ${versionError}")
endif()

execute_process(
    COMMAND "${DESKTOPCTL_EXE}" app list
    RESULT_VARIABLE appListResult
    OUTPUT_VARIABLE appListOutput
    ERROR_VARIABLE appListError
)
if(NOT appListResult EQUAL 0 OR NOT appListOutput MATCHES "applications")
    execute_process(COMMAND "${DESKTOPCTL_EXE}" session stop OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Application list failed inside the session: ${appListError}")
endif()

execute_process(
    COMMAND "${DESKTOPCTL_EXE}" session stop
    RESULT_VARIABLE stopResult
    OUTPUT_VARIABLE stopOutput
    ERROR_VARIABLE stopError
)
if(NOT stopResult EQUAL 0)
    message(FATAL_ERROR "Session stop failed: ${stopError}")
endif()

execute_process(
    COMMAND "${DESKTOPCTL_EXE}" --version
    RESULT_VARIABLE finalGateResult
    OUTPUT_VARIABLE finalGateOutput
    ERROR_VARIABLE finalGateError
)
if(finalGateResult EQUAL 0)
    message(FATAL_ERROR "Session state remained active after stop")
endif()
