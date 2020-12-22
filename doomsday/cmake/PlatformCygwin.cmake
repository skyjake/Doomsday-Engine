include (PlatformGenericUnix)

set (DE_PLATFORM_SUFFIX windows)
set (DE_AMETHYST_PLATFORM WIN32)

set (DE_INSTALL_DATA_DIR "data")
set (DE_INSTALL_DOC_DIR "doc")
#set (DE_INSTALL_LIB_DIR "bin")

add_definitions (
    -D__USE_BSD
    -DWINVER=0x0601
    -D_WIN32_WINNT=0x0601
    -D_GNU_SOURCE=1
    -DDE_PLATFORM_ID="win-${DE_ARCH}"
    -DDE_WINDOWS=1
    -DDE_CYGWIN=1
)

if (CMAKE_COMPILER_IS_GNUCXX)
    append_unique (CMAKE_CXX_FLAGS -Werror=return-type)
    append_unique (CMAKE_CXX_FLAGS -fdiagnostics-color)
endif ()

# All symbols are hidden by default.
append_unique (CMAKE_C_FLAGS   "-fvisibility=hidden")
append_unique (CMAKE_CXX_FLAGS "-fvisibility=hidden")
