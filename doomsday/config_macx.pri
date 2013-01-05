# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

include(config_unix_any.pri)

DEFINES += MACOSX

CONFIG += deng_nofixedasm deng_embedfluidsynth

# The native SDK option assumes the build is not for distribution.
!deng_nativesdk {
    contains(QT_VERSION, ^4\\.[0-6]\\..*) {
        # 4.6 or earlier, assume Tiger with 32-bit Universal Intel/PowerPC binaries.
        CONFIG += deng_macx4u_32bit
    }
    else:contains(QT_VERSION, ^4\\.7\\..*) {
        # 4.7, assume Snow Leopard with 32/64-bit Intel.
        CONFIG += deng_macx6_32bit_64bit
    }
    else:contains(QT_VERSION, ^4\\.8\\..*) {
        # 4.8+, assume Lion and 64-bit Intel.
        CONFIG += deng_macx7_64bit
    }
    else:error(Unsupported Qt version: $$QT_VERSION)
}

# Apply deng_* Configuration -------------------------------------------------

# Three options:
# - deng_nativesdk
# - deng_macx4u_32bit
# - deng_macx6_64bit

deng_nativesdk {
    echo(Using native SDK.)
    #QMAKE_MAC_SDK = $$(SDKROOT)
    #isEmpty(QMAKE_MAC_SDK) {
    #    error(Set the SDKROOT environment variable to specify which Mac OS X SDK to use.)
    #}
    DEFINES += MACOSX_NATIVESDK
    #QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.8
    #QMAKE_CFLAGS += -mmacosx-version-min=10.8
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
else:deng_macx4u_32bit {
    echo(Using Mac OS 10.4u SDK.)
    CONFIG += x86 ppc
    QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.4u.sdk
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.4
    QMAKE_CFLAGS += -mmacosx-version-min=10.4
    INCLUDEPATH = $$QMAKE_MAC_SDK/usr/X11R6/include $$INCLUDEPATH
    DEFINES += MACOS_10_4

    QMAKE_CFLAGS_WARN_ON -= -fdiagnostics-show-option
}
else {
    error(Unspecified SDK configuration.)
}

# Adjust Qt paths as needed.
qtbase = $$(QTDIR)
isEmpty(qtbase):!isEmpty(QMAKE_MAC_SDK) {
    QMAKE_INCDIR_QT = $${QMAKE_MAC_SDK}$$QMAKE_INCDIR_QT
    QMAKE_LIBDIR_QT = $${QMAKE_MAC_SDK}$$QMAKE_LIBDIR_QT
}

#deng_32bitonly {
#    CONFIG += x86
#    CONFIG -= x86_64
#    QMAKE_CFLAGS_X86_64 = ""
#    QMAKE_CXXFLAGS_X86_64 = ""
#    QMAKE_OBJECTIVE_CFLAGS_X86_64 = ""
#    QMAKE_LFLAGS_X86_64 = ""
#}

# What's our arch?
archs = "Architectures:"
ppc: archs += ppc32
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
