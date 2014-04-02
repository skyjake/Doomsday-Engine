# The Doomsday Engine Project
# Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# qmake .pri file for projects using the Doomsday SDK.
#
# The Doomsday SDK is distributed under the GNU Lesser General Public
# License version 3 (or, at your option, any later version). Please
# visit http://www.gnu.org/licenses/lgpl.html for details.
#
# Variables:
# - DENG_CONFIG     Names of supporting libraries to use (gui, appfw, shell);
#                   symlink: deploy libs as symbolic links

DENG_SDK_DIR = $$PWD

isEmpty(PREFIX): PREFIX = $$OUT_PWD

exists($$DENG_SDK_DIR/include/doomsday) {
    INCLUDEPATH += $$DENG_SDK_DIR/include/doomsday
}
else {
    INCLUDEPATH += $$DENG_SDK_DIR/include
}

win32: LIBS += -L$$DENG_SDK_DIR/lib
 else: QMAKE_LFLAGS = -L$$DENG_SDK_DIR/lib $$QMAKE_LFLAGS

# The core library is always required.
LIBS += -ldeng2

contains(DENG_CONFIG, appfw): DENG_CONFIG *= gui shell

# Supporting libraries are optional.
contains(DENG_CONFIG, gui):   LIBS += -ldeng_gui
contains(DENG_CONFIG, appfw): LIBS += -ldeng_appfw
contains(DENG_CONFIG, shell): LIBS += -ldeng_shell

defineTest(dengDynamicLinkPath) {
    # 1: path to search dynamic libraries from
    *-g++*|*-gcc*|*-clang* {
        QMAKE_LFLAGS += -Wl,-rpath,$$1
        export(QMAKE_LFLAGS)
    }
}

# Instruct the dynamic linker to load the libs from the SDK lib dir.
dengDynamicLinkPath($$DENG_SDK_DIR/lib)

macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7
}

DENG_MODULES = $$DENG_SDK_DIR/modules/*.de

# Macros ---------------------------------------------------------------------

defineTest(dengPostLink) {
    isEmpty(QMAKE_POST_LINK) {
        QMAKE_POST_LINK = $$1
    } else {
        QMAKE_POST_LINK = $$QMAKE_POST_LINK && $$1
    }
    export(QMAKE_POST_LINK)
}

defineTest(dengClear) {
    # 1: file to remove
    win32: system(del /q \"$$1\")
     else: system(rm -f \"$$1\")
}

defineTest(dengPack) {
    # 1: path of a .pack file
    # 2: actual root directory
    # 3: files to include, relative to the root
    system(cd \"$$2\" && zip -r \"$$1\" $$3 -x \\*~)
}

defineTest(dengPackModules) {
    # 1: path of a .pack file
    dengPack($$1, $$DENG_SDK_DIR, modules/*.de)
}

defineReplace(dengFindLibDir) {
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

defineReplace(dengSdkLib) {
    # 1: name of library
    win32: fn = $$DENG_SDK_DIR/lib/$${1}.dll
    else:macx {
        versions = 2 1 0
        for(vers, versions) {
            fn = $$DENG_SDK_DIR/lib/lib$${1}.$${vers}.dylib
            exists($$fn): return($$fn)
        }
    }
    else {
        fn = $$DENG_SDK_DIR/lib/lib$${1}.so
        # Apply the latest major version.
        exists($${fn}.2):      fn = $${fn}.2
        else:exists($${fn}.1): fn = $${fn}.1
        else:exists($${fn}.0): fn = $${fn}.0
    }
    return($$fn)
}

defineTest(dengDeploy) {
    # 1: app name
    # 2: install prefix
    # 3: base pack file
    prefix = $$2
    INSTALLS += target basepack denglibs
    basepack.files = $$3

    denglibs.files = $$dengSdkLib(deng2)
    contains(DENG_CONFIG, gui):   denglibs.files += $$dengSdkLib(deng_gui)
    contains(DENG_CONFIG, appfw): denglibs.files += $$dengSdkLib(deng_appfw)
    contains(DENG_CONFIG, shell): denglibs.files += $$dengSdkLib(deng_shell)

    win32 {
    }
    else:macx {
        QMAKE_BUNDLE_DATA += $$INSTALLS
        QMAKE_BUNDLE_DATA -= target
        basepack.path = Contents/Resources
        denglibs.path = Contents/Frameworks
    }
    else {
        target.path   = $$prefix/bin
        basepack.path = $$prefix/share/$${1}
        denglibs.path = $$dengFindLibDir($$prefix)
    }

    contains(DENG_CONFIG, symlink):unix {
        # Symlink the libraries rather than copy.
        macx {
            QMAKE_BUNDLE_DATA -= denglibs
            fwDir = $${TARGET}.app/$$denglibs.path
        }
        else {
            INSTALLS -= denglibs
            fwDir = $$denglibs.path
        }
        for(fn, denglibs.files) {
            dengPostLink(mkdir -p \"$$fwDir\" && ln -sf \"$$fn\" \"$$fwDir\")
        }
    }

    macx: export(QMAKE_BUNDLE_DATA)
    else {
        export(INSTALLS)
        export(target.path)
    }
    export(basepack.files)
    export(basepack.path)
    export(denglibs.files)
    export(denglibs.path)
}
