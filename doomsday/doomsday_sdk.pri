# The Doomsday Engine Project
# Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# qmake .pri file for projects using the Doomsday SDK.
#
# The Doomsday SDK is distributed under the GNU Lesser General Public
# License version 3 (or, at your option, any later version). Please
# visit http://www.gnu.org/licenses/lgpl.html for details.
#
# Variables:
# - DENG_CONFIG     Names of supporting libraries to use (gui, appfw, shell)

DENG_SDK_DIR = $$PWD

exists($$DENG_SDK_DIR/include/doomsday) {
    INCLUDEPATH += $$DENG_SDK_DIR/include/doomsday
}
else {
    INCLUDEPATH += $$DENG_SDK_DIR/include
}

LIBS += -L$$DENG_SDK_DIR/lib

# The core library is always required.
LIBS += -ldeng2

# Supporting libraries are optional.
contains(DENG_CONFIG, gui):   LIBS += -ldeng_gui
contains(DENG_CONFIG, appfw): LIBS += -ldeng_appfw
contains(DENG_CONFIG, shell): LIBS += -ldeng_shell

# Instruct the dynamic linker to load the libs from the SDK lib dir.
*-g++*|*-gcc*|*-clang* {
    QMAKE_LFLAGS += -Wl,-rpath,$$DENG_SDK_DIR/lib
}

defineTest(dengClear) {
    # 1: file to remove
    win32: system(del /q \"$$1\")
     else: system(rm -f \"$$1\")
}

defineTest(dengPack) {
    # 1: path of a .pack file
    # 2: actual root directory
    # 3: files to include, relative to the root
    system(cd \"$$2\" && zip -r \"$$1\" $$3)
}

DENG_MODULES = $$DENG_SDK_DIR/modules/*.de

defineTest(dengPackModules) {
    # 1: path of a .pack file
    dengPack($$1, $$DENG_SDK_DIR, modules/*.de)
}
