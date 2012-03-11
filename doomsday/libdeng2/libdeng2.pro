# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = lib
TARGET = deng2

# Build Configuration --------------------------------------------------------

include(../config.pri)
VERSION = $$DENG2_VERSION
DEFINES += __DENG2__

# Using Qt.
QT += core network gui

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

*-g++ {
    # Enable strict warnings.
    QMAKE_CXXFLAGS_WARN_ON *= -Wall -Wextra -pedantic -Wno-long-long
}

INCLUDEPATH += include

# Source Files ---------------------------------------------------------------

include(data.pri)
include(legacy.pri)
include(network.pri)

# Convenience headers.
HEADERS += \
    include/de/Error \
    include/de/Log \
    include/de/LogBuffer \

HEADERS += \
    include/de/c_wrapper.h \
    include/de/error.h \
    include/de/libdeng2.h \
    include/de/core/log.h \
    include/de/core/logbuffer.h

# Private headers.
HEADERS += \
    src/core/logtextstyle.h

SOURCES += \
    src/c_wrapper.cpp \
    src/core/log.cpp \
    src/core/logbuffer.cpp

# Installation ---------------------------------------------------------------

macx {
    defineTest(fixInstallName) {
        doPostLink("install_name_tool -change $$1 @executable_path/../Frameworks/$$1 libdeng2.2.dylib")
    }
    fixInstallName("QtCore.framework/Versions/4/QtCore")
    fixInstallName("QtNetwork.framework/Versions/4/QtNetwork")
    fixInstallName("QtGui.framework/Versions/4/QtGui")

    # Update the library included in the main app bundle.
    doPostLink("cp -fRp libdeng2*dylib ../engine/doomsday.app/Contents/Frameworks")
}

!macx {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
