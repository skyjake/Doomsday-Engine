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
    include/de/shell/AbstractLineEditor \
    include/de/shell/AbstractLink \
    include/de/shell/Action \
    include/de/shell/ChoiceWidget \
    include/de/shell/CommandLineWidget \
    include/de/shell/DialogWidget \
    include/de/shell/DoomsdayInfo \
    include/de/shell/EditorHistory \
    include/de/shell/InputDialog \
    include/de/shell/ITextEditor \
    include/de/shell/KeyEvent \
    include/de/shell/LabelWidget \
    include/de/shell/Lexicon \
    include/de/shell/LineEditWidget \
    include/de/shell/Link \
    include/de/shell/LocalServer \
    include/de/shell/LogWidget \
    include/de/shell/MenuWidget \
    include/de/shell/MonospaceLineWrapping \
    include/de/shell/Protocol \
    include/de/shell/ServerFinder \
    include/de/shell/TextCanvas \
    include/de/shell/TextRootWidget \
    include/de/shell/TextWidget \
    \
    include/de/shell/abstractlineeditor.h \
    include/de/shell/abstractlink.h \
    include/de/shell/action.h \
    include/de/shell/choicewidget.h \
    include/de/shell/commandlinewidget.h \
    include/de/shell/dialogwidget.h \
    include/de/shell/doomsdayinfo.h \
    include/de/shell/editorhistory.h \
    include/de/shell/inputdialog.h \
    include/de/shell/itexteditor.h \
    include/de/shell/keyevent.h \
    include/de/shell/labelwidget.h \
    include/de/shell/lexicon.h \
    include/de/shell/libshell.h \
    include/de/shell/lineeditwidget.h \
    include/de/shell/link.h \
    include/de/shell/localserver.h \
    include/de/shell/logwidget.h \
    include/de/shell/menuwidget.h \
    include/de/shell/monospacelinewrapping.h \
    include/de/shell/protocol.h \
    include/de/shell/serverfinder.h \
    include/de/shell/textcanvas.h \
    include/de/shell/textrootwidget.h \
    include/de/shell/textwidget.h

# Sources and private headers.
SOURCES += \
    src/abstractlineeditor.cpp \
    src/abstractlink.cpp \
    src/action.cpp \
    src/choicewidget.cpp \
    src/commandlinewidget.cpp \
    src/dialogwidget.cpp \
    src/doomsdayinfo.cpp \
    src/editorhistory.cpp \
    src/inputdialog.cpp \
    src/labelwidget.cpp \
    src/lexicon.cpp \
    src/libshell.cpp \
    src/lineeditwidget.cpp \
    src/link.cpp \
    src/localserver.cpp \
    src/logwidget.cpp \
    src/menuwidget.cpp \
    src/monospacelinewrapping.cpp \
    src/protocol.cpp \
    src/serverfinder.cpp \
    src/textcanvas.cpp \
    src/textrootwidget.cpp \
    src/textwidget.cpp

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
