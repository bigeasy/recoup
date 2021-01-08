add_executable (recoup_test recoup_test.c ok.c)
target_link_libraries(recoup_test recoup)
include_directories(..)
add_test(recoup_test recoup_test)
