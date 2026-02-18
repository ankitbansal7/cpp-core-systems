# Tests for cpp_core_systems

add_executable(cpp_core_systems_tests
  test_basic.cpp
)

target_link_libraries(cpp_core_systems_tests
  PRIVATE cpp_core_systems::cpp_core_systems
)

# Register with CTest
add_test(
  NAME cpp_core_systems_tests
  COMMAND cpp_core_systems_tests
)
