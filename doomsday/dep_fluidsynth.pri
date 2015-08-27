# Build configuration for libfluidsynth.
unix {
    # Is the FluidSynth development files installed?
    system($$PKG_CONFIG --exists fluidsynth) {
        flags = $$system($$PKG_CONFIG --cflags fluidsynth)
        QMAKE_CFLAGS += $$flags
        QMAKE_CXXFLAGS += $$flags
        LIBS += $$system($$PKG_CONFIG --libs fluidsynth)
    }
}
