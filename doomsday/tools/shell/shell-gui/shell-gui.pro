# The Doomsday Engine Project: Server Shell -- GUI Interface
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

include(../../../config.pri)

TEMPLATE = app

win32|macx: TARGET = Doomsday-Shell
      else: TARGET = doomsday-shell

VERSION = 1.0.0

# Build Configuration -------------------------------------------------------

DEFINES += SHELL_VERSION=\\\"$$VERSION\\\"

CONFIG += deng_qtgui

include(../../../dep_deng2.pri)
include(../../../dep_shell.pri)

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Sources -------------------------------------------------------------------

HEADERS += \
    src/mainwindow.h \
    src/qtguiapp.h \
    src/qttextcanvas.h \
    src/qtrootwidget.h \
    src/guishellapp.h \
    src/opendialog.h

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/qtguiapp.cpp \
    src/qttextcanvas.cpp \
    src/qtrootwidget.cpp \
    src/guishellapp.cpp \
    src/opendialog.cpp

# Deployment ----------------------------------------------------------------

macx {
    # Clean up previous deployment.
    doPostLink("rm -rf Doomsday-Shell.app/Contents/PlugIns/")
    doPostLink("rm -f Doomsday-Shell.app/Contents/Resources/qt.conf")

    doPostLink("macdeployqt Doomsday-Shell.app")
}
