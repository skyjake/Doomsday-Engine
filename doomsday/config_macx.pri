# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

include(config_unix_any.pri)

DEFINES += MACOSX

CONFIG += deng_nofixedasm

# The native SDK option assumes the build is not for distribution.
!deng_nativesdk {
    contains(QT_VERSION, ^4\\.[0-6]\\..*) {
        # 4.6 or earlier, assume Tiger with 32-bit Universal Intel/PowerPC binaries.
        CONFIG += deng_macx4u_32bit
    }
    else {
        # 4.7 or newer, assume Snow Leopard with 64-bit only Intel.
        CONFIG += deng_macx6_64bit
    }
}

QMAKE_LFLAGS += -flat_namespace -undefined suppress

# Apply deng_* Configuration -------------------------------------------------

# Three options:
# - deng_nativesdk
# - deng_macx4u_32bit
# - deng_macx6_64bit

deng_nativesdk {
    echo(Using native SDK.)
}
else:deng_macx6_64bit {
    echo(Using 64-bit only 10.6+ SDK.)
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
    QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.6.sdk
    INCLUDEPATH = $$QMAKE_MAC_SDK/usr/X11/include $$INCLUDEPATH
    QMAKE_INCDIR_QT = $${QMAKE_MAC_SDK}$$QMAKE_INCDIR_QT
}
else:deng_macx4u_32bit {
    error("32-bit 10.4+ SDK still wip.")
}
else {
    error(Unspecified SDK configuration.)
}

#deng_nativesdk {
#    echo("Using SDK for your Mac OS version.")
#   QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
#}
#else:deng_snowleopard {
#    echo("Using Mac OS 10.6 SDK.")
#    QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.6.sdk
#    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
#    CONFIG += x86 x86_64
#
#    deng_32bitonly {
#        CONFIG -= x86_64
#        QMAKE_CFLAGS_X86_64 = ""
#        QMAKE_OBJECTIVE_CFLAGS_X86_64 = ""
#        QMAKE_LFLAGS_X86_64 = ""
#    }
#}
#else {
#    echo("Using Mac OS 10.4 SDK.")
#    echo("Architectures: 32-bit Intel + 32-bit PowerPC.")
#    QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.4u.sdk
#    QMAKE_CFLAGS += -mmacosx-version-min=10.4
#    DEFINES += MACOS_10_4
#    CONFIG += x86 ppc
#}

#!deng_nativesdk {
#    # Not using Qt, and anyway these would not point to the chosen SDK.
#    QMAKE_INCDIR_QT = ""
#    QMAKE_LIBDIR_QT = ""
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
