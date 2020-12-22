# The Doomsday Engine Project
# Copyright (c) 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.1)
project (DE_FLUIDSYNTH)
include (../ExtensionConfig.cmake)

set (tgt audio_fluidsynth)

# Make an embedded build by default?
if (APPLE)
    set (embed ON)
elseif (UNIX)
    set (embed OFF)
endif ()
if (WIN32 AND NOT MINGW)
    message (STATUS "Not building ${tgt} because the MSYS2 version of glib-2.0 is required on Windows (set MSYS2_LIBS_DIR)")
    return ()
endif ()

option (DE_FLUIDSYNTH_EMBEDDED
    "Compile FluidSynth as part of the ${tgt} plugin" ${embed}
)

if (DE_FLUIDSYNTH_EMBEDDED)
    find_package (Glib)

    if (APPLE)
        # Bundle library dependencies on macOS.
        foreach (fn ${GLIB_LIBRARIES})
            list (APPEND macRes ${fn},Frameworks)
        endforeach ()
        deng_find_resources (${macRes})
        add_definitions (-DDARWIN=1)
    endif ()

    add_definitions (
        -D_BSD_SOURCE -DFLUIDSYNTH_NOT_A_DLL -DWITH_FLOAT
        -DHAVE_MATH_H -DHAVE_STDIO_H -DHAVE_STDLIB_H -DHAVE_STRING_H
    )
    if (UNIX)
        add_definitions (
            -DHAVE_SYS_TYPES_H -DHAVE_SYS_SOCKET_H -DHAVE_SYS_TIME_H
            -DHAVE_PTHREAD_H -DHAVE_LIMITS_H -DHAVE_UNISTD_H
            -DHAVE_NETINET_IN_H -DHAVE_NETINET_TCP_H
            -DHAVE_FCNTL_H -DHAVE_ERRNO_H
        )
    endif ()

    set (FS_DIR ${DE_EXTERNAL_SOURCE_DIR}/fluidsynth)
    include_directories (
        ${FS_DIR}/include
        ${FS_DIR}/src
        ${FS_DIR}/src/drivers
        ${FS_DIR}/src/synth
        ${FS_DIR}/src/rvoice
        ${FS_DIR}/src/midi
        ${FS_DIR}/src/utils
        ${FS_DIR}/src/sfloader
        ${FS_DIR}/src/bindings
    )
    set (SOURCES
        ${FS_DIR}/src/utils/fluid_conv.c
        ${FS_DIR}/src/utils/fluid_hash.c
        ${FS_DIR}/src/utils/fluid_list.c
        ${FS_DIR}/src/utils/fluid_ringbuffer.c
        ${FS_DIR}/src/utils/fluid_settings.c
        ${FS_DIR}/src/utils/fluid_sys.c
        ${FS_DIR}/src/sfloader/fluid_defsfont.c
        ${FS_DIR}/src/sfloader/fluid_ramsfont.c
        ${FS_DIR}/src/rvoice/fluid_adsr_env.c
        ${FS_DIR}/src/rvoice/fluid_chorus.c
        ${FS_DIR}/src/rvoice/fluid_iir_filter.c
        ${FS_DIR}/src/rvoice/fluid_lfo.c
        ${FS_DIR}/src/rvoice/fluid_rvoice.c
        ${FS_DIR}/src/rvoice/fluid_rvoice_dsp.c
        ${FS_DIR}/src/rvoice/fluid_rvoice_event.c
        ${FS_DIR}/src/rvoice/fluid_rvoice_mixer.c
        ${FS_DIR}/src/rvoice/fluid_rev.c
        ${FS_DIR}/src/synth/fluid_chan.c
        ${FS_DIR}/src/synth/fluid_event.c
        ${FS_DIR}/src/synth/fluid_gen.c
        ${FS_DIR}/src/synth/fluid_mod.c
        ${FS_DIR}/src/synth/fluid_synth.c
        ${FS_DIR}/src/synth/fluid_tuning.c
        ${FS_DIR}/src/synth/fluid_voice.c
        ${FS_DIR}/src/midi/fluid_midi.c
        ${FS_DIR}/src/midi/fluid_midi_router.c
        ${FS_DIR}/src/midi/fluid_seqbind.c
        ${FS_DIR}/src/midi/fluid_seq.c
        ${FS_DIR}/src/drivers/fluid_adriver.c
        ${FS_DIR}/src/drivers/fluid_mdriver.c
        ${FS_DIR}/src/drivers/fluid_aufile.c
        ${FS_DIR}/src/bindings/fluid_cmd.c
        ${FS_DIR}/src/bindings/fluid_filerenderer.c
    )

else ()
    # We will use the libfluidsynth installed on the system.
    find_package (PkgConfig)
    pkg_check_modules (FLUIDSYNTH fluidsynth)
    if (NOT FLUIDSYNTH_FOUND)
        message (STATUS "Not building ${tgt} because \"fluidsynth\" not found (using pkg-config).\n\
   You could try an embedded build instead (DE_FLUIDSYNTH_EMBEDDED).")
        return ()
    endif ()

    add_definitions (-DFLUIDSYNTH_DEFAULT_DRIVER_NAME="pulseaudio")
endif ()

append (CMAKE_CXX_FLAGS_DEBUG "-DDE_DSFLUIDSYNTH_DEBUG")

include_directories (include)
file (GLOB src src/*.cpp include/*.h)

deng_add_extlib (${tgt} ${src} ${SOURCES})

# Additional compiler options.
if (DE_FLUIDSYNTH_EMBEDDED)
    target_link_libraries (${tgt} PRIVATE glib)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_compile_options (${tgt} PRIVATE
            -fomit-frame-pointer -funroll-all-loops -finline-functions -fdiagnostics-show-option
            -Wno-deprecated-declarations
        )
    endif ()
    if (CMAKE_C_COMPILER_ID MATCHES ".*Clang")
        target_compile_options (${tgt} PRIVATE
            -fomit-frame-pointer -fdiagnostics-show-option -Wno-parentheses -Wno-unused-value
        )
    endif ()

    # if (APPLE)
    #     deng_bundle_install_names (${tgt} ${GLIB_LIBRARIES})
    # endif ()
else ()
    target_link_libraries (${tgt} PRIVATE ${FLUIDSYNTH_LIBRARIES})
endif ()
