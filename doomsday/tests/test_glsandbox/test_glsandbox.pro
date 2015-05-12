# Using Doomsday.
DENG_CONFIG = gui symlink
include($$DENG_SDK_DIR/doomsday_sdk.pri)

# Package the application resources.
dengPackage(net.dengine.test.glsandbox, $$OUT_PWD)

win32: INCLUDEPATH += C:/SDK/OpenGL

QT += gui opengl
CONFIG += c++11

SOURCES += \
    main.cpp \
    testwindow.cpp

HEADERS += \
    testwindow.h

!macx: dengDynamicLinkPath($$denglibs.path)
dengDynamicLinkPath($$[QT_INSTALL_LIBS])

dengDeploy(test_glsandbox, $$PREFIX)
