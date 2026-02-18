# Benchmarks for cpp_core_systems

add_executable(cpp_core_systems_bench_basic
  bench_basic.cpp
)

target_link_libraries(cpp_core_systems_bench_basic
  PRIVATE cpp_core_systems::cpp_core_systems
)
