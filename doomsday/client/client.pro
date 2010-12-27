TEMPLATE = app
TARGET = dengclient

QT += gui network opengl

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
    CLIENT_CFG_FILES.files = \
        config/net.de \
        config/client.de \
        config/user.de
    CLIENT_CFG_FILES.path = Contents/Resources/config/client
    QMAKE_BUNDLE_DATA += CFG_FILES CLIENT_CFG_FILES
}

HEADERS += \
    src/clientapp.h \
    src/localserver.h \
    src/surface.h \
    src/usersession.h \
    src/video.h \
    src/visual.h \
    src/window.h \
    src/glwindowsurface.h

SOURCES += \
    src/clientapp.cc \
    src/localserver.cc \
    src/main.cc \
    src/surface.cc \
    src/usersession.cc \
    src/video.cc \
    src/visual.cc \
    src/window.cc \
    src/glwindowsurface.cc
    
