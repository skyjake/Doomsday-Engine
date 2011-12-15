include(../../dep_glib.pri)

TEMPLATE = lib

CONFIG -= qt
CONFIG += static

DEFINES += FLUIDSYNTH_NOT_A_DLL WITH_FLOAT \
    HAVE_MATH_H HAVE_STDIO_H HAVE_STDLIB_H HAVE_STRING_H

*-g++* {
    QMAKE_CFLAGS += -fomit-frame-pointer -funroll-all-loops -finline-functions
    QMAKE_CFLAGS_WARN_ON -= -Wall -W
}

macx {
    DEFINES += DARWIN
}
unix {
    DEFINES += HAVE_SYS_TYPES_H HAVE_SYS_SOCKET_H HAVE_SYS_TIME_H \
        HAVE_PTHREAD_H HAVE_LIMITS_H HAVE_UNISTD_H HAVE_NETINET_IN_H \
        HAVE_NETINET_TCP_H HAVE_FCNTL_H HAVE_ERRNO_H

    INSTALLS += target fsh headers
    target.path = /usr/local/lib
    fsh.path = /usr/local/include
    headers.path = /usr/local/include/fluidsynth
}

# Sources -------------------------------------------------------------------

INCLUDEPATH += \
    include \
    src \
    src/drivers \
    src/synth \
    src/rvoice \
    src/midi \
    src/utils \
    src/sfloader \
    src/bindings

fsh.files = include/fluidsynth.h

headers.files = \
    include/fluidsynth/audio.h \
    include/fluidsynth/event.h \
    include/fluidsynth/gen.h \
    include/fluidsynth/log.h \
    include/fluidsynth/midi.h \
    include/fluidsynth/misc.h \
    include/fluidsynth/mod.h \
    include/fluidsynth/ramsfont.h \
    include/fluidsynth/seq.h \
    include/fluidsynth/seqbind.h \
    include/fluidsynth/settings.h \
    include/fluidsynth/sfont.h \
    include/fluidsynth/shell.h \
    include/fluidsynth/synth.h \
    include/fluidsynth/types.h \
    include/fluidsynth/voice.h \
    include/fluidsynth/version.h

HEADERS += $$fsh.files $$headers.files

# Internal headers.
HEADERS += \
    src/config.h \
    src/utils/fluid_conv.h \
    src/utils/fluid_hash.h \
    src/utils/fluid_list.h \
    src/utils/fluid_ringbuffer.h \
    src/utils/fluid_settings.h \
    src/utils/fluidsynth_priv.h \
    src/utils/fluid_sys.h \
    src/sfloader/fluid_defsfont.h \
    src/sfloader/fluid_ramsfont.h \
    src/sfloader/fluid_sfont.h \
    src/rvoice/fluid_adsr_env.h \
    src/rvoice/fluid_chorus.h \
    src/rvoice/fluid_iir_filter.h \
    src/rvoice/fluid_lfo.h \
    src/rvoice/fluid_rvoice.h \
    src/rvoice/fluid_rvoice_event.h \
    src/rvoice/fluid_rvoice_mixer.h \
    src/rvoice/fluid_phase.h \
    src/rvoice/fluid_rev.h \
    src/synth/fluid_chan.h \
    src/synth/fluid_event_priv.h \
    src/synth/fluid_event_queue.h \
    src/synth/fluid_gen.h \
    src/synth/fluid_mod.h \
    src/synth/fluid_synth.h \
    src/synth/fluid_tuning.h \
    src/synth/fluid_voice.h \
    src/midi/fluid_midi.h \
    src/midi/fluid_midi_router.h \
    src/drivers/fluid_adriver.h \
    src/drivers/fluid_mdriver.h \
    src/bindings/fluid_cmd.h

SOURCES += \
    src/utils/fluid_conv.c \
    src/utils/fluid_hash.c \
    src/utils/fluid_list.c \
    src/utils/fluid_ringbuffer.c \
    src/utils/fluid_settings.c \
    src/utils/fluid_sys.c \
    src/sfloader/fluid_defsfont.c \
    src/sfloader/fluid_ramsfont.c \
    src/rvoice/fluid_adsr_env.c \
    src/rvoice/fluid_chorus.c \
    src/rvoice/fluid_iir_filter.c \
    src/rvoice/fluid_lfo.c \
    src/rvoice/fluid_rvoice.c \
    src/rvoice/fluid_rvoice_dsp.c \
    src/rvoice/fluid_rvoice_event.c \
    src/rvoice/fluid_rvoice_mixer.c \
    src/rvoice/fluid_rev.c \
    src/synth/fluid_chan.c \
    src/synth/fluid_event.c \
    src/synth/fluid_gen.c \
    src/synth/fluid_mod.c \
    src/synth/fluid_synth.c \
    src/synth/fluid_tuning.c \
    src/synth/fluid_voice.c \
    src/midi/fluid_midi.c \
    src/midi/fluid_midi_router.c \
    src/midi/fluid_seqbind.c \
    src/midi/fluid_seq.c \
    src/drivers/fluid_adriver.c \
    src/drivers/fluid_mdriver.c \
    src/drivers/fluid_aufile.c \
    src/bindings/fluid_cmd.c \
    src/bindings/fluid_filerenderer.c
