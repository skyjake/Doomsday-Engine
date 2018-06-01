include (PlatformGenericUnix)

set (DE_PLATFORM_SUFFIX ios)
#set (DE_AMETHYST_PLATFORM IOS)

set (DE_STATIC_LINK YES)

# Install the documentation in the app bundle.
set (DE_INSTALL_DOC_DIR "Doomsday.app/Contents/Resources/doc")
#set (DE_INSTALL_MAN_DIR ${DE_INSTALL_DOC_DIR})

# Code signing.
#set (DE_CODESIGN_APP_CERT "" CACHE STRING "ID of the certificate for signing applications.")
#find_program (CODESIGN_COMMAND codesign)
#mark_as_advanced (CODESIGN_COMMAND)

# Detect macOS version.
#execute_process (COMMAND sw_vers -productVersion
#    OUTPUT_VARIABLE MACOS_VERSION
#    OUTPUT_STRIP_TRAILING_WHITESPACE
#)

# option (DE_IOS_SIMULATOR "Target the iPhoneSimulator platform" ON)
#
# if (DE_IOS_SIMULATOR)
#     set (DE_IOS_PLATFORM "iPhoneSimulator")
# else ()
#     set (DE_IOS_PLATFORM "iPhoneOS")
# endif ()
#
# execute_process (
#     COMMAND xcode-select -p
#     OUTPUT_VARIABLE XCODE_DEVROOT
#     OUTPUT_STRIP_TRAILING_WHITESPACE
# )

#set (DE_IOS_SDKVER "10.3")
#set (DE_IOS_DEVROOT "${XCODE_DEVROOT}/Platforms/${DE_IOS_PLATFORM}.platform/Developer")
#set (CMAKE_OSX_DEPLOYMENT_TARGET "${DE_IOS_SDKVER}")
#set (CMAKE_OSX_SYSROOT "${DE_IOS_DEVROOT}/SDKs/${DE_IOS_PLATFORM}${DE_IOS_SDKVER}.sdk"
#    CACHE PATH "Sysroot path"
#)
if (IOS_PLATFORM STREQUAL SIMULATOR)
    set (CMAKE_OSX_ARCHITECTURES "x86_64")
else ()
    set (CMAKE_OSX_ARCHITECTURES "arm64")
endif ()

append_unique (CMAKE_CXX_FLAGS "-fvisibility=hidden")
append_unique (CMAKE_CXX_FLAGS "-fvisibility-inlines-hidden")
append_unique (CMAKE_CXX_FLAGS "-Wno-inconsistent-missing-override")
append_unique (CMAKE_C_FLAGS   "-Wno-shorten-64-to-32")
append_unique (CMAKE_CXX_FLAGS "-Wno-shorten-64-to-32")

add_definitions (
    -DDE_STATIC_LINK=1
    -DDE_APPLE=1
    -DDE_MOBILE=1
    -DDE_IOS=1
    -DDE_BASE_DIR="."
)

set (DE_FIXED_ASM_DEFAULT OFF)

set (XCODE_ATTRIBUTE_USE_HEADERMAP NO)

macro (link_framework target linkType fw)
    find_library (${fw}_LIBRARY ${fw})
    if (${fw}_LIBRARY STREQUAL "${fw}_LIBRARY-NOTFOUND")
        message (FATAL_ERROR "link_framework: ${fw} framework not found")
    endif ()
    mark_as_advanced (${fw}_LIBRARY)
    target_link_libraries (${target} ${linkType} ${${fw}_LIBRARY})
endmacro (link_framework)

macro (deng_xcode_attribs target)
    if (IOS_PLATFORM STREQUAL SIMULATOR)
        set_target_properties (${target} PROPERTIES
            XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH YES        
        )
    endif ()
    # set_target_properties (${target} PROPERTIES
    #     XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN NO
    #     XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN NO
    # )
endmacro (deng_xcode_attribs)

macro (macx_set_bundle_name name)
    # Underscores are not allowed in bundle identifiers.
    string (REPLACE "_" "." MACOSX_BUNDLE_BUNDLE_NAME ${name})
endmacro (macx_set_bundle_name)
