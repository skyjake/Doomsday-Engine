# Let deployers know this is only a test app.
CONFIG += deng_testapp

include(../config.pri)
include(../dep_core.pri)

macx {
    defineTest(deployTest) {
        # Fix the dynamic linker paths so they point to ../Frameworks/ inside the bundle.
        fixInstallName($${1}.app/Contents/MacOS/$${1}, libdeng_core.2.dylib, ..)

        deployPackages($$DENG_PACKAGES, $$OUT_PWD/../..)
        export(QMAKE_BUNDLE_DATA)
        export(dengPacks.files)
        export(dengPacks.path)
    }

    # In an SDK build, we can access the libraries directly from the built SDK.
    deng_sdk {
        QMAKE_LFLAGS += \
            -Wl,-rpath,$$DENG_SDK_LIB_DIR \
            -Wl,-rpath,$$[QT_INSTALL_LIBS]
    }
}
else:win32 {
    CONFIG += console

    target.path = $$DENG_BIN_DIR

    defineTest(deployTest) {        
        deployPackages($$DENG_PACKAGES, $$OUT_PWD/../..)
        export(INSTALLS)
        export(dengPacks.files)
        export(dengPacks.path)
    }
}
else {
    target.path = $$DENG_BIN_DIR

    defineTest(deployTest) {
        deployPackages($$DENG_PACKAGES, $$OUT_PWD/../..)
        export(INSTALLS)
        export(dengPacks.files)
        export(dengPacks.path)
    }
}

deployTarget()
