# Doomsday Engine -- Project Directories

set (DE_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set (CMAKE_MODULE_PATH "${DE_CMAKE_DIR}") # for includes

macro (set_path var path)
    get_filename_component (${var} "${path}" REALPATH)
endmacro (set_path)

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message (FATAL_ERROR "In-source builds are not allowed. Please create a build directory and run CMake from there.")
endif ()

set_path (DE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
if (APPLE OR WIN32)
    set_path (DE_DISTRIB_DIR "${DE_SOURCE_DIR}/../products")
else ()
    set_path (DE_DISTRIB_DIR /usr)
endif ()
set (DE_DEPENDS_DIR "${DE_SOURCE_DIR}/../deps")
set (DE_EXTERNAL_SOURCE_DIR "${DE_SOURCE_DIR}/external")
set (DE_API_DIR "${DE_SOURCE_DIR}/apps/api")
set (DE_SDK_DIR "" CACHE PATH "Location of the Doomsday SDK to use for compiling")

if (CMAKE_CONFIGURATION_TYPES)
    set (DE_CONFIGURATION_TYPES ${CMAKE_CONFIGURATION_TYPES})
else ()
    set (DE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})
endif ()

if (UNIX AND NOT (APPLE OR MSYS OR CYGWIN))
    set (UNIX_LINUX YES)
    include (GNUInstallDirs)
else ()
    set (UNIX_LINUX NO)
endif()

# Install directories.
set (DE_INSTALL_DATA_DIR "share/doomsday")
set (DE_INSTALL_DOC_DIR "share/doc")
set (DE_INSTALL_MAN_DIR "share/man/man6")
if (DEFINED CMAKE_INSTALL_LIBDIR)
    set (DE_INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR})
else ()
    set (DE_INSTALL_LIB_DIR "lib")
endif ()
set (DE_INSTALL_CMAKE_DIR "${DE_INSTALL_LIB_DIR}/cmake")
set (DE_INSTALL_PLUGIN_DIR "${DE_INSTALL_LIB_DIR}/doomsday")

set (DE_BUILD_STAGING_DIR "${CMAKE_BINARY_DIR}/bundle-staging")
set (DE_VS_STAGING_DIR "${CMAKE_BINARY_DIR}/products") # for Visual Studio
