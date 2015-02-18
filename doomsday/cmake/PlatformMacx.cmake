include (PlatformGenericUnix)

add_definitions (
    -DMACOSX
    # Fallback basedir for command line apps.
    -DDENG_BASE_DIR="${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_DATA_DIR}"
)

set (DENG_FIXED_ASM_DEFAULT OFF)

macro (link_framework target linkType fw)
    find_library (${fw}_LIBRARY ${fw})
    if (${fw}_LIBRARY STREQUAL "${fw}_LIBRARY-NOTFOUND")
        message (FATAL_ERROR "link_framework: ${fw} framework not found")
    endif ()
    mark_as_advanced (${fw}_LIBRARY)
    target_link_libraries (${target} ${linkType} ${${fw}_LIBRARY})
endmacro (link_framework)

macro (deng_xcode_attribs target)
    set_target_properties (${target} PROPERTIES
        XCODE_ATTRIBUTE_USE_HEADERMAP NO
        XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN NO
        XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN NO
    )
endmacro (deng_xcode_attribs)
