QT -= core gui

# Directories ----------------------------------------------------------------

DENG_API_DIR = $$PWD/engine/api
DENG_INCLUDE_DIR = $$PWD/engine/portable/include
DENG_UNIX_INCLUDE_DIR = $$PWD/engine/unix/include
DENG_MAC_INCLUDE_DIR = $$PWD/engine/mac/include
DENG_WIN_INCLUDE_DIR = $$PWD/engine/win32/include
DENG_LZSS_DIR = $$PWD/external/lzss

# Versions -------------------------------------------------------------------

include(versions.pri)
message(Doomsday version $${DENG_VERSION}.)

# Build Options --------------------------------------------------------------

# Configure for Debug/Release build.
CONFIG(debug, debug|release) {
    message("Debug build.")
    DEFINES += _DEBUG
    DENG_CONFIG += rangecheck
} else {
    message("Release build.")
    DEFINES += NDEBUG
}

win32 {
    DEFINES += WIN32
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
    DENG_CONFIG += nofixedasm

    # Install prefix.
    PREFIX = /usr

    # Binary location.
    BINDIR = $$PREFIX/bin

    # Library location.
    LIBDIR = $$PREFIX/lib

    contains(QMAKE_HOST.arch, x86_64) {
        message(64-bit architecture detected.)
        DEFINES += HOST_IS_64BIT

        exists($$PREFIX/lib64) {
            LIBDIR = $$PREFIX/lib64
        }
        exists($$PREFIX/lib/x86_64-linux-gnu) {
            LIBDIR = $$PREFIX/lib/x86_64-linux-gnu
        }
    }

    # Link against standard math library.
    LIBS += -lm

    message(Install prefix: $$PREFIX)
    message(Binary directory: $$BINDIR)
    message(Library directory: $$LIBDIR)
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
        CONFIG += x86 x86_64
    }
    else {
        message("Using Mac OS 10.4 SDK (Universal 32-bit Intel/PowerPC binary.)")
        QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.4u.sdk
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
