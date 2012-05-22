# Build configuration for using libdeng2.
INCLUDEPATH += $$PWD/libdeng2/include

# Use the appropriate library path.
!useLibDir($$OUT_PWD/../libdeng2) {
    useLibDir($$OUT_PWD/../../libdeng2)
}

LIBS += -ldeng2

# libdeng2 requires the following Qt modules.
QT += core network gui opengl

win32 {
    # Install the required Qt DLLs into the products dir.
    INSTALLS *= qtlibs
    deng_debug: qtver = "d4"
    else:       qtver = "4"
    qtlibs.files += \
        $$[QT_INSTALL_BINS]/QtCore$${qtver}.dll \
        $$[QT_INSTALL_BINS]/QtNetwork$${qtver}.dll \
        $$[QT_INSTALL_BINS]/QtGui$${qtver}.dll \
        $$[QT_INSTALL_BINS]/QtOpenGL$${qtver}.dll
    qtlibs.path = $$DENG_LIB_DIR
}

macx {
    defineTest(fixInstallName) {
        # 1: binary file
        # 2: library name
        # 3: path to Frameworks/
        doPostLink("install_name_tool -change $$2 @executable_path/$$3/Frameworks/$$2 $$1")
    }
    defineTest(linkToBundledLibdeng2) {
        fixInstallName($${1}.bundle/$$1, libdeng2.2.dylib, ..)
        fixInstallName($${1}.bundle/$$1, QtCore.framework/Versions/4/QtCore, ..)
        fixInstallName($${1}.bundle/$$1, QtNetwork.framework/Versions/4/QtNetwork, ..)
        fixInstallName($${1}.bundle/$$1, QtGui.framework/Versions/4/QtGui, ..)
        fixInstallName($${1}.bundle/$$1, QtOpenGL.framework/Versions/4/QtOpenGL, ..)
    }
}
