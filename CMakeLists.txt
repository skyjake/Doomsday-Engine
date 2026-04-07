# Doomsday Engine
# Top-level CMakeLists

cmake_minimum_required (VERSION 3.10)
project (Doomsday)

# Build external dependencies (the_Foundation, Assimp, glbinding) via
# build_deps.py before the rest of the configuration begins.
#
# DE_BUILD_DEPS        ON by default; set OFF to skip (if deps are already built)
# DE_BUILD_DEPS_ARGS   Extra arguments forwarded to build_deps.py (e.g. "--skip-assimp")
# DE_BUILD_DEPS_TYPE   CMake build type for deps (default: Release)
# PYTHON_DIR           Hint path for finding the Python 3 interpreter

option (DE_BUILD_DEPS "Build external dependencies via scripts/build_deps.py" ON)
set (DE_BUILD_DEPS_ARGS "" CACHE STRING "Extra arguments for build_deps.py")
set (DE_BUILD_DEPS_TYPE "Release" CACHE STRING "Build type for external dependencies")

if (DE_BUILD_DEPS)
    find_program (_python python3 HINTS "${PYTHON_DIR}")
    if (NOT _python)
        find_program (_python python HINTS "${PYTHON_DIR}")
    endif ()
    if (NOT _python)
        message (FATAL_ERROR
            "Python 3 is required to build dependencies.\n"
            "Install Python 3 or set PYTHON_DIR, or disable with -DDE_BUILD_DEPS=OFF.")
    endif ()
    message (STATUS "Building external dependencies via scripts/build_deps.py ...")
    separate_arguments (_dep_args UNIX_COMMAND "${DE_BUILD_DEPS_ARGS}")
    execute_process (
        COMMAND "${_python}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_deps.py"
                -t "${DE_BUILD_DEPS_TYPE}"
                ${_dep_args}
                build
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE _deps_result
    )
    if (NOT _deps_result EQUAL 0)
        message (FATAL_ERROR
            "Dependency build failed (exit code ${_deps_result}).\n"
            "Run manually to see the full output:\n"
            "  python3 scripts/build_deps.py build")
    endif ()
endif ()

include (cmake/Config.cmake)

add_subdirectory (libs)
add_subdirectory (apps)
if (NOT IOS)
    if (DE_ENABLE_TOOLS)
        add_subdirectory (tools)
    endif ()
endif ()

add_subdirectory (post)
