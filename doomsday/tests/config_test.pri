# Let deployers know this is only a test app.
CONFIG += deng_testapp

include(../config.pri)
include(../dep_core.pri)

macx {
    defineTest(deployTest) {
        contDir = $${OUT_PWD}/$${1}.app/Contents
        fwDir = \"$$contDir/Frameworks\"
        # Quite a hack: directly use the client's Frameworks folder.
        doPostLink("rm -rf $$fwDir")
        doPostLink("ln -s \"$$OUT_PWD/../../client/Doomsday.app/Contents/Frameworks\" $$fwDir")
        doPostLink("ln -sf \"$$OUT_PWD/../../client/Doomsday.app/Contents/PlugIns\" \"$$contDir/\"")
        doPostLink("ln -sf \"$$OUT_PWD/../../client/Doomsday.app/Contents/Resources/qt.conf\" \"$$contDir/Resources/qt.conf\"")

        # Fix the dynamic linker paths so they point to ../Frameworks/ inside the bundle.
        fixInstallName($${1}.app/Contents/MacOS/$${1}, libdeng_core.2.dylib, ..)

        deployPackages($$DENG_PACKAGES, $$OUT_PWD/../..)
        export(QMAKE_BUNDLE_DATA)
        export(dengPacks.files)
        export(dengPacks.path)
    }
}
else:win32 {
    CONFIG += console

    target.path = $$DENG_BIN_DIR

    defineTest(deployTest) {
        INSTALLS += target
        deployPackages($$DENG_PACKAGES, $$OUT_PWD/../..)
        export(INSTALLS)
        export(dengPacks.files)
        export(dengPacks.path)
    }
}
else {
    target.path = $$DENG_BIN_DIR

    defineTest(deployTest) {
        INSTALLS += target
        deployPackages(DENG_PACKAGES, $$OUT_PWD/../..)
        export(INSTALLS)
        export(dengPacks.files)
        export(dengPacks.path)
    }
}
