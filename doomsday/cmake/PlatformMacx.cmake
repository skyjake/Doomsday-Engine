include (PlatformGenericUnix)

set (DENG_PLATFORM_SUFFIX macx)
set (DENG_AMETHYST_PLATFORM MACOSX)

# Install the documentation in the app bundle.
set (DENG_INSTALL_DOC_DIR "Doomsday.app/Contents/Resources/doc")
set (DENG_INSTALL_MAN_DIR ${DENG_INSTALL_DOC_DIR})

set (DENG_CODESIGN_APP_CERT "" CACHE STRING "ID of the certificate for signing applications.")
find_program (CODESIGN_COMMAND codesign)
mark_as_advanced (CODESIGN_COMMAND)

# Detect OS X version.
execute_process (COMMAND sw_vers -productVersion 
    OUTPUT_VARIABLE MACOS_VERSION 
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (NOT MACOS_VERSION VERSION_LESS 10.7)
    add_definitions (-DMACOS_10_7)
endif ()
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

macro (macx_set_bundle_name name)
    # Underscores are not allowed in bundle identifiers.
    string (REPLACE "_" "." MACOSX_BUNDLE_BUNDLE_NAME ${name})
endmacro (macx_set_bundle_name)
