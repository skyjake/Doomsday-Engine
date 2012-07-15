# Build configuration for glib.
unix {
    # Is the GLib development files installed?
    !system(pkg-config --exists gthread-2.0) {
        error(Missing dependency: FluidSynth requires GLib 2.0 development files)
    }

    QMAKE_CFLAGS += $$system(pkg-config --cflags gthread-2.0 glib-2.0)
            LIBS += $$system(pkg-config --libs   gthread-2.0 glib-2.0)
}
