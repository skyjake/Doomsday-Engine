include(dep_core_cwrapper.pri)

# libcore's C++ API requires the following Qt modules.
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

win32: defineReplace(qtDLL) {
    greaterThan(QT_MAJOR_VERSION, 4) {
        deng_debug: return(Qt$${QT_MAJOR_VERSION}$${1}d.dll)
              else: return(Qt$${QT_MAJOR_VERSION}$${1}.dll)
    }
    else {
        deng_debug: return(Qt$${1}$${QT_MAJOR_VERSION}d.dll)
              else: return(Qt$${1}$${QT_MAJOR_VERSION}.dll)
    }
}

deng_debug: qtver = d$$QT_MAJOR_VERSION
else:       qtver = $$QT_MAJOR_VERSION

win32 {
    # Install the required Qt DLLs into the products dir.
    INSTALLS *= qtlibs qtplugins
    qtlibs.files += \
        $$[QT_INSTALL_BINS]/$$qtDLL(Core) \
        $$[QT_INSTALL_BINS]/$$qtDLL(Network)
    deng_qtgui:     qtlibs.files += $$[QT_INSTALL_BINS]/$$qtDLL(Gui)
    deng_qtwidgets: qtlibs.files += $$[QT_INSTALL_BINS]/$$qtDLL(Widgets)
    deng_qtopengl:  qtlibs.files += $$[QT_INSTALL_BINS]/$$qtDLL(OpenGL)
    qtlibs.path = $$DENG_LIB_DIR

    qtplugins.path = $$DENG_LIB_DIR/imageformats
    greaterThan(QT_MAJOR_VERSION, 4) {
        qtplugins.files = \
            $$[QT_INSTALL_PLUGINS]/imageformats/qjpeg.dll \
            $$[QT_INSTALL_PLUGINS]/imageformats/qico.dll \
            $$[QT_INSTALL_PLUGINS]/imageformats/qgif.dll

        # International Components for Unicode
        INSTALLS *= qtdeps
        qtdeps.files += \
            $$[QT_INSTALL_BINS]/icuin52.dll \
            $$[QT_INSTALL_BINS]/icuuc52.dll \
            $$[QT_INSTALL_BINS]/icudt52.dll
        qtdeps.path = $$DENG_LIB_DIR
    }
    else {
        qtplugins.files = $$[QT_INSTALL_PLUGINS]/imageformats/qjpeg4.dll
    }
}

macx {
    defineTest(linkBinaryToBundledLibcore) {
        fixInstallName($$1, libdeng_core.2.dylib, ..)
        fixInstallName($$1, QtCore.framework/Versions/$$QT_MAJOR_VERSION/QtCore, ..)
        fixInstallName($$1, QtNetwork.framework/Versions/$$QT_MAJOR_VERSION/QtNetwork, ..)
        deng_qtgui:     fixInstallName($$1, QtGui.framework/Versions/$$QT_MAJOR_VERSION/QtGui, ..)
        deng_qtwidgets: fixInstallName($$1, QtWidgets.framework/Versions/$$QT_MAJOR_VERSION/QtWidgets, ..)
        deng_qtopengl:  fixInstallName($$1, QtOpenGL.framework/Versions/$$QT_MAJOR_VERSION/QtOpenGL, ..)
    }
    defineTest(linkToBundledLibcore) {
        linkBinaryToBundledLibcore($${1}.bundle/$$1)
    }
    defineTest(linkDylibToBundledLibcore) {
        linkBinaryToBundledLibcore($${1}.dylib)
    }
}
