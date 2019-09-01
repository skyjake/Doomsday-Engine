if (IOS)
    # On iOS, we'll install files into the app bundle instead of
    # any user-specified location.
    set (CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/apps/client/\${BUILD_TYPE}\${EFFECTIVE_PLATFORM_NAME})
    return ()
endif ()

if (DEFINED DE_PREFIX)
    message (FATAL_ERROR "DE_PREFIX is obsolete. Use CMAKE_INSTALL_PREFIX instead.")
endif ()

# Install destination.
get_filename_component (defaultInstallPrefix "${DE_DISTRIB_DIR}" REALPATH)
set (CMAKE_INSTALL_PREFIX "${defaultInstallPrefix}" CACHE STRING "Install prefix")

# Some CMake targets may be exported to the install directory.
list (APPEND CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})
