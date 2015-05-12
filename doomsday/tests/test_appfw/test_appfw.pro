# Using Doomsday.
DENG_CONFIG = gui appfw symlink
include($$DENG_SDK_DIR/doomsday_sdk.pri)

# Package the application resources.
dengPackage(net.dengine.test.appfw, $$OUT_PWD)

win32: INCLUDEPATH += C:/SDK/OpenGL

QT += gui opengl network
CONFIG += c++11

SOURCES += \
    src/approotwidget.cpp \
    src/appwindowsystem.cpp \
    src/globalshortcuts.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/testapp.cpp

HEADERS += \
    src/approotwidget.h \
    src/appwindowsystem.h \
    src/globalshortcuts.h \
    src/mainwindow.h \
    src/testapp.h

!macx: dengDynamicLinkPath($$denglibs.path)
dengDynamicLinkPath($$[QT_INSTALL_LIBS])

dengDeploy(test_appfw, $$PREFIX)
