# Build configuration for using libdeng2.
INCLUDEPATH += $$PWD/libdeng2/include

win32 {
    deng_debug {
        btype = "Debug"
    } else {
        btype = "Release"
    }
}

# Use the appropriate library path.
exists($$OUT_PWD/../libdeng2/$$btype): LIBS += -L../libdeng2/$$btype
else:exists($$OUT_PWD/../../libdeng2/$$btype): LIBS += -L../../libdeng2/$$btype

LIBS += -ldeng2

# libdeng2 requires the following Qt modules.
QT += core network
