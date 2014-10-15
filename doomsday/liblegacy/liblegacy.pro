# The Doomsday Engine Project -- Legacy and C Language Support
# License: GPL 2+
# Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2013 Daniel Swanson <danij@dengine.net>

TEMPLATE = lib
TARGET = deng_legacy

# Build Configuration --------------------------------------------------------

include(../config.pri)

VERSION = $$DENG_VERSION

# External Dependencies ------------------------------------------------------

include(../dep_core.pri)

win32 {
    LIBS += -lwinmm

    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}
# Enable strict warnings.
*-g++|*-gcc|*-clang* {
    warnOpts = -Wall -Wextra -pedantic -Wno-long-long
    QMAKE_CFLAGS_WARN_ON   *= $$warnOpts
    QMAKE_CXXFLAGS_WARN_ON *= $$warnOpts
}
*-g++|*-gcc {
    # We are using code that is not ISO C compliant (anonymous structs/unions)
    # so disable the warnings about it.
    QMAKE_CFLAGS_WARN_ON -= -pedantic
}
*-clang* {
    QMAKE_CFLAGS_WARN_ON *= -Wno-c11-extensions
}
win32-msvc* {
    #QMAKE_CXXFLAGS_WARN_ON ~= s/-W3/-W4/
}

INCLUDEPATH += src include include/de

# Definitions ----------------------------------------------------------------

DEFINES += __DENG__ __DOOMSDAY__

!isEmpty(DENG_BUILD) {
    !win32: echo(Build number: $$DENG_BUILD)
    DEFINES += DOOMSDAY_BUILD_TEXT=\\\"$$DENG_BUILD\\\"
} else {
    !win32: echo(DENG_BUILD is not defined.)
}

deng_writertypecheck {
    DEFINES += DENG_WRITER_TYPECHECK
}

# Source Files ---------------------------------------------------------------

# Public headers
HEADERS += \
    include/de/aabox.h \
    include/de/animator.h \
    include/de/binangle.h \
    include/de/concurrency.h \
    include/de/ddstring.h \
    include/de/findfile.h \
    include/de/fixedpoint.h \
    include/de/liblegacy.h \
    include/de/kdtree.h \
    include/de/mathutil.h \
    include/de/memory.h \
    include/de/memoryblockset.h \
    include/de/memoryzone.h \
    include/de/point.h \
    include/de/reader.h \
    include/de/rect.h \
    include/de/size.h \
    include/de/smoother.h \
    include/de/stack.h \
    include/de/str.h \
    include/de/str.hh \
    include/de/stringarray.h \
    include/de/strutil.h \
    include/de/timer.h \
    include/de/types.h \
    include/de/unittest.h \
    include/de/vector1.h \
    include/de/writer.h

# Sources and private headers
SOURCES += \
    src/animator.c \
    src/binangle.c \
    src/concurrency.cpp \
    src/fixedpoint.c \
    src/liblegacy.c \
    src/kdtree.c \
    src/mathutil.c \
    src/memory.c \
    src/memoryblockset.c \
    src/memoryzone.c \
    src/memoryzone_private.h \
    src/point.c \
    src/reader.c \
    src/rect.c \
    src/size.c \
    src/smoother.cpp \
    src/stack.c \
    src/str.c \
    src/stringarray.cpp \
    src/strutil.c \
    src/timer.cpp \
    src/vector1.c \
    src/writer.c

win32 {
    SOURCES += src/findfile_windows.cpp
}
else:unix {
    SOURCES += src/findfile_unix.c
}

# Installation ---------------------------------------------------------------

macx {
    xcodeFinalizeBuild($$TARGET)
    linkDylibToBundledLibcore(libdeng_legacy)

    doPostLink("install_name_tool -id @rpath/libdeng_legacy.1.dylib libdeng_legacy.1.dylib")

    # Update the library included in the main app bundle.
    doPostLink("mkdir -p ../client/Doomsday.app/Contents/Frameworks")
    doPostLink("cp -fRp libdeng_legacy*dylib ../client/Doomsday.app/Contents/Frameworks")
}

deployLibrary()
