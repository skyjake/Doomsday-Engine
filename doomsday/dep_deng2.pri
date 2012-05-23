include(dep_deng2_cwrapper.pri)

# libdeng2's C++ API requires the following Qt modules.
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
    defineTest(linkToBundledLibdeng2) {
        fixInstallName($${1}.bundle/$$1, libdeng2.2.dylib, ..)
        fixInstallName($${1}.bundle/$$1, QtCore.framework/Versions/4/QtCore, ..)
        fixInstallName($${1}.bundle/$$1, QtNetwork.framework/Versions/4/QtNetwork, ..)
        fixInstallName($${1}.bundle/$$1, QtGui.framework/Versions/4/QtGui, ..)
        fixInstallName($${1}.bundle/$$1, QtOpenGL.framework/Versions/4/QtOpenGL, ..)
    }
}
