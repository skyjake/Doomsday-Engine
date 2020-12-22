set (DE_LZSS_DIR "${DE_EXTERNAL_SOURCE_DIR}/lzss")

if (TARGET lzss)
    # Already defined.
    return ()
endif ()

add_library (lzss STATIC EXCLUDE_FROM_ALL ${DE_LZSS_DIR}/src/lzss.c)
target_include_directories (lzss PUBLIC "${DE_LZSS_DIR}/include")
deng_link_libraries (lzss PRIVATE DengCore)
if (UNIX_LINUX)
    target_compile_options (lzss PRIVATE -fPIC)
endif ()
set_target_properties (lzss PROPERTIES
    AUTOMOC OFF
    FOLDER Libraries
)
