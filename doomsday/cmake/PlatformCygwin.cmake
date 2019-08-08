include (PlatformGenericUnix)

set (DE_PLATFORM_SUFFIX windows)
set (DE_AMETHYST_PLATFORM WIN32)

# if (NOT CYGWIN)
#     set (DE_INSTALL_DATA_DIR   "data")
#     set (DE_INSTALL_DOC_DIR    "doc")
#     set (DE_INSTALL_LIB_DIR    "bin")
# endif ()

add_definitions (
    -D__USE_BSD
    -DWINVER=0x0601
    -D_WIN32_WINNT=0x0601
    -D_GNU_SOURCE=1
    -DDE_PLATFORM_ID="win-${DE_ARCH}"
)
