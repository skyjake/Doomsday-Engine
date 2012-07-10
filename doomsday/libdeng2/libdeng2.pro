# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

TEMPLATE = lib
TARGET = deng2

# Build Configuration --------------------------------------------------------

include(../config.pri)
VERSION = $$DENG2_VERSION
DEFINES += __DENG2__

# Using Qt.
QT += core network gui opengl

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

# Enable strict warnings for C++ code.
*-g++ {
    QMAKE_CXXFLAGS_WARN_ON *= -Wall -Wextra -pedantic -Wno-long-long
}
win32-msvc* {
    #QMAKE_CXXFLAGS_WARN_ON ~= s/-W3/-W4/
}

INCLUDEPATH += include

# Source Files ---------------------------------------------------------------

include(data.pri)
include(legacy.pri)
include(network.pri)

# Convenience headers.
HEADERS += \
    include/de/App \
    include/de/CommandLine \
    include/de/Error \
    include/de/Log \
    include/de/LogBuffer \

HEADERS += \
    include/de/c_wrapper.h \
    include/de/error.h \
    include/de/libdeng2.h \
    include/de/version.h \
    include/de/core/app.h \
    include/de/core/commandline.h \
    include/de/core/log.h \
    include/de/core/logbuffer.h

# Private headers.
HEADERS += \
    src/core/logtextstyle.h \
    src/core/callbacktimer.h

SOURCES += \
    src/c_wrapper.cpp \
    src/core/app.cpp \
    src/core/commandline.cpp \
    src/core/callbacktimer.cpp \
    src/core/log.cpp \
    src/core/logbuffer.cpp \

# Installation ---------------------------------------------------------------

macx {
    defineTest(fixInstallName) {
        doPostLink("install_name_tool -change $$1 @executable_path/../Frameworks/$$1 libdeng2.2.dylib")
        doPostLink("install_name_tool -change $$(QTDIR)lib/$$1 @executable_path/../Frameworks/$$1 libdeng2.2.dylib")
        doPostLink("install_name_tool -change $$(QTDIR)/lib/$$1 @executable_path/../Frameworks/$$1 libdeng2.2.dylib")
    }
    fixInstallName("QtCore.framework/Versions/4/QtCore")
    fixInstallName("QtNetwork.framework/Versions/4/QtNetwork")
    fixInstallName("QtGui.framework/Versions/4/QtGui")
    fixInstallName("QtOpenGL.framework/Versions/4/QtOpenGL")

    # Update the library included in the main app bundle.
    doPostLink("mkdir -p ../engine/doomsday.app/Contents/Frameworks")
    doPostLink("cp -fRp libdeng2*dylib ../engine/doomsday.app/Contents/Frameworks")
}

!macx {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}

win32 {
    RC_FILE = res/win32/deng2.rc
}
