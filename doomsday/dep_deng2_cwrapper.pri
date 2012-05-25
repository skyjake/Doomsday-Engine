# Build configuration for using the libdeng2 C wrapper.
INCLUDEPATH += $$PWD/libdeng2/include

# Use the appropriate library path.
!useLibDir($$OUT_PWD/../libdeng2) {
    !useLibDir($$OUT_PWD/../../libdeng2) {
	useLibDir($$OUT_PWD/../../builddir/libdeng2)
    }
}

LIBS += -ldeng2

macx {
    defineTest(fixInstallName) {
        # 1: binary file
        # 2: library name
        # 3: path to Frameworks/
        doPostLink("install_name_tool -change $$2 @executable_path/$$3/Frameworks/$$2 $$1")
    }
    defineTest(linkToBundledLibdeng2) {
        fixInstallName($${1}.bundle/$$1, libdeng2.2.dylib, ..)
    }
}
