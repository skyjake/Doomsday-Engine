include (PlatformGenericUnix)

set (DENG_PLATFORM_SUFFIX macx)
set (DENG_AMETHYST_PLATFORM MACOSX)

# Install the documentation in the app bundle.
set (DENG_INSTALL_DOC_DIR "Doomsday.app/Contents/Resources/doc")
set (DENG_INSTALL_MAN_DIR ${DENG_INSTALL_DOC_DIR})

# Code signing.
set (DENG_CODESIGN_APP_CERT "" CACHE STRING "ID of the certificate for signing applications.")
find_program (CODESIGN_COMMAND codesign)
mark_as_advanced (CODESIGN_COMMAND)

# Detect macOS version.
execute_process (COMMAND sw_vers -productVersion
    OUTPUT_VARIABLE MACOS_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_definitions (
    -DMACOSX=1
    -DDENG_APPLE=1
    -DDENG_PLATFORM_ID="mac10_10-${DENG_ARCH}"
    # Fallback basedir for command line apps.
    -DDENG_BASE_DIR="${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_DATA_DIR}"
)
if (NOT MACOS_VERSION VERSION_LESS 10.7)
    add_definitions (-DMACOS_10_7=1)
endif ()
if (MACOS_VERSION VERSION_LESS 10.12)
    # QTKit has been deprecated; should use AVFoundation instead.
    set (DENG_HAVE_QTKIT YES CACHE INTERNAL "")
    add_definitions (-DMACOS_HAVE_QTKIT=1)
else ()
    set (DENG_HAVE_QTKIT NO CACHE INTERNAL "")
endif ()

# Check compiler version.
if (NOT DEFINED CLANG_VERSION_STRING AND ${CMAKE_CXX_COMPILER_ID} MATCHES ".*Clang.*")
    execute_process (COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE CLANG_VERSION_FULL)
    string (REGEX REPLACE ".*version ([0-9]+\\.[0-9]+).*" "\\1" CLANG_VERSION_STRING ${CLANG_VERSION_FULL})
    set (CLANG_VERSION_STRING "${CLANG_VERSION_STRING}" CACHE INTERNAL "Clang version number")
endif ()

if (${CLANG_VERSION_STRING} VERSION_EQUAL 7.0 OR
    ${CLANG_VERSION_STRING} VERSION_GREATER 7.0)
    # Too many warnings from Qt.
    append_unique (CMAKE_CXX_FLAGS "-Wno-deprecated-declarations -Wno-inconsistent-missing-override")
endif ()

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
