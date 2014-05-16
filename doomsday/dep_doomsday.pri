# Build configuration for using C/legacy support.
INCLUDEPATH += $$PWD/libdoomsday/include

# Use the appropriate library path (trying some alternatives).
!useLibDir($$OUT_PWD/../libdoomsday) {
    !useLibDir($$OUT_PWD/../../libdoomsday) {
        useLibDir($$OUT_PWD/../../builddir/libdoomsday)
    }
}

LIBS += -ldeng_doomsday

macx {
    defineTest(linkBinaryToBundledLibdoomsday) {
        fixInstallName($${1}, libdeng_doomsday.1.dylib, ..)
    }
    defineTest(linkToBundledLibdoomsday) {
        fixInstallName($${1}.bundle/$$1, libdeng_doomsday.1.dylib, ..)
    }
}
