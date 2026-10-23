# Benchmarks for cpp_core_systems

find_package(Threads REQUIRED)

add_executable(bench_tcp net/bench_tcp.cpp)
target_link_libraries(bench_tcp PRIVATE
  cpp_core_systems::cpp_core_systems
  Threads::Threads
)
