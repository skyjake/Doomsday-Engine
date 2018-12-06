if (WIN32)
    set (MINGW_GLIB_DIR "" CACHE PATH "GLib 2.0 SDK directory (mingw)")
endif ()

if (APPLE AND GLIB_STATIC_DIR AND GETTEXT_STATIC_DIR)
    # Use the static versions of GLib and its dependencies.
    add_library (glib INTERFACE)
    target_link_libraries (glib INTERFACE
        ${GLIB_STATIC_DIR}/lib/libglib-2.0.a
        ${GLIB_STATIC_DIR}/lib/libgthread-2.0.a
        ${GETTEXT_STATIC_DIR}/lib/libintl.a
        iconv
    )
    target_include_directories (glib INTERFACE
        ${GLIB_STATIC_DIR}/include/glib-2.0
        ${GLIB_STATIC_DIR}/lib/glib-2.0/include
        ${GETTEXT_STATIC_DIR}/include
    )
    link_framework (glib INTERFACE CoreFoundation)
    link_framework (glib INTERFACE CoreServices)
elseif (MINGW_GLIB_DIR)
    add_library (glib INTERFACE)
    target_link_libraries (glib INTERFACE
        ${MINGW_GLIB_DIR}/${DENG_ARCH}/lib/libglib-2.0-0.lib
        ${MINGW_GLIB_DIR}/${DENG_ARCH}/lib/libgthread-2.0-0.lib
        ws2_32.lib
    )
    target_include_directories (glib INTERFACE
        ${MINGW_GLIB_DIR}/${DENG_ARCH}/include/glib-2.0
        ${MINGW_GLIB_DIR}/${DENG_ARCH}/lib/glib-2.0/include
    )
    if (DENG_ARCH STREQUAL x86)
        deng_install_library (${MINGW_GLIB_DIR}/${DENG_ARCH}/bin/libgcc_s_dw2-1.dll)    
    endif ()
    deng_install_library (${MINGW_GLIB_DIR}/${DENG_ARCH}/bin/libglib-2.0-0.dll)
    deng_install_library (${MINGW_GLIB_DIR}/${DENG_ARCH}/bin/libgthread-2.0-0.dll)
    deng_install_library (${MINGW_GLIB_DIR}/${DENG_ARCH}/bin/libintl-8.dll)
    deng_install_library (${MINGW_GLIB_DIR}/${DENG_ARCH}/bin/libiconv-2.dll)
    deng_install_library (${MINGW_GLIB_DIR}/${DENG_ARCH}/bin/libpcre-1.dll)
    deng_install_library (${MINGW_GLIB_DIR}/${DENG_ARCH}/bin/libwinpthread-1.dll)
else ()
    # Find GLib via pkg-config.
    find_package (PkgConfig)

    add_pkgconfig_interface_library (glib glib-2.0 gthread-2.0)

    get_property (GLIB_LIBRARIES TARGET glib
        PROPERTY INTERFACE_LINK_LIBRARIES
    )
endif ()
