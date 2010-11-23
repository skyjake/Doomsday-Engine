TEMPLATE = app
TARGET = dengserver

QT += gui network

# Use the libdeng2 version number for the server.
include(../libdeng2/version.pri)

macx {
    # Compile against the deng2 framework.
    INCLUDEPATH += $$(HOME)/Library/Frameworks/deng2.framework/Headers/

    QMAKE_LFLAGS += -F$$(HOME)/Library/Frameworks
    LIBS += -framework deng2

    # Configuration files.
    CFG_FILES.files = ../libdeng2/config
    CFG_FILES.path = Contents/Resources
    SERVER_CFG_FILES.files = \
        config/net.de \
        config/server.de
    SERVER_CFG_FILES.path = Contents/Resources/config/server
    QMAKE_BUNDLE_DATA += CFG_FILES SERVER_CFG_FILES
}

HEADERS += \
    src/client.h \
    src/remoteuser.h \
    src/serverapp.h \
    src/session.h

SOURCES += \
    src/client.cc \
    src/main.cc \
    src/remoteuser.cc \
    src/serverapp.cc \
    src/session.cc
