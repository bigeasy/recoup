project(cjson)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
cmake_minimum_required (VERSION 3.19)
include(../common.cmake)
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set_target_properties(storage PROPERTIES COMPILE_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage -Wall -pedantic -Wextra")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage ")
    include(CodeCoverage)
    setup_target_for_coverage(
        NAME coverage
        EXECUTABLE build/json_test)
endif(CMAKE_BUILD_TYPE MATCHES Debug)
