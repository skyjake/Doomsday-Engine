# The Doomsday Engine Project -- Doomsday 2 Core Library
# License: LGPL 3
# Copyright (c) 2011-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2014 Daniel Swanson <danij@dengine.net>

TEMPLATE = lib
TARGET = deng_core

# Build Configuration --------------------------------------------------------

include(../config.pri)
include(../dep_zlib.pri)

# Version numbers.
LIBDENG2_RELEASE_LABEL  = Dev
LIBDENG2_MAJOR_VERSION  = 2
LIBDENG2_MINOR_VERSION  = 0
LIBDENG2_PATCHLEVEL     = 0

VERSION = $${LIBDENG2_MAJOR_VERSION}.$${LIBDENG2_MINOR_VERSION}.$${LIBDENG2_PATCHLEVEL}

DEFINES += LIBDENG2_RELEASE_LABEL=\\\"$$LIBDENG2_RELEASE_LABEL\\\"
DEFINES += LIBDENG2_MAJOR_VERSION=$$LIBDENG2_MAJOR_VERSION
DEFINES += LIBDENG2_MINOR_VERSION=$$LIBDENG2_MINOR_VERSION
DEFINES += LIBDENG2_PATCHLEVEL=$$LIBDENG2_PATCHLEVEL
DEFINES += LIBDENG2_VERSION=\\\"$${LIBDENG2_MAJOR_VERSION}.$${LIBDENG2_MINOR_VERSION}.$${LIBDENG2_PATCHLEVEL}\\\"

DEFINES += __DENG2__

!isEmpty(DENG_BUILD) {
    DEFINES += LIBDENG2_BUILD_TEXT=\\\"$$DENG_BUILD\\\"
}

# Using Qt.
QT += core network

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll

    # For Windows, an .rc file is generated for embedded version info in the binary.
    system(python "$$PWD/res/win32/generate_rc.py" "$$PWD/res/win32/core.rc" \
        $$LIBDENG2_MAJOR_VERSION $$LIBDENG2_MINOR_VERSION $$LIBDENG2_PATCHLEVEL \
        $$LIBDENG2_RELEASE_LABEL $$DENG_BUILD)
    RC_FILE = res/win32/core.rc

    OTHER_FILES += res/win32/core.rc.in
}

# Enable strict warnings for C++ code.
*-g++|*-clang* {
    QMAKE_CXXFLAGS_WARN_ON *= -Wall -Wextra -pedantic
}
*-g++: QMAKE_CXXFLAGS_WARN_ON *= -Wno-long-long
win32-msvc* {
    #QMAKE_CXXFLAGS_WARN_ON ~= s/-W3/-W4/
}

INCLUDEPATH += include

PRECOMPILED_HEADER = src/precompiled.h

# Source Files ---------------------------------------------------------------

include(concurrency.pri)
include(data.pri)
include(filesys.pri)
include(game.pri)
include(network.pri)
include(scriptsys.pri)
include(widgets.pri)

publicHeaders(root, \
    include/de/App \
    include/de/Asset \
    include/de/Clock \
    include/de/CommandLine \
    include/de/Config \
    include/de/DebugLogSink \
    include/de/Error \
    include/de/Event \
    include/de/FileLogSink \
    include/de/Garbage \
    include/de/HighPerformanceTimer \
    include/de/Id \
    include/de/Library \
    include/de/Log \
    include/de/LogBuffer \
    include/de/LogFilter \
    include/de/LogSink \
    include/de/Loop \
    include/de/Matrix \
    include/de/MemoryLogSink \
    include/de/MonospaceLogSinkFormatter \
    include/de/Range \
    include/de/Rectangle \
    include/de/System \
    include/de/TextApp \
    include/de/TextStreamLogSink \
    include/de/UnixInfo \
    include/de/Vector \
    include/de/Version \
    \
    include/de/c_wrapper.h \
    include/de/charsymbols.h \
    include/de/error.h \
    include/de/libcore.h \
    include/de/math.h \
)

publicHeaders(core, \
    include/de/core/app.h \
    include/de/core/asset.h \
    include/de/core/clock.h \
    include/de/core/commandline.h \
    include/de/core/config.h \
    include/de/core/debuglogsink.h \
    include/de/core/event.h \
    include/de/core/filelogsink.h \
    include/de/core/garbage.h \
    include/de/core/highperformancetimer.h \
    include/de/core/id.h \
    include/de/core/library.h \
    include/de/core/log.h \
    include/de/core/logbuffer.h \
    include/de/core/logfilter.h \
    include/de/core/logsink.h \
    include/de/core/loop.h \
    include/de/core/matrix.h \
    include/de/core/memorylogsink.h \
    include/de/core/monospacelogsinkformatter.h \
    include/de/core/range.h \
    include/de/core/rectangle.h \
    include/de/core/system.h \
    include/de/core/textapp.h \
    include/de/core/textstreamlogsink.h \
    include/de/core/unixinfo.h \
    include/de/core/vector.h \
    include/de/core/version.h \
)

# Private headers.
HEADERS += \
    src/core/logtextstyle.h \
    src/core/callbacktimer.h

SOURCES += \
    src/c_wrapper.cpp \
    src/error.cpp \
    src/matrix.cpp \
    src/version.cpp \
    src/core/app.cpp \
    src/core/asset.cpp \
    src/core/callbacktimer.cpp \
    src/core/clock.cpp \
    src/core/commandline.cpp \
    src/core/config.cpp \
    src/core/debuglogsink.cpp \
    src/core/filelogsink.cpp \
    src/core/garbage.cpp \
    src/core/highperformancetimer.cpp \
    src/core/id.cpp \
    src/core/library.cpp \
    src/core/log.cpp \
    src/core/logbuffer.cpp \
    src/core/logfilter.cpp \
    src/core/logsink.cpp \
    src/core/loop.cpp \
    src/core/memorylogsink.cpp \
    src/core/monospacelogsinkformatter.cpp \
    src/core/system.cpp \
    src/core/textapp.cpp \
    src/core/textstreamlogsink.cpp \
    src/core/unixinfo.cpp

scripts.files = \
    modules/Config.de \
    modules/Log.de \
    modules/recutil.de

OTHER_FILES += \
    $$scripts.files

# Installation ---------------------------------------------------------------

macx {
    defineTest(fixInstallName) {
        doPostLink("install_name_tool -change $$1 @rpath/$$1 libdeng_core.2.dylib")
        doPostLink("install_name_tool -change $$[QT_INSTALL_LIBS]/$$1 @rpath/$$1 libdeng_core.2.dylib")
    }
    doPostLink("install_name_tool -id @rpath/libdeng_core.2.dylib libdeng_core.2.dylib")
    fixInstallName("QtCore.framework/Versions/$$QT_MAJOR_VERSION/QtCore")
    fixInstallName("QtNetwork.framework/Versions/$$QT_MAJOR_VERSION/QtNetwork")

    # Update the library included in the main app bundle.
    doPostLink("mkdir -p ../client/Doomsday.app/Contents/Frameworks")
    doPostLink("cp -fRp libdeng_core*dylib ../client/Doomsday.app/Contents/Frameworks")
}

deployLibrary()

deng_sdk {
    INSTALLS *= scripts
    scripts.path = $$DENG_SDK_DIR/modules
}
