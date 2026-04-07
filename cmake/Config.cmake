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
# deps/products is prepended so it takes priority over any system-wide installs.
list (INSERT CMAKE_PREFIX_PATH 0 "${DE_DEPENDS_DIR}/products")
if (NOT DE_SDK_DIR STREQUAL "")
    list (APPEND CMAKE_PREFIX_PATH "${DE_SDK_DIR}/${DE_INSTALL_LIB_DIR}")
endif ()

# Prepend deps/products/include as an explicit -I flag at the start of
# CMAKE_CXX_FLAGS/CMAKE_C_FLAGS so it appears before all include_directories
# entries in the compile command.  This ensures deps headers take priority
# over any system-wide installation (e.g. /usr/local/include on macOS).
foreach (_lang C CXX)
    set (CMAKE_${_lang}_FLAGS "-I${DE_DEPENDS_DIR}/products/include ${CMAKE_${_lang}_FLAGS}"
         CACHE STRING "Flags for ${_lang} compiler" FORCE)
endforeach ()

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
