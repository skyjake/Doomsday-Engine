include (PlatformGenericUnix)

add_definitions (
    -DMACOSX
    # Fallback basedir for command line apps.
    -DDENG_BASE_DIR="${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_DATA_DIR}"
)

set (DENG_FIXED_ASM_DEFAULT OFF)

macro (link_framework target fw)
    find_library (${fw}_LIBRARY ${fw})
    mark_as_advanced (${fw}_LIBRARY)
    target_link_libraries (${target} PRIVATE ${${fw}_LIBRARY})
endmacro (link_framework)
