# Build all example executables in this folder

add_executable(example_scope_timer
  "time/example_scope_timer.cpp")

add_executable(example_slist
  "containers/example_slist.cpp")

target_link_libraries(example_scope_timer
  PRIVATE cpp_core_systems::cpp_core_systems
)

target_link_libraries(example_slist
  PRIVATE cpp_core_systems::cpp_core_systems
)
