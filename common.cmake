project(storage)
add_library(storage STATIC ../start.c)
add_executable(start_test ../start_test.c ../ok.c)
target_link_libraries(start_test storage)
