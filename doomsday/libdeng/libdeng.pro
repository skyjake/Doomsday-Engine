# The Doomsday Engine Project
# Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012 Daniel Swanson <danij@dengine.net>

TEMPLATE = lib
TARGET = deng

# Build Configuration --------------------------------------------------------

include(../config.pri)

deng_writertypecheck {
    DEFINES += DENG_WRITER_TYPECHECK
}

VERSION = $$DENG_VERSION

# External Dependencies ------------------------------------------------------

include(../dep_deng2.pri)

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

INCLUDEPATH += api include

# Definitions ----------------------------------------------------------------

DEFINES += __DENG__ __DOOMSDAY__

!isEmpty(DENG_BUILD) {
    !win32: echo(Build number: $$DENG_BUILD)
    DEFINES += DOOMSDAY_BUILD_TEXT=\\\"$$DENG_BUILD\\\"
} else {
    !win32: echo(DENG_BUILD is not defined.)
}

# Source Files ---------------------------------------------------------------

HEADERS +=

SOURCES += 

# Installation ---------------------------------------------------------------

macx {
    linkDylibToBundledLibdeng2(libdeng)
} 
else {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}

#win32 {
#    RC_FILE = windows/res/deng.rc
#}
