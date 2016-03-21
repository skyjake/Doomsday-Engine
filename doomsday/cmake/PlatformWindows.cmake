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

# Code signing.
set (DENG_SIGNTOOL_CERT "" CACHE FILEPATH "Path of the certificate for signing files.")
set (DENG_SIGNTOOL_PASSPHRASE "" CACHE STRING "Signing certificate passphrase.")
find_program (SIGNTOOL_COMMAND signtool)
mark_as_advanced (SIGNTOOL_COMMAND)

function (deng_signtool path)
    install (CODE "
        message (STATUS "\Signing ${path}...\")
        execute_process (COMMAND ${SIGNTOOL_COMMAND}
            /f ${DENG_SIGNTOOL_CERT}
            /p ${DENG_SIGNTOOL_PASSPHRASE}
            /t http://timestamp.verisign.com/scripts/timstamp.dll
            \"${path}\"
        )"
    )
endfunction ()

if (MSVC)
    add_definitions (
        -DMSVC
        -DWIN32_MSVC
    )

    # Disable warnings about unreferenced formal parameters (C4100).
    append_unique (CMAKE_C_FLAGS   "-w14505 -wd4100 -wd4748")
    append_unique (CMAKE_CXX_FLAGS "-w14505 -wd4100 -wd4748")

    # Locate Visual Studio.
    if (MSVC14)
        get_filename_component (VS_DIR 
            [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\14.0\\Setup\\VS;ProductDir]
            REALPATH CACHE
        )
    elseif (MSVC12)
        get_filename_component (VS_DIR 
            [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\12.0\\Setup\\VS;ProductDir]
            REALPATH CACHE
        )
    elseif (MSVC11)
        get_filename_component (VS_DIR 
            [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\11.0\\Setup\\VS;ProductDir]
            REALPATH CACHE
        )
    elseif (MSVC10)
        get_filename_component (VS_DIR 
            [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\10.0\\Setup\\VS;ProductDir]
            REALPATH CACHE
        )
    endif ()

    if (NOT DEFINED VC_REDIST_LIBS)
        file (GLOB VC_REDIST_LIBS ${VS_DIR}/vc/redist/x86/Microsoft.VC120.CRT/msvc*)
        set (VC_REDIST_LIBS ${VC_REDIST_LIBS} CACHE STRING "Visual C++ redistributable libraries")
    endif ()
    if (NOT DEFINED VC_REDIST_LIBS_DEBUG)
        file (GLOB VC_REDIST_LIBS_DEBUG ${VS_DIR}/vc/redist/Debug_NonRedist/x86/Microsoft.VC120.DebugCRT/msvc*)
        set (VC_REDIST_LIBS_DEBUG ${VC_REDIST_LIBS_DEBUG} CACHE STRING 
            "Visual C++ redistributable libraries (debug builds)"
        )
    endif ()
endif ()
