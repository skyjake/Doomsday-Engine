# The Doomsday Engine Project
# Copyright (c) 2011-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

defineTest(runPython2) {
    win32: system(python $$1)       # 2.7 still expected
     else: system(/usr/bin/env python2.7 $$1)
}

defineTest(runPython2InDir) {
    win32: system(cd "$$1" && python $$2)
     else: system(cd "$$1" && /usr/bin/env python2.7 $$2)
}

defineTest(echo) {
    deng_verbosebuildconfig {
        !win32 {
            message($$1)
        } else {
            # We don't want to get the printed messages after everything else,
            # so print to stdout.
            system(echo $$1)
        }
    }
}

defineReplace(findLibDir) {
    # Determines the appropriate library directory given prefix $$1
    prefix = $$1
    dir = $$prefix/lib
    contains(QMAKE_HOST.arch, x86_64) {
        exists($$prefix/lib64) {
            dir = $$prefix/lib64
        }
        exists($$prefix/lib/x86_64-linux-gnu) {
            dir = $$prefix/lib/x86_64-linux-gnu
        }
    }
    return($$dir)
}

defineTest(useLibDir) {
    btype = ""
    win32 {
        deng_debug: btype = "/Debug"
              else: btype = "/Release"
    }
    exists($${1}$${btype}) {
        win32 {
            LIBS += -L$${1}$${btype}
            export(LIBS)
        }
        else {
            # Specify this library directory first to ensure it overrides the
            # system library directory.
            QMAKE_LFLAGS = -L$${1}$${btype} $$QMAKE_LFLAGS
            export(QMAKE_LFLAGS)
        }
        return(true)
    }
    return(false)
}

defineTest(doPostLink) {
    isEmpty(QMAKE_POST_LINK) {
        QMAKE_POST_LINK = $$1
    } else {
        QMAKE_POST_LINK = $$QMAKE_POST_LINK && $$1
    }
    export(QMAKE_POST_LINK)
}

defineTest(buildPackage) {
    # 1: path of a .pack source directory
    # 2: target directory where the zipped .pack is written
    runPython2($$PWD/../build/scripts/buildpackage.py \"$${1}.pack\" \"$$2\")

    # Automatically deploy the package.
    builtpacks.files += $$2/$${1}.pack
    export(builtpacks.files)
}

defineTest(deployTarget) {
    unix:!macx {
        INSTALLS += target
        target.path = $$DENG_BIN_DIR
        export(INSTALLS)
        export(target.path)
    }
    else:win32 {
        DESTDIR = $$DENG_BIN_DIR
        export(DESTDIR)
    }
}

defineTest(deployLibrary) {
    win32:!deng_sdk {
        DLLDESTDIR = $$DENG_LIB_DIR
        export(DLLDESTDIR)
    }
    unix:!macx {
        INSTALLS += target
        target.path = $$DENG_LIB_DIR
        export(target.path)
    }
    deng_sdk {
        INSTALLS *= target
        target.path = $$DENG_SDK_LIB_DIR
        export(target.path)
        win32 {
            INSTALLS *= targetlib
            deng_debug: targetlib.files = $$OUT_PWD/Debug/$${TARGET}.lib
                  else: targetlib.files = $$OUT_PWD/Release/$${TARGET}.lib
            targetlib.path = $$DENG_SDK_LIB_DIR
            export(targetlib.files)
            export(targetlib.path)
        }
        INSTALLS *= builtpacks
    }
    export(INSTALLS)
}

macx {
    defineTest(removeQtLibPrefix) {
        doPostLink("install_name_tool -change $$[QT_INSTALL_LIBS]/$$2 $$2 $$1")
    }
    defineTest(fixInstallName) {
        # 1: binary file
        # 2: library name
        # 3: path to Frameworks/
        removeQtLibPrefix($$1, $$2)
        doPostLink("install_name_tool -change $$2 @rpath/$$2 $$1")
    }
    defineTest(fixPluginInstallId) {
        # 1: target name
        # 2: version
        doPostLink("install_name_tool -id @executable_path/../DengPlugins/$${1}.bundle/Versions/$$2/$$1 $${1}.bundle/Versions/$$2/$$1")
    }
}

defineTest(publicSubHeaders) {
    # 1: id
    # 2: "root" for the main include dir
    # 3: header files
    deng_sdk {
        ident = $$1
        dir = $$2
        contains(1, root): dir = .
        eval(sdk_headers_$${1}.files += $$3)
        eval(sdk_headers_$${1}.path = $$DENG_SDK_HEADER_DIR/$$dir)
        INSTALLS *= sdk_headers_$$1
        export(INSTALLS)
        export(sdk_headers_$${1}.files)
        export(sdk_headers_$${1}.path)
    }
    HEADERS += $$3
    export(HEADERS)
}

defineTest(publicHeaders) {
    # 1: id/root dir
    # 2: header files
    publicSubHeaders($$1, $$1, $$2)
    export(HEADERS)
    deng_sdk {
        export(INSTALLS)
        export(sdk_headers_$${1}.files)
        export(sdk_headers_$${1}.path)
    }
}

defineTest(deployPackages) {
    # 1: list of package identifiers
    # 2: path where packages are installed from
    for(pkg, 1) {
        fn = "$$2/$$pkg"
        exists($$fn): dengPacks.files += $$fn
    }
    macx {
        dengPacks.path = Contents/Resources
        QMAKE_BUNDLE_DATA += dengPacks
        export(QMAKE_BUNDLE_DATA)
    }
    else {
        dengPacks.path = $$DENG_DATA_DIR
        INSTALLS += dengPacks
        export(INSTALLS)
    }
    export(dengPacks.files)
    export(dengPacks.path)
}
