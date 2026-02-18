# Build all example executables in this folder

add_executable(cpp_core_systems_example_basic
  time/scope_timer_example.cpp
)

target_link_libraries(cpp_core_systems_example_basic
  PRIVATE cpp_core_systems::cpp_core_systems
)
