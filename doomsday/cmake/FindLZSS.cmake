set (DE_LZSS_DIR "${DE_EXTERNAL_SOURCE_DIR}/lzss")

if (TARGET lzss)
    # Already defined.
    return ()
endif ()

add_library (lzss STATIC EXCLUDE_FROM_ALL ${DE_LZSS_DIR}/src/lzss.c)
target_include_directories (lzss PUBLIC "${DE_LZSS_DIR}/include")
target_link_libraries (lzss PRIVATE Deng::liblegacy)
set_target_properties (lzss PROPERTIES
    AUTOMOC OFF
    FOLDER Libraries
)
