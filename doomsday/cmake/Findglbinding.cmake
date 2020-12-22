message (STATUS "Looking for glbinding...")

if (APPLE)
    # Homebrew's glbinding CMake config doesn't seem to work.
    include (${glbinding_DIR}/cmake/glbinding/glbinding-export.cmake)
else ()
    find_package (glbinding REQUIRED NO_CMAKE_PATH)
endif ()
