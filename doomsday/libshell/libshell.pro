# The Doomsday Engine Project: Server Shell -- Common Functionality
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

include(../config.pri)

TEMPLATE = lib
TARGET   = deng_shell
VERSION  = 0.1.0

include(../dep_deng2.pri)

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

DEFINES += __LIBSHELL__

INCLUDEPATH += include

# Public headers.
HEADERS += \
    include/de/shell/Action \
    include/de/shell/ChoiceWidget \
    include/de/shell/DialogWidget \
    include/de/shell/InputDialog \
    include/de/shell/KeyEvent \
    include/de/shell/LabelWidget \
    include/de/shell/LineEditWidget \
    include/de/shell/Link \
    include/de/shell/LocalServer \
    include/de/shell/MenuWidget \
    include/de/shell/Protocol \
    include/de/shell/ServerFinder \
    include/de/shell/TextCanvas \
    include/de/shell/TextRootWidget \
    include/de/shell/TextWidget \
    \
    include/de/shell/action.h \
    include/de/shell/choicewidget.h \
    include/de/shell/dialogwidget.h \
    include/de/shell/inputdialog.h \
    include/de/shell/keyevent.h \
    include/de/shell/labelwidget.h \
    include/de/shell/libshell.h \
    include/de/shell/lineeditwidget.h \
    include/de/shell/link.h \
    include/de/shell/localserver.h \
    include/de/shell/menuwidget.h \
    include/de/shell/protocol.h \
    include/de/shell/textcanvas.h \
    include/de/shell/textrootwidget.h \
    include/de/shell/textwidget.h \ 
    include/de/shell/serverfinder.h \
    include/de/shell/lexicon.h

# Sources and private headers.
SOURCES += \
    src/action.cpp \
    src/dialogwidget.cpp \
    src/inputdialog.cpp \
    src/labelwidget.cpp \
    src/libshell.cpp \
    src/lineeditwidget.cpp \
    src/link.cpp \
    src/menuwidget.cpp \
    src/protocol.cpp \
    src/textcanvas.cpp \
    src/textrootwidget.cpp \
    src/textwidget.cpp \
    src/choicewidget.cpp \
    src/localserver.cpp \
    src/serverfinder.cpp \
    src/lexicon.cpp

# Installation ---------------------------------------------------------------

macx {
    linkDylibToBundledLibdeng2(libdeng_shell)

    doPostLink("install_name_tool -id @executable_path/../Frameworks/libdeng_shell.0.dylib libdeng_shell.0.dylib")

    # Update the library included in the main app bundle.
    doPostLink("mkdir -p ../client/Doomsday.app/Contents/Frameworks")
    doPostLink("cp -fRp libdeng_shell*dylib ../client/Doomsday.app/Contents/Frameworks")
}
else {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
