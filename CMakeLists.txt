# CMakeList.txt : CMake project for cpp_core_systems, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("cpp_core_systems")

# ----------------------------
# Options
# ----------------------------
option(CPP_CORE_SYSTEMS_BUILD_EXAMPLES   "Build examples"   ON)
option(CPP_CORE_SYSTEMS_BUILD_TESTS      "Build tests"      OFF)
option(CPP_CORE_SYSTEMS_BUILD_BENCHMARKS "Build benchmarks" OFF)

# ----------------------------
# Library target: cpp_core_systems
# ----------------------------
# Visual Studio template created an executable from cpp_core_systems.cpp/cpp_core_systems.h.
# Convert to a library with sources under src/ and public headers under core/.
file(GLOB_RECURSE CPP_CORE_SYSTEMS_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)

if (CPP_CORE_SYSTEMS_SOURCES STREQUAL "")
  message(FATAL_ERROR
    "No source files found under 'src/'.\n"
    "Move your implementation files into src/ (e.g., src/cpp_core_systems.cpp), or add at least one .cpp there."
  )
endif()

add_library(cpp_core_systems ${CPP_CORE_SYSTEMS_SOURCES})
add_library(cpp_core_systems::cpp_core_systems ALIAS cpp_core_systems)

target_include_directories(cpp_core_systems
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
    $<INSTALL_INTERFACE:core>
)

target_compile_features(cpp_core_systems PUBLIC cxx_std_20)

set_target_properties(cpp_core_systems PROPERTIES
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# ----------------------------
# Subdirectories
# ----------------------------
if (CPP_CORE_SYSTEMS_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if (CPP_CORE_SYSTEMS_BUILD_TESTS)
  include(CTest) # defines BUILD_TESTING option
  if (BUILD_TESTING)
    add_subdirectory(tests)
  endif()
endif()

if (CPP_CORE_SYSTEMS_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

# TODO: Add install/export targets if needed.
