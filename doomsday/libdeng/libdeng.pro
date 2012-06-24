# The Doomsday Engine Project
# Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012 Daniel Swanson <danij@dengine.net>

TEMPLATE = lib
TARGET = deng1

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

INCLUDEPATH += src include include/de

# Definitions ----------------------------------------------------------------

DEFINES += __DENG__ __DOOMSDAY__

!isEmpty(DENG_BUILD) {
    !win32: echo(Build number: $$DENG_BUILD)
    DEFINES += DOOMSDAY_BUILD_TEXT=\\\"$$DENG_BUILD\\\"
} else {
    !win32: echo(DENG_BUILD is not defined.)
}

# Source Files ---------------------------------------------------------------

# Public headers
HEADERS += \
    include/de/smoother.h \
    include/de/libdeng.h \
    include/de/types.h
    #include/version.h

# Sources and private headers
SOURCES += \
    src/smoother.cpp

# Installation ---------------------------------------------------------------

macx {
    linkDylibToBundledLibdeng2(libdeng1)

    defineTest(fixDengLinkage) {
        doPostLink("install_name_tool -change $$1 @executable_path/../Frameworks/$$1 libdeng1.1.dylib")
        doPostLink("install_name_tool -change $$(QTDIR)lib/$$1 @executable_path/../Frameworks/$$1 libdeng1.1.dylib")
        doPostLink("install_name_tool -change $$(QTDIR)/lib/$$1 @executable_path/../Frameworks/$$1 libdeng1.1.dylib")
    }
    fixDengLinkage("QtCore.framework/Versions/4/QtCore")
    fixDengLinkage("QtNetwork.framework/Versions/4/QtNetwork")
    fixDengLinkage("QtGui.framework/Versions/4/QtGui")
    fixDengLinkage("QtOpenGL.framework/Versions/4/QtOpenGL")

    # Update the library included in the main app bundle.
    doPostLink("mkdir -p ../engine/Doomsday.app/Contents/Frameworks")
    doPostLink("cp -fRp libdeng1*dylib ../engine/Doomsday.app/Contents/Frameworks")
}
else {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}

#win32 {
#    RC_FILE = res/windows/deng.rc
#}
