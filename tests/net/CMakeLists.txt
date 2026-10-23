add_executable(test_net
  test_ip_address.cpp
  test_socket_endpoint.cpp
  test_socket_error.cpp
  test_socket_handle.cpp
  test_socket_runtime.cpp
)
target_link_libraries(test_net PRIVATE cpp_core_systems::cpp_core_systems GTest::gtest_main)
gtest_discover_tests(test_net PROPERTIES LABELS "net")
