if (IOS)
    # On iOS, we'll install files into the app bundle instead of
    # any user-specified location.
    set (CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/apps/client/\${BUILD_TYPE}\${EFFECTIVE_PLATFORM_NAME})
    return ()
endif ()

if (DEFINED DE_PREFIX)
    message (FATAL_ERROR "DE_PREFIX is obsolete. Use CMAKE_INSTALL_PREFIX instead.")
endif ()

# Default install destination: <root>/products (unless the user specified something else).
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${DE_SOURCE_DIR}/products"
         CACHE PATH "Install prefix" FORCE)
endif ()

# Some CMake targets may be exported to the install directory.
list (APPEND CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})
