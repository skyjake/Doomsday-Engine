# Build configuration for using libdeng2.
INCLUDEPATH += $$PWD/libdeng2/include

# Use the appropriate library path.
!useLibDir($$OUT_PWD/../libdeng2) {
    useLibDir($$OUT_PWD/../../libdeng2)
}

LIBS += -ldeng2

# libdeng2 requires the following Qt modules.
QT += core network
