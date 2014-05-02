# Let deployers know this is only a test app.
CONFIG += deng_testapp

include(../config.pri)
include(../dep_core.pri)

mod.files = \
    $$DENG_MODULES_DIR/Config.de \
    $$DENG_MODULES_DIR/recutil.de

macx {
    mod.path = Contents/Resources/modules

    defineTest(deployTest) {
        contDir = $${OUT_PWD}/$${1}.app/Contents
        fwDir = \"$$contDir/Frameworks\"
        # Quite a hack: directly use the client's Frameworks folder.
        doPostLink("rm -rf $$fwDir")
        doPostLink("ln -s \"$$OUT_PWD/../../client/Doomsday.app/Contents/Frameworks\" $$fwDir")
        doPostLink("ln -sf \"$$OUT_PWD/../../client/Doomsday.app/Contents/PlugIns\" \"$$contDir/PlugIns\"")
        doPostLink("ln -sf \"$$OUT_PWD/../../client/Doomsday.app/Contents/Resources/qt.conf\" \"$$contDir/Resources/qt.conf\"")

        # Fix the dynamic linker paths so they point to ../Frameworks/ inside the bundle.
        fixInstallName($${1}.app/Contents/MacOS/$${1}, libdeng_core.2.dylib, ..)

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
