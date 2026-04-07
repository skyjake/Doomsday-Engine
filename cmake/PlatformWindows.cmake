set (DE_PLATFORM_SUFFIX windows)
set (DE_AMETHYST_PLATFORM WIN32)

if (NOT CYGWIN)
    set (DE_INSTALL_DATA_DIR   "data")
    set (DE_INSTALL_DOC_DIR    "doc")
    set (DE_INSTALL_LIB_DIR    "bin")
    set (DE_INSTALL_PLUGIN_DIR "${DE_INSTALL_LIB_DIR}/plugins")
endif ()

add_definitions (
    -DWIN32
    -DWINVER=0x0601
    -D_WIN32_WINNT=0x0601
    -D_CRT_SECURE_NO_WARNINGS
    -D_USE_MATH_DEFINES
    -DDE_PLATFORM_ID="win-${DE_ARCH}"
    -DDE_WINDOWS=1
)

# Code signing.
set (DE_SIGNTOOL_CERT "" CACHE STRING "Name of the certificate for signing files.")
set (DE_SIGNTOOL_PIN "" CACHE STRING "PIN for signing key.")
set (DE_SIGNTOOL_TIMESTAMP "" CACHE STRING "URL of the signing timestamp server.")
find_program (SIGNTOOL_COMMAND signtool)
mark_as_advanced (SIGNTOOL_COMMAND)

function (deng_signtool path comp)
    install (CODE "message (STATUS \"Signing ${path}...\")
        execute_process (COMMAND \"${SIGNTOOL_COMMAND}\" -pin ${DE_SIGNTOOL_PIN}
            sign
            /n \"${DE_SIGNTOOL_CERT}\"
            /t ${DE_SIGNTOOL_TIMESTAMP}
            /fd sha1
            /v
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
    append_unique (CMAKE_CXX_FLAGS "-w14505 -wd4100 -wd4748")

    # de::Error is derived from std::runtime_error (non-dll-interface class).
    append_unique (CMAKE_CXX_FLAGS "-wd4251 -wd4275")

    # Possible loss of data due to number conversion.
    append_unique (CMAKE_CXX_FLAGS "-wd4244")

    # Enable multi-processor compiling.
    append_unique (CMAKE_C_FLAGS   "-MP")
    append_unique (CMAKE_CXX_FLAGS "-MP")

    if (ARCH_BITS EQUAL 64)
        # There are many warnings about possible loss of data due to implicit
        # type conversions.
        append_unique (CMAKE_C_FLAGS   "-wd4267")
        append_unique (CMAKE_CXX_FLAGS "-wd4267")
    endif ()

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
                ${VC_REDIST_DIR}/${DE_ARCH}/Microsoft.VC140.CRT/msvc*dll
                ${VC_REDIST_DIR}/${DE_ARCH}/Microsoft.VC140.CRT/vcruntime*dll
                ${WINDOWS_KIT_REDIST_DLL_DIR}/${DE_ARCH}/*.dll
            )
            set (VC_REDIST_LIBS ${VC_REDIST_LIBS} CACHE STRING "Visual C++/UCRT redistributable libraries")
        endif ()
        if (NOT VC_REDIST_LIBS_DEBUG)
            file (GLOB VC_REDIST_LIBS_DEBUG
                ${VC_REDIST_DIR}/Debug_NonRedist/${DE_ARCH}/Microsoft.VC140.DebugCRT/msvc*
                ${VC_REDIST_DIR}/Debug_NonRedist/${DE_ARCH}/Microsoft.VC140.DebugCRT/vcruntime*
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
