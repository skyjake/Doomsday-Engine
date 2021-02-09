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
option (DE_ENABLE_INSTALL_TO_PRODUCTS "Set the install prefix to the 'products' directory" NO)
if (DE_ENABLE_INSTALL_TO_PRODUCTS)
    get_filename_component (defaultInstallPrefix "${DE_SOURCE_DIR}/../products" REALPATH)
    set (CMAKE_INSTALL_PREFIX "${defaultInstallPrefix}" CACHE STRING "Install prefix" FORCE)

    # Some CMake targets may be exported to the install directory.
    list (APPEND CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})
endif ()
