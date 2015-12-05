# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

include(config_unix_any.pri)

# File locations.
macx-xcode {
    DOOMSDAY_APP_PATH = $$OUT_PWD/../client/Debug/Doomsday.app
}
else {
    exists($$OUT_PWD/../client): DOOMSDAY_APP_PATH = $$OUT_PWD/../client/Doomsday.app
    else: DOOMSDAY_APP_PATH = $$OUT_PWD/../../client/Doomsday.app
}
echo(App path: $$DOOMSDAY_APP_PATH)

DEFINES += MACOSX

CONFIG += deng_nofixedasm deng_embedfluidsynth

deng_qt5 {
    # DisplayMode uses deprecated APIs; OS X fullscreen mode is not compatible with
    # mode changes.
    CONFIG += deng_nodisplaymode
}

# The native SDK option assumes the build is not for distribution.
deng_qtautoselect:!deng_nativesdk {
    contains(QT_VERSION, ^4\\.7\\..*) {
        # 4.7, assume Snow Leopard with 32/64-bit Intel.
        CONFIG += deng_macx6_32bit_64bit
    }
    else:contains(QT_VERSION, ^4\\.8\\..*) {
        # 4.8+, assume Lion and 64-bit Intel.
        CONFIG += deng_macx7_64bit
    }
    else:contains(QT_VERSION, ^5\\.[0-9]\\..*) {
        # 5.0+, assume Mountain Lion and 64-bit Intel.
        CONFIG += deng_macx8_64bit
    }
    else:error(Unsupported Qt version: $$QT_VERSION)
}

# Apply deng_* Configuration -------------------------------------------------

deng_macx10_64bit {
    echo(Using Mac OS 10.10 SDK.)
    CONFIG -= x86
    CONFIG += x86_64
    deng_qt5 {
        QMAKE_MAC_SDK = macosx10.10
    }
    else {
        QMAKE_MAC_SDK = $$system(xcode-select --print-path)/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.10.sdk
    }
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
    QMAKE_CFLAGS += -mmacosx-version-min=10.9
    QMAKE_CXXFLAGS += -mmacosx-version-min=10.9
    DEFINES += MACOS_10_7

    *-clang* {
        # Ignore warnings from Qt headers.
        QMAKE_CXXFLAGS_WARN_ON += -Wno-c++11-long-long -Wno-unused-private-field
    }
}
else:deng_macx8_64bit {
    echo(Using Mac OS 10.8 SDK.)
    CONFIG -= x86
    CONFIG += x86_64
    deng_qt5 {
        QMAKE_MAC_SDK = macosx
    }
    else {
        QMAKE_MAC_SDK = $$system(xcode-select --print-path)/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.8.sdk
    }
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7
    QMAKE_CFLAGS += -mmacosx-version-min=10.7
    QMAKE_CXXFLAGS += -mmacosx-version-min=10.7
    DEFINES += MACOS_10_7

    *-clang* {
        # Ignore warnings from Qt headers.
        QMAKE_CXXFLAGS_WARN_ON += -Wno-c++11-long-long -Wno-unused-private-field
    }
}
else:deng_macx7_64bit {
    echo(Using Mac OS 10.7 SDK.)
    CONFIG -= x86
    CONFIG += x86_64
    QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.7.sdk
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
    QMAKE_CFLAGS += -mmacosx-version-min=10.6
    QMAKE_CXXFLAGS += -mmacosx-version-min=10.6
}
else:deng_macx6_32bit_64bit {
    echo(Using Mac OS 10.6 SDK.)
    CONFIG += x86 x86_64
    QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.6.sdk
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5
    QMAKE_CFLAGS += -mmacosx-version-min=10.5
    QMAKE_CXXFLAGS += -mmacosx-version-min=10.5
}
else:deng_nativesdk {
    echo(Using native SDK.)
    DEFINES += MACOSX_NATIVESDK
}
else {
    error(Unspecified SDK configuration.)
}

# Adjust Qt paths as needed.
!deng_macx8_64bit:!deng_qt5 {
    qtbase = $$(QTDIR)
    isEmpty(qtbase):!isEmpty(QMAKE_MAC_SDK) {
        QMAKE_INCDIR_QT = $${QMAKE_MAC_SDK}$$QMAKE_INCDIR_QT
        QMAKE_LIBDIR_QT = $${QMAKE_MAC_SDK}$$QMAKE_LIBDIR_QT
    }
}

# What's our arch?
archs = "Architectures:"
   x86: archs += intel32
x86_64: archs += intel64
echo($$archs)

deng_c++11 {
    echo(C++11 enabled (using libc++).)
    QMAKE_OBJECTIVE_CFLAGS += -stdlib=libc++
    QMAKE_CXXFLAGS += -std=c++11 -stdlib=libc++
    QMAKE_CXXFLAGS_WARN_ON += -Wno-deprecated-register
    QMAKE_LFLAGS += -stdlib=libc++
}

# Add the bundled Frameworks to the rpath.
QMAKE_LFLAGS += -Wl,-rpath,@loader_path/../Frameworks

# Xcode project settings.
*-xcode {
    xcHeaderMap.name     = USE_HEADERMAP
    xcHeaderMap.value    = NO
    xcHidden.name        = GCC_SYMBOLS_PRIVATE_EXTERN
    xcHidden.value       = NO
    xcInlineHidden.name  = GCC_INLINES_ARE_PRIVATE_EXTERN
    xcInlineHidden.value = NO
    QMAKE_MAC_XCODE_SETTINGS += xcHeaderMap xcHidden xcInlineHidden

    # Place all built binaries in the products folder.
    DESTDIR = $$DENG_ROOT_DIR/../distrib/products

    # Allow using libraries from the products folder.
    LIBS += -L$$DESTDIR
}

defineTest(xcodeFinalizeBuild) {
    # 1 - name of target
    *-xcode {
        doPostLink("cd $TARGET_BUILD_DIR")
        versionComponents = $$split(VERSION, .)
        major = $$first(versionComponents)
        doPostLink("ln -fs lib$${1}.dylib lib$${1}.$${major}.dylib")
        doPostLink("ln -fs lib$${1}.dylib $$DESTDIR/lib$${1}.$${major}.dylib")
    }
}

defineTest(xcodeFinalizeAppBuild) {
    *-xcode {
        doPostLink("cd $TARGET_BUILD_DIR")
        doPostLink(export PATH=\"$$[QT_HOST_BINS]:$PATH\")
    }
}

defineTest(xcodeDeployDengPlugins) {
    *-xcode {
        QMAKE_BUNDLE_DATA += macx_plugins
        
        for(name, 1) {
            macx_plugins.files += $$DESTDIR/$${name}.bundle
        }
        macx_plugins.path = Contents/DengPlugins
        
        export(QMAKE_BUNDLE_DATA)
        export(macx_plugins.files)
        export(macx_plugins.path)        
    }
}

# Macros ---------------------------------------------------------------------

defineTest(useFramework) {
    LIBS += -framework $$1
    INCLUDEPATH += $$QMAKE_MAC_SDK/System/Library/Frameworks/$${1}.framework/Headers
    export(LIBS)
    export(INCLUDEPATH)
    return(true)
}