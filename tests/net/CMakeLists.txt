add_executable(test_net test_net.cpp)
target_link_libraries(test_net PRIVATE cpp_core_systems::cpp_core_systems GTest::gtest_main)
gtest_discover_tests(test_net PROPERTIES LABELS "net")
