set (DENG_LZSS_DIR "${DENG_EXTERNAL_SOURCE_DIR}/lzss")

if (TARGET lzss)
    # Already defined.
    return ()
endif ()

if (WIN32)
    # Use the prebuilt library.
    add_library (lzss INTERFACE)
    target_include_directories (lzss INTERFACE "${DENG_LZSS_DIR}/portable/include")
    target_link_libraries (lzss INTERFACE "${DENG_LZSS_DIR}/win32/lzss.lib")
    deng_install_library ("${DENG_LZSS_DIR}/win32/lzss.dll")
else ()
    add_library (lzss STATIC EXCLUDE_FROM_ALL ${DENG_LZSS_DIR}/unix/src/lzss.c)
    target_include_directories (lzss PUBLIC "${DENG_LZSS_DIR}/portable/include")
    target_link_libraries (lzss PRIVATE Deng::liblegacy)
    set_target_properties (lzss PROPERTIES
        AUTOMOC OFF
        FOLDER Libraries
    )
endif ()
