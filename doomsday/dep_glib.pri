# Build configuration for glib.
macx { 
    QMAKE_CFLAGS += $$system(pkg-config --cflags gthread-2.0 glib-2.0)
    LIBS += $$system(pkg-config --libs gthread-2.0 glib-2.0)

    #QMAKE_CFLAGS += -I/usr/local/include/glib-2.0 -I/usr/local/lib/glib-2.0/include
    #LIBS += -L/usr/local/lib -lglib-2.0
}
#unix {
#    QMAKE_CFLAGS += $$system(pkg-config --cflags glib-2.0)
#    LIBS += $$system(pkg-config --libs glib-2.0)
#}

