QT -= core gui

# Directories ----------------------------------------------------------------

DENG_API_DIR = $$PWD/engine/api
DENG_INCLUDE_DIR = $$PWD/engine/portable/include
DENG_UNIX_INCLUDE_DIR = $$PWD/engine/unix/include
DENG_MAC_INCLUDE_DIR = $$PWD/engine/mac/include
DENG_WIN_INCLUDE_DIR = $$PWD/engine/win32/include

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
    DENG_CONFIG += rangecheck
} else {
    echo(Release build.)
    DEFINES += NDEBUG
}

win32 {
    win32-gcc* {
        error("Sorry, gcc is not supported in the Windows build.")
    }

    DEFINES += WIN32 _CRT_SECURE_NO_WARNINGS

    DENG_EXPORT_LIB = $$OUT_PWD/../engine/doomsday.lib

    # Also build the OpenAL plugin.
    DENG_PLUGINS += openal
}
unix {
    # Unix/Mac build options.
    DEFINES += UNIX

    # We are not interested in unused parameters (there are quite a few).
    QMAKE_CFLAGS_WARN_ON += \
        -Wno-unused-parameter \
        -Wno-unused-variable \
        -Wno-missing-field-initializers \
        -Wno-missing-braces
}
unix:!macx {
    # Generic Unix build options.
    DENG_CONFIG += nofixedasm installsb

    # Link against standard math library.
    LIBS += -lm

    # Install prefix.
    PREFIX = /usr

    # Binary location.
    DENG_BIN_DIR = $$PREFIX/bin

    # Library location.
    DENG_LIB_DIR = $$PREFIX/lib

    contains(QMAKE_HOST.arch, x86_64) {
        message(64-bit architecture detected.)
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

    message(Binary directory: $$DENG_BIN_DIR)
    message(Library directory: $$DENG_LIB_DIR)
    message(Doomsday base directory: $$DENG_BASE_DIR)
}
macx {
    # Mac OS X build options.
    DENG_CONFIG += snowleopard nofixedasm

    DEFINES += MACOSX

    QMAKE_LFLAGS += -flat_namespace -undefined suppress

    # Select OS version.
    contains(DENG_CONFIG, snowleopard) {
        message("Using Mac OS 10.6 SDK (Universal 32/64-bit, no PowerPC binary).")
        QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.6.sdk
        QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
        CONFIG += x86 x86_64
    }
    else {
        message("Using Mac OS 10.4 SDK (Universal 32-bit Intel/PowerPC binary.)")
        QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.4u.sdk
        QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.4
        CONFIG += x86 ppc
    }
}

# Apply DENG_CONFIG ----------------------------------------------------------

contains(DENG_CONFIG, nofixedasm) {
    DEFINES += NO_FIXED_ASM
}
!contains(DENG_CONFIG, rangecheck) {
    DEFINES += NORANGECHECKING
}
