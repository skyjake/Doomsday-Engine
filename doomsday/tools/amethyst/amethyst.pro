# Amethyst (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

QT += core
QT -= gui

CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app
TARGET = amethyst

SOURCES += \
    src/block.cpp \
    src/callstack.cpp \
    src/command.cpp \
    src/commandline.cpp \
    src/commandrule.cpp \
    src/commandruleset.cpp \
    src/contextrelation.cpp \
    src/formatrule.cpp \
    src/gem.cpp \
    src/gemclass.cpp \
    src/gemtest.cpp \
    src/gemtestcommand.cpp \
    src/length.cpp \
    src/lengthrule.cpp \
    src/linkable.cpp \
    src/macro.cpp \
    src/main.cpp \
    src/outputcontext.cpp \
    src/outputstate.cpp \
    src/processor.cpp \
    src/rule.cpp \
    src/ruleset.cpp \
    src/schedule.cpp \
    src/shard.cpp \
    src/source.cpp \
    src/string.cpp \
    src/structurecounter.cpp \
    src/token.cpp \
    src/utils.cpp \
    src/exception.cpp

HEADERS += \
    src/block.h \
    src/callstack.h \
    src/command.h \
    src/commandline.h \
    src/commandrule.h \
    src/commandruleset.h \
    src/contextrelation.h \
    src/defs.h \
    src/exception.h \
    src/formatrule.h \
    src/gem.h \
    src/gemclass.h \
    src/gemtest.h \
    src/gemtestcommand.h \
    src/length.h \
    src/lengthrule.h \
    src/linkable.h \
    src/list.h \
    src/macro.h \
    src/outputcontext.h \
    src/outputstate.h \
    src/processor.h \
    src/rule.h \
    src/ruleset.h \
    src/schedule.h \
    src/shard.h \
    src/source.h \
    src/string.h \
    src/stringlist.h \
    src/structurecounter.h \
    src/token.h \
    src/uniquelist.h \
    src/utils.h

OTHER_FILES += plan.txt changelog.txt

# Compiler options.
*-gcc* {
    QMAKE_CXXFLAGS += -pedantic
    !mac:!win32: QMAKE_CXXFLAGS += -std=c++0x
}
win32-msvc* {
    DEFINES += _CRT_SECURE_NO_WARNINGS
}

# Installation.
library.files += \
    lib/amestd.ame \
    lib/ametxt.ame \
    lib/amehtml.ame \
    lib/amertf.ame \
    lib/ameman.ame \
    lib/amewiki.ame \
    lib/amehelp.ame

unix {
    isEmpty(PREFIX) {
        PREFIX = /usr/local
    }
    BINDIR = $$PREFIX/bin
    SHAREDIR = $$PREFIX/share/amethyst
    DOCDIR = $$PREFIX/share/doc/amethyst
    AMELIBDIR = $$SHAREDIR/lib

    # Default location for the library files.
    DEFINES += AME_LIBDIR=\\\"$$AMELIBDIR\\\"

    INSTALLS += target library
    target.path = $$BINDIR
    
    library.path = $$AMELIBDIR
}
win32 {
    isEmpty(PREFIX) {
        PREFIX = $$PWD/inst
    }
    BINDIR = $$PREFIX
    AMELIBDIR = $$PREFIX/lib
    DOCDIR = $%PREFIX/doc

    INSTALLS += target qtcore library #guide
    target.path = $$BINDIR

    CONFIG(debug, debug|release): qtver = d4
    else: qtver = 4
    qtcore.files = $$[QT_INSTALL_LIBS]/QtCore$${qtver}.dll
    qtcore.path = $$BINDIR

    library.path = $$AMELIBDIR
}
