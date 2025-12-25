add_executable(test_basic test_basic.c)
target_link_libraries(test_basic PRIVATE unilog)
add_test(NAME test_basic COMMAND test_basic)

add_executable(test_buffer test_buffer.c)
target_link_libraries(test_buffer PRIVATE unilog)
add_test(NAME test_buffer COMMAND test_buffer)

add_executable(test_threadsafe test_threadsafe.c)
target_link_libraries(test_threadsafe PRIVATE unilog pthread)
add_test(NAME test_threadsafe COMMAND test_threadsafe)
