# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config_plugin.pri)
include(../../dep_glib.pri)

TEMPLATE = lib

win32|macx: TARGET = dsFluidSynth
      else: TARGET = dsfluidsynth

CONFIG -= qt

# Define this to get debug messages.
DEFINES += DENG_DSFLUIDSYNTH_DEBUG

win32 {
    RC_FILE = res/fluidsynth.rc

    QMAKE_LFLAGS += /DEF:\"$$PWD/api/dsfluidsynth.def\"
    OTHER_FILES += api/dsfluidsynth.def

    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
macx {
    linkToBundledLibdeng2(dsFluidSynth)
    linkToBundledLibdeng(dsFluidSynth)
}
unix:!macx {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}

# libfluidsynth config ------------------------------------------------------

DEFINES += FLUIDSYNTH_NOT_A_DLL WITH_FLOAT \
    HAVE_MATH_H HAVE_STDIO_H HAVE_STDLIB_H HAVE_STRING_H

*-g++* {
    QMAKE_CFLAGS += -fomit-frame-pointer -funroll-all-loops -finline-functions -fdiagnostics-show-option
    QMAKE_CFLAGS_WARN_ON -= -Wall -W
    QMAKE_CFLAGS_WARN_ON += -Wno-deprecated-declarations
}
*-clang* {
    QMAKE_CFLAGS += -fomit-frame-pointer -finline-functions -fdiagnostics-show-option
    QMAKE_CFLAGS_WARN_ON -= -Wall -W
    QMAKE_CFLAGS_WARN_ON += -Wno-deprecated-declarations -Wno-parentheses
}

macx {
    DEFINES += DARWIN
}
unix {
    DEFINES += HAVE_SYS_TYPES_H HAVE_SYS_SOCKET_H HAVE_SYS_TIME_H \
        HAVE_PTHREAD_H HAVE_LIMITS_H HAVE_UNISTD_H HAVE_NETINET_IN_H \
        HAVE_NETINET_TCP_H HAVE_FCNTL_H HAVE_ERRNO_H
}
#unix:!macx {
#    INSTALLS += target fsh headers
#    target.path = /usr/local/lib
#    fsh.path = /usr/local/include
#    headers.path = /usr/local/include/fluidsynth
#}

# Sources -------------------------------------------------------------------

INCLUDEPATH += include

HEADERS += \
    include/driver_fluidsynth.h \
    include/fluidsynth_music.h \
    include/version.h

SOURCES += \
    src/driver_fluidsynth.cpp \
    src/fluidsynth_music.cpp

# libfluidsynth
FS_DIR = ../../external/fluidsynth

INCLUDEPATH += \
    $${FS_DIR}/include \
    $${FS_DIR}/src \
    $${FS_DIR}/src/drivers \
    $${FS_DIR}/src/synth \
    $${FS_DIR}/src/rvoice \
    $${FS_DIR}/src/midi \
    $${FS_DIR}/src/utils \
    $${FS_DIR}/src/sfloader \
    $${FS_DIR}/src/bindings

fsh.files = $${FS_DIR}include/fluidsynth.h

headers.files = \
    $${FS_DIR}include/fluidsynth/audio.h \
    $${FS_DIR}/include/fluidsynth/event.h \
    $${FS_DIR}/include/fluidsynth/gen.h \
    $${FS_DIR}/include/fluidsynth/log.h \
    $${FS_DIR}/include/fluidsynth/midi.h \
    $${FS_DIR}/include/fluidsynth/misc.h \
    $${FS_DIR}/include/fluidsynth/mod.h \
    $${FS_DIR}/include/fluidsynth/ramsfont.h \
    $${FS_DIR}/include/fluidsynth/seq.h \
    $${FS_DIR}/include/fluidsynth/seqbind.h \
    $${FS_DIR}/include/fluidsynth/settings.h \
    $${FS_DIR}/include/fluidsynth/sfont.h \
    $${FS_DIR}/include/fluidsynth/shell.h \
    $${FS_DIR}/include/fluidsynth/synth.h \
    $${FS_DIR}/include/fluidsynth/types.h \
    $${FS_DIR}/include/fluidsynth/voice.h \
    $${FS_DIR}/include/fluidsynth/version.h

HEADERS += $$fsh.files $$headers.files

# Internal headers.
HEADERS += \
    $${FS_DIR}/src/config.h \
    $${FS_DIR}/src/utils/fluid_conv.h \
    $${FS_DIR}/src/utils/fluid_hash.h \
    $${FS_DIR}/src/utils/fluid_list.h \
    $${FS_DIR}/src/utils/fluid_ringbuffer.h \
    $${FS_DIR}/src/utils/fluid_settings.h \
    $${FS_DIR}/src/utils/fluidsynth_priv.h \
    $${FS_DIR}/src/utils/fluid_sys.h \
    $${FS_DIR}/src/sfloader/fluid_defsfont.h \
    $${FS_DIR}/src/sfloader/fluid_ramsfont.h \
    $${FS_DIR}/src/sfloader/fluid_sfont.h \
    $${FS_DIR}/src/rvoice/fluid_adsr_env.h \
    $${FS_DIR}/src/rvoice/fluid_chorus.h \
    $${FS_DIR}/src/rvoice/fluid_iir_filter.h \
    $${FS_DIR}/src/rvoice/fluid_lfo.h \
    $${FS_DIR}/src/rvoice/fluid_rvoice.h \
    $${FS_DIR}/src/rvoice/fluid_rvoice_event.h \
    $${FS_DIR}/src/rvoice/fluid_rvoice_mixer.h \
    $${FS_DIR}/src/rvoice/fluid_phase.h \
    $${FS_DIR}/src/rvoice/fluid_rev.h \
    $${FS_DIR}/src/synth/fluid_chan.h \
    $${FS_DIR}/src/synth/fluid_event_priv.h \
    $${FS_DIR}/src/synth/fluid_event_queue.h \
    $${FS_DIR}/src/synth/fluid_gen.h \
    $${FS_DIR}/src/synth/fluid_mod.h \
    $${FS_DIR}/src/synth/fluid_synth.h \
    $${FS_DIR}/src/synth/fluid_tuning.h \
    $${FS_DIR}/src/synth/fluid_voice.h \
    $${FS_DIR}/src/midi/fluid_midi.h \
    $${FS_DIR}/src/midi/fluid_midi_router.h \
    $${FS_DIR}/src/drivers/fluid_adriver.h \
    $${FS_DIR}/src/drivers/fluid_mdriver.h \
    $${FS_DIR}/src/bindings/fluid_cmd.h

SOURCES += \
    $${FS_DIR}/src/utils/fluid_conv.c \
    $${FS_DIR}/src/utils/fluid_hash.c \
    $${FS_DIR}/src/utils/fluid_list.c \
    $${FS_DIR}/src/utils/fluid_ringbuffer.c \
    $${FS_DIR}/src/utils/fluid_settings.c \
    $${FS_DIR}/src/utils/fluid_sys.c \
    $${FS_DIR}/src/sfloader/fluid_defsfont.c \
    $${FS_DIR}/src/sfloader/fluid_ramsfont.c \
    $${FS_DIR}/src/rvoice/fluid_adsr_env.c \
    $${FS_DIR}/src/rvoice/fluid_chorus.c \
    $${FS_DIR}/src/rvoice/fluid_iir_filter.c \
    $${FS_DIR}/src/rvoice/fluid_lfo.c \
    $${FS_DIR}/src/rvoice/fluid_rvoice.c \
    $${FS_DIR}/src/rvoice/fluid_rvoice_dsp.c \
    $${FS_DIR}/src/rvoice/fluid_rvoice_event.c \
    $${FS_DIR}/src/rvoice/fluid_rvoice_mixer.c \
    $${FS_DIR}/src/rvoice/fluid_rev.c \
    $${FS_DIR}/src/synth/fluid_chan.c \
    $${FS_DIR}/src/synth/fluid_event.c \
    $${FS_DIR}/src/synth/fluid_gen.c \
    $${FS_DIR}/src/synth/fluid_mod.c \
    $${FS_DIR}/src/synth/fluid_synth.c \
    $${FS_DIR}/src/synth/fluid_tuning.c \
    $${FS_DIR}/src/synth/fluid_voice.c \
    $${FS_DIR}/src/midi/fluid_midi.c \
    $${FS_DIR}/src/midi/fluid_midi_router.c \
    $${FS_DIR}/src/midi/fluid_seqbind.c \
    $${FS_DIR}/src/midi/fluid_seq.c \
    $${FS_DIR}/src/drivers/fluid_adriver.c \
    $${FS_DIR}/src/drivers/fluid_mdriver.c \
    $${FS_DIR}/src/drivers/fluid_aufile.c \
    $${FS_DIR}/src/bindings/fluid_cmd.c \
    $${FS_DIR}/src/bindings/fluid_filerenderer.c
