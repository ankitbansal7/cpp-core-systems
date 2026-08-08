# Tests for cpp_core_systems
#
# GoogleTest is pulled in via FetchContent so contributors don't need a
# system-wide install. CTest/enable_testing() is already set up by the
# top-level CMakeLists (via include(CTest) inside the BUILD_TESTS block).

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)

# On Windows, force GoogleTest to use the shared (DLL) C runtime so it matches
# the rest of the build and avoids CRT-mismatch link errors.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Provides gtest_discover_tests(), which registers each TEST() with CTest.
include(GoogleTest)

add_subdirectory(containers)
