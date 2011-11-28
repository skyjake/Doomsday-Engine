# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011 Daniel Swanson <danij@dengine.net>
#
# Do not modify this file. Custom CONFIG options can be specified on the 
# qmake command line or in config_user.pri.
#
# NOTE: The PREFIX option should always be specified on the qmake command
#       line, as it is checked before config_user.pri is read.
#
# CONFIG options for Doomsday:
# - deng_32bitonly          Only do a 32-bit build (no 64-bit)
# - deng_aptunstable        Include the unstable apt repository
# - deng_nofixedasm         Disable assembler fixed-point math
# - deng_openal             Build the OpenAL sound driver
# - deng_packres            Package the Doomsday resources
# - deng_rangecheck         Parameter range checking/value assertions
# - deng_snowberry          Include Snowberry in installation
# - deng_snowleopard        (Mac OS X) Use 10.6 SDK
# - deng_writertypecheck    Enable type checking in Writer/Reader

QT -= core gui
CONFIG *= thread

# Directories ----------------------------------------------------------------

DENG_API_DIR = $$PWD/engine/api
DENG_INCLUDE_DIR = $$PWD/engine/portable/include
DENG_UNIX_INCLUDE_DIR = $$PWD/engine/unix/include
DENG_MAC_INCLUDE_DIR = $$PWD/engine/mac/include
DENG_WIN_INCLUDE_DIR = $$PWD/engine/win32/include

# Binaries and generated files are placed here.
DENG_WIN_PRODUCTS_DIR = $$PWD/../distrib/products

# Versions -------------------------------------------------------------------

include(versions.pri)

# Build Options --------------------------------------------------------------

defineTest(echo) {
    !win32 {
        message($$1)
    } else {
        # We don't want to get the printed messages after everything else,
        # so print to stdout.
        system(echo $$1)
    }
}

# Configure for Debug/Release build.
CONFIG(debug, debug|release) {
    echo(Debug build.)
    DEFINES += _DEBUG
    CONFIG += deng_rangecheck
} else {
    echo(Release build.)
    DEFINES += NDEBUG
}

win32 {
    win32-gcc* {
        error("Sorry, gcc is not supported in the Windows build.")
    }

    DEFINES += WIN32 _CRT_SECURE_NO_WARNINGS

    # Library location.
    DENG_EXPORT_LIB = $$OUT_PWD/../engine/doomsday.lib

    # Install locations:
    DENG_BASE_DIR = $$DENG_WIN_PRODUCTS_DIR

    DENG_LIB_DIR = $$DENG_BASE_DIR/bin
    DENG_DATA_DIR = $$DENG_BASE_DIR/data
    DENG_DOCS_DIR = $$DENG_BASE_DIR/doc

    # Tell rc where to get the API headers.
    QMAKE_RC = $$QMAKE_RC /I \"$$DENG_API_DIR\"

    # Also build the OpenAL plugin.
    CONFIG += deng_openal
}
unix {
    # Unix/Mac build options.
    DEFINES += UNIX

    # Ease up on the warnings. (The old C code is a bit messy.)
    QMAKE_CFLAGS_WARN_ON -= -Wall
    QMAKE_CFLAGS_WARN_ON -= -W
}
unix:!macx {
    # Generic Unix build options.
    CONFIG += deng_nofixedasm deng_snowberry deng_packres

    # Choose the apt repository to include in the distribution.
    CONFIG += deng_aptunstable

    # Link against standard math library.
    LIBS += -lm

    # Install prefix.
    isEmpty(PREFIX) {
        PREFIX = /usr
    }

    # Binary location.
    DENG_BIN_DIR = $$PREFIX/bin

    # Library location.
    DENG_LIB_DIR = $$PREFIX/lib

    contains(QMAKE_HOST.arch, x86_64) {
        echo(64-bit architecture detected.)
        DEFINES += HOST_IS_64BIT

        exists($$PREFIX/lib64) {
            DENG_LIB_DIR = $$PREFIX/lib64
        }
        exists($$PREFIX/lib/x86_64-linux-gnu) {
            DENG_LIB_DIR = $$PREFIX/lib/x86_64-linux-gnu
        }
    }

    DENG_BASE_DIR = $$PREFIX/share/doomsday
    DENG_DATA_DIR = $$DENG_BASE_DIR/data

    echo(Binary directory: $$DENG_BIN_DIR)
    echo(Library directory: $$DENG_LIB_DIR)
    echo(Doomsday base directory: $$DENG_BASE_DIR)
}
macx {
    # Mac OS X build options.
    CONFIG += deng_snowleopard deng_nofixedasm

    DEFINES += MACOSX

    # Print include directories and other info.
    #QMAKE_CFLAGS += -Wp,-v

    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}

# Options defined by the user (may not exist).
exists(config_user.pri) {
    include(config_user.pri)
}

# Apply deng_* Configuration -------------------------------------------------

deng_nofixedasm {
    DEFINES += NO_FIXED_ASM
}
!deng_rangecheck {
    DEFINES += NORANGECHECKING
}
macx {
    # Select OS version.
    deng_snowleopard {
        deng_32bitonly {
            echo("Using Mac OS 10.6 SDK (32-bit Intel).")
            QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.6.sdk
            QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
            CONFIG += x86
            CONFIG -= x86_64
        } else {
            echo("Using Mac OS 10.6 SDK (32/64-bit Intel).")
            QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.6.sdk
            QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
            CONFIG += x86 x86_64
        }
    }
    else {
        echo("Using Mac OS 10.4 SDK (32-bit Intel + PowerPC).")
        QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.4u.sdk
        QMAKE_CFLAGS += -mmacosx-version-min=10.4
        DEFINES += MACOS_10_4
        CONFIG += x86 ppc
    }

    # Not using Qt, and anyway these would not point to the chosen SDK.
    QMAKE_INCDIR_QT = ""
    QMAKE_LIBDIR_QT = ""

    defineTest(useFramework) {
        LIBS += -framework $$1
        INCLUDEPATH += $$QMAKE_MAC_SDK/System/Library/Frameworks/$${1}.framework/Headers
        export(LIBS)
        export(INCLUDEPATH)
        return(true)
    }
}
