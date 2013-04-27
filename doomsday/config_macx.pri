# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

include(config_unix_any.pri)

DEFINES += MACOSX

CONFIG += deng_nofixedasm deng_embedfluidsynth

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
    else:contains(QT_VERSION, ^5\\.[0-1]\\..*) {
        # 5.0+, assume Mountain Lion and 64-bit Intel.
        CONFIG += deng_macx8_64bit
    }
    else:error(Unsupported Qt version: $$QT_VERSION)
}

# Apply deng_* Configuration -------------------------------------------------

deng_macx8_64bit {
    echo(Using Mac OS 10.8 SDK.)
    CONFIG -= x86
    CONFIG += x86_64
    QMAKE_MAC_SDK = $$system(xcode-select --print-path)/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.8.sdk
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7
    QMAKE_CFLAGS += -mmacosx-version-min=10.7
    QMAKE_CXXFLAGS += -mmacosx-version-min=10.7
    DEFINES += MACOS_10_7

    *-clang* {
        # Ignore warnings from Qt headers.
        QMAKE_CXXFLAGS += -Wno-c++11-long-long -Wno-unused-private-field
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
    INCLUDEPATH = $$QMAKE_MAC_SDK/usr/X11/include $$INCLUDEPATH
}
else:deng_macx6_32bit_64bit {
    echo(Using Mac OS 10.6 SDK.)
    CONFIG += x86 x86_64
    QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.6.sdk
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5
    QMAKE_CFLAGS += -mmacosx-version-min=10.5
    QMAKE_CXXFLAGS += -mmacosx-version-min=10.5
    INCLUDEPATH = $$QMAKE_MAC_SDK/usr/X11/include $$INCLUDEPATH
}
else:deng_nativesdk {
    echo(Using native SDK.)
    DEFINES += MACOSX_NATIVESDK
}
else {
    error(Unspecified SDK configuration.)
}

# Adjust Qt paths as needed.
!deng_macx8_64bit {
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

# Macros ---------------------------------------------------------------------

defineTest(useFramework) {
    LIBS += -framework $$1
    INCLUDEPATH += $$QMAKE_MAC_SDK/System/Library/Frameworks/$${1}.framework/Headers
    export(LIBS)
    export(INCLUDEPATH)
    return(true)
}
