include (PlatformGenericUnix)

set (DENG_PLATFORM_SUFFIX ios)
#set (DENG_AMETHYST_PLATFORM IOS)

# Install the documentation in the app bundle.
set (DENG_INSTALL_DOC_DIR "Doomsday.app/Contents/Resources/doc")
#set (DENG_INSTALL_MAN_DIR ${DENG_INSTALL_DOC_DIR})

# Code signing.
#set (DENG_CODESIGN_APP_CERT "" CACHE STRING "ID of the certificate for signing applications.")
#find_program (CODESIGN_COMMAND codesign)
#mark_as_advanced (CODESIGN_COMMAND)

# Detect macOS version.
#execute_process (COMMAND sw_vers -productVersion
#    OUTPUT_VARIABLE MACOS_VERSION
#    OUTPUT_STRIP_TRAILING_WHITESPACE
#)

# option (DENG_IOS_SIMULATOR "Target the iPhoneSimulator platform" ON)
#
# if (DENG_IOS_SIMULATOR)
#     set (DENG_IOS_PLATFORM "iPhoneSimulator")
# else ()
#     set (DENG_IOS_PLATFORM "iPhoneOS")
# endif ()
#
# execute_process (
#     COMMAND xcode-select -p
#     OUTPUT_VARIABLE XCODE_DEVROOT
#     OUTPUT_STRIP_TRAILING_WHITESPACE
# )

#set (DENG_IOS_SDKVER "10.3")
#set (DENG_IOS_DEVROOT "${XCODE_DEVROOT}/Platforms/${DENG_IOS_PLATFORM}.platform/Developer")
#set (CMAKE_OSX_DEPLOYMENT_TARGET "${DENG_IOS_SDKVER}")
#set (CMAKE_OSX_SYSROOT "${DENG_IOS_DEVROOT}/SDKs/${DENG_IOS_PLATFORM}${DENG_IOS_SDKVER}.sdk"
#    CACHE PATH "Sysroot path"
#)
if (IOS_PLATFORM STREQUAL SIMULATOR)
    set (CMAKE_OSX_ARCHITECTURES "x86_64")
else ()
    set (CMAKE_OSX_ARCHITECTURES "arm64")
endif ()

add_definitions (
    -DDENG_APPLE=1
    -DDENG_MOBILE=1
    -DDENG_IOS=1
    -DDENG_BASE_DIR="."
)

set (DENG_FIXED_ASM_DEFAULT OFF)

set (XCODE_ATTRIBUTE_USE_HEADERMAP NO)
set (XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH YES)

macro (link_framework target linkType fw)
    find_library (${fw}_LIBRARY ${fw})
    if (${fw}_LIBRARY STREQUAL "${fw}_LIBRARY-NOTFOUND")
        message (FATAL_ERROR "link_framework: ${fw} framework not found")
    endif ()
    mark_as_advanced (${fw}_LIBRARY)
    target_link_libraries (${target} ${linkType} ${${fw}_LIBRARY})
endmacro (link_framework)

macro (deng_xcode_attribs target)
    # set_target_properties (${target} PROPERTIES
    #     XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN NO
    #     XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN NO
    # )
endmacro (deng_xcode_attribs)

macro (macx_set_bundle_name name)
    # Underscores are not allowed in bundle identifiers.
    string (REPLACE "_" "." MACOSX_BUNDLE_BUNDLE_NAME ${name})
endmacro (macx_set_bundle_name)
