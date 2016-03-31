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
    -DWINVER=0x0601
    -D_WIN32_WINNT=0x0601
    -D_CRT_SECURE_NO_WARNINGS
    -D_USE_MATH_DEFINES
)

# Code signing.
set (DENG_SIGNTOOL_CERT "" CACHE FILEPATH "Path of the certificate for signing files.")
set (DENG_SIGNTOOL_PASSPHRASE "" CACHE STRING "Signing certificate passphrase.")
find_program (SIGNTOOL_COMMAND signtool)
mark_as_advanced (SIGNTOOL_COMMAND)

function (deng_signtool path comp)
    install (CODE "message (STATUS \"Signing ${path}...\")
        execute_process (COMMAND \"${SIGNTOOL_COMMAND}\" sign
            /f \"${DENG_SIGNTOOL_CERT}\"
            /p ${DENG_SIGNTOOL_PASSPHRASE}
            /t http://timestamp.verisign.com/scripts/timstamp.dll
            \"\${CMAKE_INSTALL_PREFIX}/${path}\"
        )"
        COMPONENT ${comp}
    )
endfunction ()

if (MSVC)
    add_definitions (
        -DMSVC
        -DWIN32_MSVC
    )

    # Disable warnings about unreferenced formal parameters (C4100).
    append_unique (CMAKE_C_FLAGS   "-w14505 -wd4100 -wd4748")
    append_unique (CMAKE_CXX_FLAGS "-w14505 -wd4100 -wd4748 -Zm300")

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

    set (VC_REDIST_DIR ${VS_DIR}/vc/redist)

    if (MSVC14)
        if (NOT VC_REDIST_LIBS)
            file (GLOB VC_REDIST_LIBS
                ${VC_REDIST_DIR}/${DENG_ARCH}/Microsoft.VC140.CRT/msvc*dll
                ${VC_REDIST_DIR}/${DENG_ARCH}/Microsoft.VC140.CRT/vcruntime*dll
                ${WINDOWS_KIT_REDIST_DLL_DIR}/${DENG_ARCH}/*.dll
            )
            set (VC_REDIST_LIBS ${VC_REDIST_LIBS} CACHE STRING "Visual C++/UCRT redistributable libraries")
        endif ()
        if (NOT VC_REDIST_LIBS_DEBUG)
            file (GLOB VC_REDIST_LIBS_DEBUG
                ${VC_REDIST_DIR}/Debug_NonRedist/${DENG_ARCH}/Microsoft.VC140.DebugCRT/msvc*
                ${VC_REDIST_DIR}/Debug_NonRedist/${DENG_ARCH}/Microsoft.VC140.DebugCRT/vcruntime*
            )
            set (VC_REDIST_LIBS_DEBUG ${VC_REDIST_LIBS_DEBUG} CACHE STRING
                "Visual C++ redistributable libraries (debug builds)"
            )
        endif ()
    endif ()

    if (MSVC12)
        if (NOT DEFINED VC_REDIST_LIBS)
            file (GLOB VC_REDIST_LIBS ${VC_REDIST_DIR}/x86/Microsoft.VC120.CRT/msvc*)
            set (VC_REDIST_LIBS ${VC_REDIST_LIBS} CACHE STRING "Visual C++ redistributable libraries")
        endif ()
        if (NOT DEFINED VC_REDIST_LIBS_DEBUG)
            file (GLOB VC_REDIST_LIBS_DEBUG ${VC_REDIST_DIR}/Debug_NonRedist/x86/Microsoft.VC120.DebugCRT/msvc*)
            set (VC_REDIST_LIBS_DEBUG ${VC_REDIST_LIBS_DEBUG} CACHE STRING
                "Visual C++ redistributable libraries (debug builds)"
            )
        endif ()
    endif ()
endif ()
