# Doomsday Engine -- General configuration
#
# All CMakeLists should include this file to gain access to the overall
# project configuration.

if (POLICY CMP0068)
    cmake_policy (SET CMP0068 OLD)  # macOS: RPATH affects install_name
endif ()

get_filename_component (_where "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
message (STATUS "Configuring ${_where}...")

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

if (WIN32)
    set (MSYS2_LIBS_DIR "" CACHE PATH "msys2-libs directory")
endif ()
# find_package (the_Foundation REQUIRED)

# Platform-Specific Configuration --------------------------------------------

if (IOS)
    include (PlatformiOS)
elseif (APPLE)
    include (PlatformMacx)
elseif (WIN32)
    include (PlatformWindows)
else ()
    include (PlatformUnix)
endif ()

# Helpers --------------------------------------------------------------------

set (Python_ADDITIONAL_VERSIONS 2.7)
find_package (PythonInterp REQUIRED)

if (DE_ENABLE_COTIRE)
    include (cotire)
endif ()
include (LegacyPK3s)
