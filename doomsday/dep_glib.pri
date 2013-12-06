# Build configuration for glib.
unix {
    # Is the GLib development files installed?
    !system(pkg-config --exists gthread-2.0) {
        error(Missing dependency: FluidSynth requires GLib 2.0 development files)
    }
    
    macx {
        # We assume that glib (and gettext) has been installed with Homebrew.
        GETTEXT_VERSION = $$system(ls /usr/local/Cellar/gettext/ | sort -n | tail -n1)
        LIBS += -L/usr/local/Cellar/gettext/$$GETTEXT_VERSION/lib/
    }

    QMAKE_CFLAGS += $$system(pkg-config --cflags gthread-2.0 glib-2.0)
            LIBS += $$system(pkg-config --libs   gthread-2.0 glib-2.0)
}
