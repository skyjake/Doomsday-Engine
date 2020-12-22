# Doomsday Engine -- General configuration
#
# All CMakeLists should include this file to gain access to the overall
# project configuration.

if (POLICY CMP0068)
    cmake_policy (SET CMP0068 NEW)  # macOS: RPATH affects install_name
endif ()
if (POLICY CMP0079)
    cmake_policy (SET CMP0079 NEW)
endif ()

get_filename_component (_where "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
message (STATUS "Configuring ${_where}...")

set_property (GLOBAL PROPERTY USE_FOLDERS ON)

include (${CMAKE_CURRENT_LIST_DIR}/Directories.cmake)
include (Macros)
include (Arch)
include (BuildTypes)
include (InstallPrefix)
include (Version)
find_package (Ccache)
include (Options)
include (Packaging)

set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "client")

# Prefix path is used for finding CMake config packages.
if (NOT DE_SDK_DIR STREQUAL "")
    list (APPEND CMAKE_PREFIX_PATH "${DE_SDK_DIR}/${DE_INSTALL_LIB_DIR}")
endif ()
list (APPEND CMAKE_PREFIX_PATH "${DE_DEPENDS_DIR}/products")
list (APPEND CMAKE_MODULE_PATH "${DE_DEPENDS_DIR}/products/lib/cmake/assimp-4.1")

# Platform-Specific Configuration --------------------------------------------

if (MSYS)
    include (PlatformMsys)
elseif (CYGWIN)
    include (PlatformCygwin)
elseif (IOS)
    include (PlatformiOS)
elseif (APPLE)
    include (PlatformMacx)
elseif (WIN32)
    include (PlatformWindows)
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND EXISTS /opt/vc/include/bcm_host.h)
    include (PlatformRaspberryPi)
else ()
    include (PlatformUnix)
endif ()

# Helpers --------------------------------------------------------------------

# CMake 3.12 adds a better Python finder, but older OS distros like Fedora 23
# only come with older versions, and we don't want to require manually
# installing a newer CMake.

find_program (PYTHON_EXECUTABLE python3 HINTS "${PYTHON_DIR}")
find_program (PYTHON_EXECUTABLE python HINTS "${PYTHON_DIR}")
if (NOT PYTHON_EXECUTABLE)
    message (FATAL_ERROR "Python 3 required; did not found a python on the path (set PYTHON_DIR)")
endif ()
execute_process (COMMAND ${PYTHON_EXECUTABLE} --version OUTPUT_VARIABLE pythonVer)
string (REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" pythonVer ${pythonVer})
#message (STATUS "Found Python: ${PYTHON_EXECUTABLE} (version: ${pythonVer})")

if (pythonVer VERSION_LESS 3)
    message (FATAL_ERROR "Python 3 required; found ${pythonVer}")
endif ()

if (DE_ENABLE_COTIRE)
    include (cotire)
endif ()
include (LegacyPK3s)
