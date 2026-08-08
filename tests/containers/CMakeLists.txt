# Container tests

add_executable(test_slist
  test_slist.cpp
)

target_link_libraries(test_slist
  PRIVATE
    cpp_core_systems::cpp_core_systems
    GTest::gtest_main
)

# Auto-register every TEST() in the binary as an individual CTest test.
# The "containers" label lets `ctest -L containers` run just this group.
gtest_discover_tests(test_slist
  PROPERTIES LABELS "containers"
)
