QT -= core gui

include(versions.pri)

message(Doomsday version $${DENG_VERSION}.)

# Debug/release build selection.
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
    DEFINES += UNIX

    # We are not interested in unused parameters (there are quite a few).
    QMAKE_CFLAGS_WARN_ON += \
        -Wno-unused-parameter \
        -Wno-unused-variable \
        -Wno-missing-field-initializers \
        -Wno-missing-braces
}
macx {
    DEFINES += MACOSX

    QMAKE_LFLAGS += -flat_namespace -undefined suppress

    DENG_CONFIG += snowleopard nofixedasm

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

# Directories ----------------------------------------------------------------

DENG_API_DIR = $$PWD/engine/api
DENG_INCLUDE_DIR = $$PWD/engine/portable/include
DENG_UNIX_INCLUDE_DIR = $$PWD/engine/unix/include
DENG_MAC_INCLUDE_DIR = $$PWD/engine/mac/include
DENG_WIN_INCLUDE_DIR = $$PWD/engine/win32/include
DENG_LZSS_DIR = $$PWD/external/lzss

# Apply DENG_CONFIG ----------------------------------------------------------

contains(DENG_CONFIG, nofixedasm) {
    DEFINES += NO_FIXED_ASM
}
!contains(DENG_CONFIG, rangecheck) {
    DEFINES += NORANGECHECKING
}
