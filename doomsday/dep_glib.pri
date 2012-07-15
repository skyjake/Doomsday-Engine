# Build configuration for glib.
unix {
    QMAKE_CFLAGS += $$system(pkg-config --cflags gthread-2.0 glib-2.0)
            LIBS += $$system(pkg-config --libs   gthread-2.0 glib-2.0)
}

