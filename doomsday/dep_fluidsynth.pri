# Build configuration for libfluidsynth.
unix {
    # Is the FluidSynth development files installed?
    system(pkg-config --exists fluidsynth) {
        flags = $$system(pkg-config --cflags fluidsynth)
        QMAKE_CFLAGS += $$flags
        QMAKE_CXXFLAGS += $$flags
        LIBS += $$system(pkg-config --libs fluidsynth)
    }
}
