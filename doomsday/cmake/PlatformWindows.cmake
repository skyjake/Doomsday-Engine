set (DENG_PLATFORM_SUFFIX windows)
set (DENG_AMETHYST_PLATFORM WIN32)

if (NOT CYGWIN)
    set (DENG_INSTALL_DATA_DIR   "data")
    set (DENG_INSTALL_DOC_DIR    "doc")
    set (DENG_INSTALL_LIB_DIR    "bin")
    set (DENG_INSTALL_PLUGIN_DIR "${DENG_INSTALL_LIB_DIR}/plugins")
endif ()

add_definitions (
    -DWIN32
    -D_CRT_SECURE_NO_WARNINGS
    -D_USE_MATH_DEFINES
)

if (MSVC)
    add_definitions (
        -DMSVC
        -DWIN32_MSVC
    )

    # Disable warnings about unreferenced formal parameters (C4100).
    append_unique (CMAKE_C_FLAGS   "-w14505 -wd4100 -wd4748")
    append_unique (CMAKE_CXX_FLAGS "-w14505 -wd4100 -wd4748")
endif ()
