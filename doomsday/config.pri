# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>
#
# Do not modify this file. Custom CONFIG options can be specified on the 
# qmake command line or in config_user.pri.
#
# NOTE: The PREFIX option should always be specified on the qmake command
#       line, as it is checked before config_user.pri is read.
#
# User-definable variables:
#   PREFIX          Install prefix for Unix (specify on qmake command line)
#   SCRIPT_PYTHON   Path of the Python interpreter binary to be used in
#                   generated scripts (python on path used for building)
#
# CONFIG options for Doomsday:
# - deng_32bitonly          Only do a 32-bit build (no 64-bit)
# - deng_aptstable          Include the stable apt repository .list
# - deng_aptunstable        Include the unstable apt repository .list
# - deng_fmod               Build the FMOD Ex sound driver
# - deng_nativesdk          (Mac OS X) Use the current OS's SDK
# - deng_nofixedasm         Disable assembler fixed-point math
# - deng_nosdlmixer         Disable SDL_mixer; use dummy driver as default
# - deng_nosnowberry        (Unix) Exclude Snowberry from installation
# - deng_openal             Build the OpenAL sound driver
# - deng_nopackres          Do not package the Doomsday resources
# - deng_rangecheck         Parameter range checking/value assertions
# - deng_snowberry          Include Snowberry in installation
# - deng_snowleopard        (Mac OS X) Use 10.6 SDK
# - deng_writertypecheck    Enable type checking in Writer/Reader
#
# Read-only options (set automatically):
# - deng_debug              Debug build.

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

# Macros ---------------------------------------------------------------------

defineTest(echo) {
    !win32 {
        message($$1)
    } else {
        # We don't want to get the printed messages after everything else,
        # so print to stdout.
        system(echo $$1)
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

# Build Options --------------------------------------------------------------

# Configure for Debug/Release build.
CONFIG(debug, debug|release) {
    !win32: echo(Debug build.)
    DEFINES += _DEBUG
    CONFIG += deng_debug deng_rangecheck
} else {
    !win32: echo(Release build.)
    DEFINES += NDEBUG
}

# Check for a 64-bit compiler.
contains(QMAKE_HOST.arch, x86_64) {
    echo(64-bit architecture detected.)
    DEFINES += HOST_IS_64BIT
}

win32 {
    win32-g++* {
        error("Sorry, gcc is not supported in the Windows build.")
    }

    DEFINES += WIN32 _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES

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
    QMAKE_CFLAGS_WARN_ON += -Werror-implicit-function-declaration
}
unix:!macx {
    # Generic Unix build options.
    CONFIG += deng_nofixedasm
    !deng_nosnowberry: CONFIG += deng_snowberry

    exists(/etc/apt) {
        # Choose the apt repository to include in the distribution.
        isStableRelease(): CONFIG += deng_aptstable
                     else: CONFIG += deng_aptunstable
    }

    # Link against standard math library.
    LIBS += -lm

    # Install prefix.
    isEmpty(PREFIX) {
        freebsd-*: PREFIX = /usr/local
             else: PREFIX = /usr
    }

    # Binary location.
    DENG_BIN_DIR = $$PREFIX/bin

    # Library location.
    DENG_LIB_DIR = $$PREFIX/lib

    contains(QMAKE_HOST.arch, x86_64) {
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
    CONFIG += deng_nativesdk deng_nofixedasm

    DEFINES += MACOSX

    # Print include directories and other info.
    #QMAKE_CFLAGS += -Wp,-v

    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}

# Options defined by the user (may not exist).
exists(config_user.pri) {
    include(config_user.pri)
}

# System Tools ---------------------------------------------------------------

unix:!macx {
    # Python to be used in generated scripts.
    isEmpty(SCRIPT_PYTHON) {
        exists(/usr/bin/python): SCRIPT_PYTHON = /usr/bin/python
        exists(/usr/local/bin/python): SCRIPT_PYTHON = /usr/local/bin/python
    }
    isEmpty(SCRIPT_PYTHON) {
        # Check the system path.
        SCRIPT_PYTHON = $$system(which python)
    }
}

# Apply deng_* Configuration -------------------------------------------------

isStableRelease(): DEFINES += DENG_STABLE

deng_nofixedasm {
    DEFINES += DENG_NO_FIXED_ASM
}
!deng_rangecheck {
    DEFINES += DENG_NO_RANGECHECKING
}
deng_nosdlmixer {
    DEFINES += DENG_DISABLE_SDLMIXER
}
macx {
    # Select OS version.
    deng_nativesdk {
        echo("Using SDK for your Mac OS version.")
        QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
    }
    else:deng_snowleopard {
        echo("Using Mac OS 10.6 SDK.")
        QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.6.sdk
        QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
        CONFIG += x86 x86_64

        deng_32bitonly {
            CONFIG -= x86_64
            QMAKE_CFLAGS_X86_64 = ""
            QMAKE_OBJECTIVE_CFLAGS_X86_64 = ""
            QMAKE_LFLAGS_X86_64 = ""
        }
    }
    else {
        echo("Using Mac OS 10.4 SDK.")
        echo("Architectures: 32-bit Intel + 32-bit PowerPC.")
        QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.4u.sdk
        QMAKE_CFLAGS += -mmacosx-version-min=10.4
        DEFINES += MACOS_10_4
        CONFIG += x86 ppc
    }

    # What's our arch?
    !ppc {
        x86_64:x86 {
            echo("Architectures: 32-bit Intel + 64-bit Intel.")
        }
        else:x86_64 {
            echo("Architectures: 64-bit Intel.")
        }
        else {
            echo("Architectures: 32-bit Intel.")
        }
    }

    !deng_nativesdk {
        # Not using Qt, and anyway these would not point to the chosen SDK.
        QMAKE_INCDIR_QT = ""
        QMAKE_LIBDIR_QT = ""
    }
    
    defineTest(useFramework) {
        LIBS += -framework $$1
        INCLUDEPATH += $$QMAKE_MAC_SDK/System/Library/Frameworks/$${1}.framework/Headers
        export(LIBS)
        export(INCLUDEPATH)
        return(true)
    }
}
