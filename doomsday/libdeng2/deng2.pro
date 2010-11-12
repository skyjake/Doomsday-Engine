TARGET = deng2
TEMPLATE = lib
QT += core network

# Version numbers.
LIBDENG2_RELEASE_LABEL  = Dev
LIBDENG2_MAJOR_VERSION  = 2
LIBDENG2_MINOR_VERSION  = 0
LIBDENG2_PATCHLEVEL     = 0

SOURCES += \
    $$files(src/core/*.cc) \
    $$files(src/filesys/*.cc) \
    $$files(src/data/*.cc) \
    $$files(src/types/*.cc) \
    $$files(src/scriptsys/*.cc)
#    src/net/*.cc \
#    src/scriptsys/*.cc \
#    src/types/*.cc \
#    src/worldsys/*.cc

HEADERS += \
    include/de/deng.h \
    include/de/error.h \
    include/de/math.h \
    include/de/version.h \
    $$files(include/de/core/*.h) \
    $$files(include/de/data/*.h) \
    $$files(include/de/filesys/*.h) \
    $$files(include/de/net/*.h) \
    $$files(include/de/scriptsys/*.h) \
    $$files(include/de/types/*.h) \
    $$files(include/de/worldsys/*.h)

INCLUDEPATH = include

DEFINES += LIBDENG2_RELEASE_LABEL=\\\"$$LIBDENG2_RELEASE_LABEL\\\"
DEFINES += LIBDENG2_MAJOR_VERSION=$$LIBDENG2_MAJOR_VERSION
DEFINES += LIBDENG2_MINOR_VERSION=$$LIBDENG2_MINOR_VERSION
DEFINES += LIBDENG2_PATCHLEVEL=$$LIBDENG2_PATCHLEVEL

LIBS += -lz
