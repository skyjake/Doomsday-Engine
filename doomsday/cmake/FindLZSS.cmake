set (DENG_LZSS_DIR "${DENG_EXTERNAL_SOURCE_DIR}/lzss")

if (TARGET lzss)
    # Already defined.
    return ()
endif ()

if (WIN32)
    add_library (lzss INTERFACE)
    target_include_directories (lzss INTERFACE "${DENG_LZSS_DIR}/portable/include")
    target_link_libraries (lzss INTERFACE "${DENG_LZSS_DIR}/win32/lzss.lib")
else ()
    add_library (lzss STATIC EXCLUDE_FROM_ALL ${DENG_LZSS_DIR}/unix/src/lzss.c)
    target_include_directories (lzss PUBLIC "${DENG_LZSS_DIR}/portable/include")
    target_link_libraries (lzss PRIVATE Deng::liblegacy)
    set_property (TARGET lzss PROPERTY AUTOMOC OFF)
endif ()

set_property (TARGET lzss PROPERTY FOLDER Libraries)


# INCLUDEPATH += $$DENG_LZSS_DIR/portable/include
#
# HEADERS += \
#     $$DENG_LZSS_DIR/portable/include/lzss.h
#
# win32 {
#     LIBS += -L$$DENG_LZSS_DIR/win32 -llzss
#
#     # Installed shared libs.
#     INSTALLS += lzsslibs
#     lzsslibs.files = $$DENG_LZSS_DIR/win32/lzss.dll
#     lzsslibs.path = $$DENG_LIB_DIR
# }
# else {
#     SOURCES += \
#         $$DENG_LZSS_DIR/unix/src/lzss.c
# }
