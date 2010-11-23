TEMPLATE = lib
TARGET = deng2
QT += core network

include(version.pri)

LIBDENG2_HEADERS = \
    include/de/core.h \
    include/de/data.h \
    include/de/deng.h \
    include/de/error.h \
    include/de/fs.h \
    include/de/math.h \
    include/de/net.h \
    include/de/script.h \
    include/de/types.h \
    include/de/version.h \
    include/de/world.h \
    $$files(include/de/core/*.h) \
    $$files(include/de/data/*.h) \
    $$files(include/de/filesys/*.h) \
    $$files(include/de/net/*.h) \
    $$files(include/de/scriptsys/*.h) \
    $$files(include/de/types/*.h) \
    $$files(include/de/worldsys/*.h)

INCLUDEPATH = include

HEADERS += $$LIBDENG2_HEADERS

SOURCES += \
    $$files(src/core/*.cc) \
    $$files(src/data/*.cc) \
    $$files(src/filesys/*.cc) \
    $$files(src/net/*.cc) \
    $$files(src/types/*.cc) \
    $$files(src/scriptsys/*.cc) \
    $$files(src/worldsys/*.cc)

# Build a framework on Mac OS X.
macx {
    CONFIG += lib_bundle
    FRAMEWORK_HEADERS.version = Versions
    FRAMEWORK_HEADERS.files = include/de
    FRAMEWORK_HEADERS.path = Headers
    QMAKE_BUNDLE_DATA += FRAMEWORK_HEADERS
}

LIBS += -lz

INSTALLS += target
target.path = $$(HOME)/Library/Frameworks/
