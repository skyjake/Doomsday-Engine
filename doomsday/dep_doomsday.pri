# Build configuration for using C/legacy support.
INCLUDEPATH += $$PWD/libdoomsday/include

# Use the appropriate library path (trying some alternatives).
!useLibDir($$OUT_PWD/../libdoomsday) {
    !useLibDir($$OUT_PWD/../../libdoomsday) {
        useLibDir($$OUT_PWD/../../builddir/libdoomsday)
    }
}

LIBS += -ldoomsday

macx {
    defineTest(linkBinaryToBundledLibdoomsday) {
        fixInstallName($${1}, libdoomsday.1.dylib, ..)
    }
    defineTest(linkToBundledLibdoomsday) {
        fixInstallName($${1}.bundle/$$1, libdoomsday.1.dylib, ..)
    }
}
