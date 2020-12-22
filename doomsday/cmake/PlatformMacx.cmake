include (PlatformGenericUnix)

set (DE_PLATFORM_SUFFIX macx)
set (DE_AMETHYST_PLATFORM MACOSX)

# Install the documentation in the app bundle.
set (DE_INSTALL_DOC_DIR "Doomsday.app/Contents/Resources/doc")
set (DE_INSTALL_MAN_DIR ${DE_INSTALL_DOC_DIR})

# Code signing.
set (DE_CODESIGN_APP_CERT "" CACHE STRING "ID of the certificate for signing applications.")
find_program (CODESIGN_COMMAND codesign)
mark_as_advanced (CODESIGN_COMMAND)

# Detect macOS version.
execute_process (COMMAND sw_vers -productVersion
    OUTPUT_VARIABLE MACOS_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_definitions (
    -DMACOSX=1
    -DDE_APPLE=1
    -DDE_PLATFORM_ID="mac10_10-${DE_ARCH}"
    # Fallback basedir for command line apps.
    -DDE_BASE_DIR="${CMAKE_INSTALL_PREFIX}/${DE_INSTALL_DATA_DIR}"
)
if (NOT MACOS_VERSION VERSION_LESS 10.7)
    add_definitions (-DMACOS_10_7=1)
endif ()
if (MACOS_VERSION VERSION_LESS 10.12)
    # QTKit has been deprecated; should use AVFoundation instead.
    set (DE_HAVE_QTKIT YES CACHE INTERNAL "")
    add_definitions (-DMACOS_HAVE_QTKIT=1)
else ()
    set (DE_HAVE_QTKIT NO CACHE INTERNAL "")
endif ()

# Check compiler version.
if (NOT DEFINED CLANG_VERSION_STRING AND "${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang.*")
    execute_process (COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE CLANG_VERSION_FULL)
    string (REGEX REPLACE ".*version ([0-9]+\\.[0-9]+).*" "\\1" CLANG_VERSION_STRING ${CLANG_VERSION_FULL})
    set (CLANG_VERSION_STRING "${CLANG_VERSION_STRING}" CACHE INTERNAL "Clang version number")
endif ()

if (${CLANG_VERSION_STRING} VERSION_EQUAL 7.0 OR
    ${CLANG_VERSION_STRING} VERSION_GREATER 7.0)
    # Too many warnings from Qt.
    append_unique (CMAKE_CXX_FLAGS "-Wno-deprecated-declarations -Wno-inconsistent-missing-override")
    append_unique (CMAKE_CXX_FLAGS "-Wno-gnu-anonymous-struct")
    append_unique (CMAKE_CXX_FLAGS "-Wno-nested-anon-types")
endif ()

# All symbols are hidden by default.
append_unique (CMAKE_C_FLAGS   "-fvisibility=hidden")
append_unique (CMAKE_CXX_FLAGS "-fvisibility=hidden")

set (DE_FIXED_ASM_DEFAULT OFF)

macro (link_framework target linkType fw)
    find_library (${fw}_LIBRARY ${fw})
    if (${fw}_LIBRARY STREQUAL "${fw}_LIBRARY-NOTFOUND")
        message (FATAL_ERROR "link_framework: ${fw} framework not found")
    endif ()
    mark_as_advanced (${fw}_LIBRARY)
    target_link_libraries (${target} ${linkType} ${${fw}_LIBRARY})
endmacro (link_framework)
