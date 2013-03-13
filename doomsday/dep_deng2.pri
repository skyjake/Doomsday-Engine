include(dep_deng2_cwrapper.pri)

# libdeng2's C++ API requires the following Qt modules.
QT += core network

# Optional modules:
deng_qtopengl: QT += opengl
deng_qtgui {
    QT += gui
    greaterThan(QT_MAJOR_VERSION, 4) {
        QT += widgets
        CONFIG += deng_qtwidgets
    }
}

deng_debug: qtver = d$$QT_MAJOR_VERSION
else:       qtver = $$QT_MAJOR_VERSION

win32 {
    # Install the required Qt DLLs into the products dir.
    INSTALLS *= qtlibs qtplugins
    qtlibs.files += \
        $$[QT_INSTALL_BINS]/QtCore$${qtver}.dll \
        $$[QT_INSTALL_BINS]/QtNetwork$${qtver}.dll
    deng_qtgui:     qtlibs.files += $$[QT_INSTALL_BINS]/QtGui$${qtver}.dll
    deng_qtwidgets: qtlibs.files += $$[QT_INSTALL_BINS]/QtWidgets$${qtver}.dll
    deng_qtopengl:  qtlibs.files += $$[QT_INSTALL_BINS]/QtOpenGL$${qtver}.dll
    qtlibs.path = $$DENG_LIB_DIR

    qtplugins.files = $$[QT_INSTALL_PLUGINS]/imageformats/qjpeg4.dll
    qtplugins.path = $$DENG_LIB_DIR/imageformats
}

macx {
    defineTest(linkBinaryToBundledLibdeng2) {
        fixInstallName($$1, libdeng2.2.dylib, ..)
        fixInstallName($$1, QtCore.framework/Versions/$$QT_MAJOR_VERSION/QtCore, ..)
        fixInstallName($$1, QtNetwork.framework/Versions/$$QT_MAJOR_VERSION/QtNetwork, ..)
        deng_qtgui:     fixInstallName($$1, QtGui.framework/Versions/$$QT_MAJOR_VERSION/QtGui, ..)
        deng_qtwidgets: fixInstallName($$1, QtWidgets.framework/Versions/$$QT_MAJOR_VERSION/QtWidgets, ..)
        deng_qtopengl:  fixInstallName($$1, QtOpenGL.framework/Versions/$$QT_MAJOR_VERSION/QtOpenGL, ..)
    }
    defineTest(linkToBundledLibdeng2) {
        linkBinaryToBundledLibdeng2($${1}.bundle/$$1)
    }
    defineTest(linkDylibToBundledLibdeng2) {
        linkBinaryToBundledLibdeng2($${1}.dylib)
    }
}
