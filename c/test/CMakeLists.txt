add_executable (json_test json_test.c ok.c)
target_link_libraries(json_test json)
include_directories(..)
add_test(json_test json_test)
