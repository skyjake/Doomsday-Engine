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

defineTest(useLibDir) {
    btype = ""
    win32 {
        deng_debug: btype = "/Debug"
              else: btype = "/Release"
    }
    exists($${1}$${btype}) {
        LIBS += -L$${1}$${btype}
        export(LIBS)
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

macx {
    defineTest(removeQtLibPrefix) {
        doPostLink("install_name_tool -change $$[QT_INSTALL_LIBS]/$$2 $$2 $$1")
    }
    defineTest(fixInstallName) {
        # 1: binary file
        # 2: library name
        # 3: path to Frameworks/
        removeQtLibPrefix($$1, $$2)
        doPostLink("install_name_tool -change $$2 @executable_path/$$3/Frameworks/$$2 $$1")
    }
    defineTest(fixPluginInstallId) {
        # 1: target name
        # 2: version
        doPostLink("install_name_tool -id @executable_path/../DengPlugins/$${1}.bundle/Versions/$$2/$$1 $${1}.bundle/Versions/$$2/$$1")
    }
}

defineTest(publicHeaders) {
    # 1: id ("root" for the main include dir)
    # 2: header files
    deng_sdk {
        dir = $$1
        contains(1, root): dir = .
        eval(sdk_headers_$${1}.files += $$2)
        eval(sdk_headers_$${1}.path = $$DENG_SDK_HEADER_DIR/$$dir)
        INSTALLS *= sdk_headers_$$1
        export(INSTALLS)
        export(sdk_headers_$${1}.files)
        export(sdk_headers_$${1}.path)
    }
    HEADERS += $$2
    export(HEADERS)
}
