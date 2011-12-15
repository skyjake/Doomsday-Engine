# Build configuration for glib.
unix {
    QMAKE_CFLAGS += $$system(pkg-config --cflags glib-2.0)
    LIBS += $$system(pkg-config --libs glib-2.0)
}
