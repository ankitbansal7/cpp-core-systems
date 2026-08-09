# Build all example executables in this folder
#
# Every .cpp found under examples/ (at any depth) becomes its own executable
# named after the file stem, so adding an example -- or a whole new category
# folder -- needs no changes here. CONFIGURE_DEPENDS asks the generator to
# re-run the glob at build time and re-configure when the set of files
# changes, matching how the top-level CMakeLists collects sources under src/.
file(GLOB_RECURSE CPP_CORE_SYSTEMS_EXAMPLE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

foreach(example_source IN LISTS CPP_CORE_SYSTEMS_EXAMPLE_SOURCES)
  get_filename_component(example_name "${example_source}" NAME_WE)

  # Target names are global, so two folders can't both define example_foo.cpp.
  if (TARGET ${example_name})
    message(FATAL_ERROR
      "Duplicate example target '${example_name}' from ${example_source}.\n"
      "Example file names must be unique across all examples/ subfolders."
    )
  endif()

  add_executable(${example_name} "${example_source}")

  target_link_libraries(${example_name}
    PRIVATE cpp_core_systems::cpp_core_systems
  )
endforeach()
