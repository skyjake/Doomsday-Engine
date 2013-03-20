# The Doomsday Engine Project: Server Shell -- GUI Interface
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

include(../../../config.pri)

TEMPLATE = app

      macx: TARGET = "Doomsday Shell"
else:win32: TARGET = Doomsday-Shell
      else: TARGET = doomsday-shell

VERSION = $$DENG_VERSION

# Build Configuration -------------------------------------------------------

DEFINES += SHELL_VERSION=\\\"$$VERSION\\\"

CONFIG += deng_qtgui

include(../../../dep_deng2.pri)
include(../../../dep_shell.pri)

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Sources -------------------------------------------------------------------

HEADERS += \
    src/aboutdialog.h \
    src/consolepage.h \
    src/folderselection.h \
    src/guishellapp.h \
    src/linkwindow.h \
    src/localserverdialog.h \
    src/opendialog.h \
    src/preferences.h \
    src/qtguiapp.h \
    src/qtrootwidget.h \
    src/qttextcanvas.h \
    src/statuswidget.h

SOURCES += \
    src/aboutdialog.cpp \
    src/consolepage.cpp \
    src/folderselection.cpp \
    src/guishellapp.cpp \
    src/linkwindow.cpp \
    src/localserverdialog.cpp \
    src/main.cpp \
    src/opendialog.cpp \
    src/preferences.cpp \
    src/qtguiapp.cpp \
    src/qtrootwidget.cpp \
    src/qttextcanvas.cpp \
    src/statuswidget.cpp

RESOURCES += \
    res/shell.qrc

win32 {
    RC_FILE = res/windows/shell.rc
}

# Deployment ----------------------------------------------------------------

macx {
    ICON = res/macx/shell.icns
    QMAKE_INFO_PLIST = res/macx/Info.plist
    QMAKE_BUNDLE_DATA += res
    res.path = Contents/Resources
    res.files = res/macx/English.lproj

    # Clean up previous deployment.
    doPostLink("rm -rf \"Doomsday Shell.app/Contents/PlugIns/\"")
    doPostLink("rm -f \"Doomsday Shell.app/Contents/Resources/qt.conf\"")

    doPostLink("macdeployqt \"Doomsday Shell.app\"")
}
else {
    INSTALLS += target
    target.path = $$DENG_BIN_DIR
    
    unix {
        INSTALLS += icon
        icon.files = res/shell.png
        icon.path = $$DENG_BASE_DIR/icons
        
        # Generate a .desktop file for the applications menu.
        desktopFile = doomsday-shell.desktop
        !system(sed \"s:BIN_DIR:$$DENG_BIN_DIR:; s:BASE_DIR:$$DENG_BASE_DIR:\" \
            <\"../../../../distrib/linux/$$desktopFile\" \
            >\"$$OUT_PWD/$$desktopFile\"): error(Can\'t build $$desktopFile)
        desktop.files = $$OUT_PWD/$$desktopFile
        desktop.path = $$PREFIX/share/applications        
        INSTALLS += desktop

        OTHER_FILES += ../../../../distrib/linux/$$desktopFile
    }
}
