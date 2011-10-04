# Build configuration for using libdeng2.
INCLUDEPATH += $$PWD/libdeng2/include

# Use the appropriate library path.
exists($$OUT_PWD/../libdeng2): LIBS += -L../libdeng2
else:exists($$OUT_PWD/../../libdeng2): LIBS += -L../../libdeng2

LIBS += -ldeng2

# libdeng2 requires the following Qt modules.
QT += core network
