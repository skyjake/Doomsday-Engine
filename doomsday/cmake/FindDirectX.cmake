if (WIN32 AND NOT TARGET DirectX)
    find_library (DIRECTX_GUID_LIBRARY dxguid
        PATHS ${DIRECTX_DIR}
        HINTS ENV DXSDK_DIR
        # TODO: look in the registry?
        PATH_SUFFIXES Lib/${DE_ARCH} ${DE_ARCH}
    )
    mark_as_advanced (DIRECTX_GUID_LIBRARY)
    if (NOT DIRECTX_GUID_LIBRARY)
        message (FATAL_ERROR "DirectX SDK not found. Set the DIRECTX_DIR variable.")
    endif ()

    get_filename_component (_libDir ${DIRECTX_GUID_LIBRARY} DIRECTORY)
    get_filename_component (_incDir ${_libDir}/../../include REALPATH)

    add_library (DirectX INTERFACE)
    target_include_directories (DirectX INTERFACE ${_incDir})
    target_link_libraries (DirectX INTERFACE
        ${_libDir}/dinput8.lib
        ${_libDir}/dsound.lib
        ${DIRECTX_GUID_LIBRARY}
    )
endif ()
