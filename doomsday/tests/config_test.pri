include(../config.pri)
include(../dep_deng2.pri)

mod.files = \
    $$DENG_MODULES_DIR/Config.de \
    $$DENG_MODULES_DIR/recutil.de

macx {
    mod.path = Contents/Resources/modules

    defineTest(deployTest) {
        # Arg 1: target name (without .app)
        fwDir = \"$${OUT_PWD}/$${1}.app/Contents/Frameworks/\"
        doPostLink("rm -rf $$fwDir")
        doPostLink("mkdir $$fwDir")
        doPostLink("ln -s $$OUT_PWD/../../libdeng2/libdeng2.dylib $$fwDir/libdeng2.dylib")
        doPostLink("ln -s $$OUT_PWD/../../libdeng2/libdeng2.2.dylib $$fwDir/libdeng2.2.dylib")

        # Fix the dynamic linker paths so they point to ../Frameworks/ inside the bundle.
        fixInstallName($${1}.app/Contents/MacOS/$${1}, libdeng2.2.dylib, ..)

        QMAKE_BUNDLE_DATA += mod
        export(QMAKE_BUNDLE_DATA)
    }
}
else:win32 {
    CONFIG += console

    target.path = $$DENG_BIN_DIR
    mod.path = $$DENG_DATA_DIR/modules

    defineTest(deployTest) {
        INSTALLS += mod target
        export(INSTALLS)
    }
}
else {
    target.path = $$DENG_BIN_DIR
    mod.path = $$DENG_DATA_DIR/modules

    defineTest(deployTest) {
        INSTALLS += mod target
        export(INSTALLS)
    }
}
