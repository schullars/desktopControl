if(NOT DEFINED DESKTOPCTL_EXE OR NOT EXISTS "${DESKTOPCTL_EXE}")
    message(FATAL_ERROR "DESKTOPCTL_EXE is missing or invalid")
endif()
if(NOT DEFINED DESKTOPCTL_TEST_DIR)
    message(FATAL_ERROR "DESKTOPCTL_TEST_DIR is missing")
endif()

set(desktopctlCommand ${DESKTOPCTL_EMULATOR} "${DESKTOPCTL_EXE}")
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef nonce)
set(resultRoot "${DESKTOPCTL_TEST_DIR}/result-${nonce}")
file(MAKE_DIRECTORY "${resultRoot}")

execute_process(COMMAND ${desktopctlCommand} -h
    RESULT_VARIABLE baselineCode OUTPUT_VARIABLE baselineHelp ERROR_VARIABLE baselineError)
if(NOT baselineCode EQUAL 0 OR NOT baselineError STREQUAL "")
    message(FATAL_ERROR "Baseline help failed")
endif()

function(checkResult name expectedCode)
    set(path "${resultRoot}/${name}.json")
    execute_process(COMMAND ${desktopctlCommand} --result-file "${path}" ${ARGN}
        RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 10)
    if(NOT code EQUAL expectedCode OR NOT EXISTS "${path}")
        message(FATAL_ERROR "${name}: exit=${code}; stderr=${errors}; result missing or wrong code")
    endif()
    file(READ "${path}" payload)
    string(JSON schema GET "${payload}" schemaVersion)
    string(JSON resultCode GET "${payload}" exitCode)
    string(JSON resultOutput GET "${payload}" stdout)
    string(JSON resultErrors GET "${payload}" stderr)
    if(NOT schema EQUAL 1 OR NOT resultCode EQUAL code
        OR NOT resultOutput STREQUAL output OR NOT resultErrors STREQUAL errors)
        message(FATAL_ERROR "${name}: envelope does not match process output and exit code")
    endif()
    if(name STREQUAL "help" AND NOT output STREQUAL baselineHelp)
        message(FATAL_ERROR "Result-file help differs from ordinary help")
    endif()
endfunction()

checkResult(help 0 -h)
checkResult(missing-command 2)
checkResult(duplicate-option 2 --result-file ignored.json -h)
checkResult(reject-streaming 2 mcp serve)
checkResult(reject-host 2 session host)

execute_process(COMMAND ${desktopctlCommand} --result-file
    RESULT_VARIABLE missingPathCode OUTPUT_QUIET ERROR_VARIABLE missingPathError)
if(NOT missingPathCode EQUAL 2 OR NOT missingPathError MATCHES "requires a nonempty output path")
    message(FATAL_ERROR "Missing result-file path was not rejected")
endif()

# Validate a bad output path before any operation, even help, without touching a desktop session.
execute_process(COMMAND ${desktopctlCommand} --result-file "${resultRoot}/missing/out.json" -h
    RESULT_VARIABLE badParentCode OUTPUT_VARIABLE badParentOutput ERROR_VARIABLE badParentError)
if(NOT badParentCode EQUAL 3 OR NOT badParentOutput STREQUAL "")
    message(FATAL_ERROR "Unwritable result destination did not fail before executing help")
endif()

file(WRITE "${resultRoot}/existing.json" "preserve me")
execute_process(COMMAND ${desktopctlCommand} --result-file "${resultRoot}/existing.json" -h
    RESULT_VARIABLE existingCode OUTPUT_VARIABLE existingOutput ERROR_QUIET)
file(READ "${resultRoot}/existing.json" existingBytes)
if(NOT existingCode EQUAL 3 OR NOT existingOutput STREQUAL ""
    OR NOT existingBytes STREQUAL "preserve me")
    message(FATAL_ERROR "Existing destination was executed against or overwritten")
endif()

file(GLOB pendingFiles "${resultRoot}/*.pending-*")
if(pendingFiles)
    message(FATAL_ERROR "Orphan pending result files: ${pendingFiles}")
endif()
file(REMOVE_RECURSE "${resultRoot}")
