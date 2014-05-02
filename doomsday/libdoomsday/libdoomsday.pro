# The Doomsday Engine Project -- Common Doomsday Subsystems
# License: GPL 2+
# Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2014 Daniel Swanson <danij@dengine.net>

TEMPLATE = lib
TARGET = doomsday

# Build Configuration --------------------------------------------------------

include(../config.pri)

VERSION = $$DENG_VERSION

# External Dependencies ------------------------------------------------------

include(../dep_core.pri)
include(../dep_legacy.pri)
include(../dep_shell.pri)

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}
# Enable strict warnings.
*-g++|*-gcc|*-clang* {
    #warnOpts = -Wall -Wextra -pedantic -Wno-long-long
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

INCLUDEPATH += src include include/de $$DENG_API_DIR

# Definitions ----------------------------------------------------------------

DEFINES += __DENG__ __DOOMSDAY__ __LIBDOOMSDAY__

!isEmpty(DENG_BUILD) {
    !win32: echo(Build number: $$DENG_BUILD)
    DEFINES += DOOMSDAY_BUILD_TEXT=\\\"$$DENG_BUILD\\\"
} else {
    !win32: echo(DENG_BUILD is not defined.)
}

# Source Files ---------------------------------------------------------------

# Public headers
HEADERS += \
    include/doomsday/audio/logical.h \
    include/doomsday/audio/s_wav.h \
    include/doomsday/dualstring.h \
    include/doomsday/world/mobj.h

# Sources and private headers
SOURCES += \
    src/audio/logical.cpp \
    src/audio/s_wav.cpp \
    src/dualstring.cpp

# Installation ---------------------------------------------------------------

macx {
    linkDylibToBundledLibcore  (libdoomsday)
    linkDylibToBundledLiblegacy(libdoomsday)
    linkDylibToBundledLibshell (libdoomsday)

    doPostLink("install_name_tool -id @executable_path/../Frameworks/libdoomsday.1.dylib libdoomsday.1.dylib")

    # Update the library included in the main app bundle.
    doPostLink("mkdir -p ../client/Doomsday.app/Contents/Frameworks")
    doPostLink("cp -fRp libdoomsday*dylib ../client/Doomsday.app/Contents/Frameworks")
}
else {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
