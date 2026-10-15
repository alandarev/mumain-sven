cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SOURCE_DIR OR NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "SOURCE_DIR must name an existing directory")
endif()
if(NOT DEFINED PATCH_FILE OR NOT EXISTS "${PATCH_FILE}")
    message(FATAL_ERROR "PATCH_FILE must name an existing file")
endif()

file(REAL_PATH "${SOURCE_DIR}" SOURCE_DIR)
get_filename_component(SOURCE_PARENT "${SOURCE_DIR}" DIRECTORY)
get_filename_component(PATCH_FILE "${PATCH_FILE}" ABSOLUTE)

find_package(Git REQUIRED)
# Archives below a parent worktree must not silently skip all paths using that repo's prefix.
# An actual checkout's own .git remains discoverable below this ceiling.
set(git_command "${CMAKE_COMMAND}" -E env --unset=GIT_DIR --unset=GIT_WORK_TREE
    "GIT_CEILING_DIRECTORIES=${SOURCE_PARENT}" "${GIT_EXECUTABLE}")

# Addition-only zero-context patches can forward-apply twice. Recognize the applied state first.
execute_process(
    COMMAND ${git_command} apply --unidiff-zero --reverse --check "${PATCH_FILE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE reverse_result
    OUTPUT_VARIABLE reverse_output
    ERROR_VARIABLE reverse_error
)
if(reverse_result EQUAL 0)
    return()
endif()

execute_process(
    COMMAND ${git_command} apply --unidiff-zero --check "${PATCH_FILE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE forward_result
    OUTPUT_VARIABLE forward_output
    ERROR_VARIABLE forward_error
)
if(forward_result EQUAL 0)
    execute_process(
        COMMAND ${git_command} apply --unidiff-zero "${PATCH_FILE}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE apply_result
        OUTPUT_VARIABLE apply_output
        ERROR_VARIABLE apply_error
    )
    if(NOT apply_result EQUAL 0)
        message(FATAL_ERROR "git apply failed:\n${apply_output}${apply_error}")
    endif()
    return()
endif()


message(FATAL_ERROR
    "patch cannot be applied or recognized as already applied\n"
    "forward check:\n${forward_output}${forward_error}\n"
    "reverse check:\n${reverse_output}${reverse_error}")
