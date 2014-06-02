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
include(../dep_zlib.pri)

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
    include/doomsday/console/alias.h \
    include/doomsday/console/cmd.h \
    include/doomsday/console/exec.h \
    include/doomsday/console/knownword.h \
    include/doomsday/console/var.h \
    include/doomsday/defs/ded.h \
    include/doomsday/defs/dedarray.h \
    include/doomsday/defs/dedfile.h \
    include/doomsday/defs/dedparser.h \
    include/doomsday/defs/dedtypes.h \
    include/doomsday/dualstring.h \
    include/doomsday/filesys/file.h \
    include/doomsday/filesys/filehandle.h \
    include/doomsday/filesys/filehandlebuilder.h \
    include/doomsday/filesys/fileid.h \
    include/doomsday/filesys/fileinfo.h \
    include/doomsday/filesys/filetype.h \
    include/doomsday/filesys/fs_main.h \
    include/doomsday/filesys/fs_util.h \
    include/doomsday/filesys/lumpcache.h \
    include/doomsday/filesys/lumpindex.h \
    include/doomsday/filesys/searchpath.h \
    include/doomsday/filesys/sys_direc.h \
    include/doomsday/filesys/wad.h \
    include/doomsday/filesys/zip.h \
    include/doomsday/help.h \
    include/doomsday/paths.h \
    include/doomsday/resource/resourceclass.h \
    include/doomsday/resource/wav.h \
    include/doomsday/uri.h \
    include/doomsday/world/mobj.h

win32: HEADERS += \
    include/doomsday/filesys/fs_windows.h

# Sources and private headers
SOURCES += \
    src/audio/logical.cpp \
    src/console/alias.cpp \
    src/console/cmd.cpp \
    src/console/exec.cpp \
    src/console/knownword.cpp \
    src/console/var.cpp \
    src/defs/ded.cpp \
    src/defs/dedfile.cpp \
    src/defs/dedparser.cpp \
    src/dualstring.cpp \
    src/filesys/file.cpp \
    src/filesys/filehandle.cpp \
    src/filesys/fileid.cpp \
    src/filesys/filetype.cpp \
    src/filesys/fs_main.cpp \
    src/filesys/fs_scheme.cpp \
    src/filesys/fs_util.cpp \
    src/filesys/lumpindex.cpp \
    src/filesys/searchpath.cpp \
    src/filesys/sys_direc.cpp \
    src/filesys/wad.cpp \
    src/filesys/zip.cpp \
    src/help.cpp \
    src/paths.cpp \
    src/resource/resourceclass.cpp \
    src/resource/wav.cpp \
    src/uri.cpp

win32: SOURCES += \
    src/filesys/fs_windows.cpp

# Installation ---------------------------------------------------------------

macx {
    linkDylibToBundledLibcore  (libdoomsday)
    linkDylibToBundledLiblegacy(libdoomsday)
    linkDylibToBundledLibshell (libdoomsday)

    doPostLink("install_name_tool -id @rpath/libdoomsday.1.dylib libdoomsday.1.dylib")

    # Update the library included in the main app bundle.
    doPostLink("mkdir -p ../client/Doomsday.app/Contents/Frameworks")
    doPostLink("cp -fRp libdoomsday*dylib ../client/Doomsday.app/Contents/Frameworks")
}
else {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
