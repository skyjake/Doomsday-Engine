# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

# Common Unix build options for all Unix-compatible platforms (assumes gcc/clang).

DEFINES += UNIX

# Ease up on the warnings. (The old C code is a bit messy.)
QMAKE_CFLAGS_WARN_ON -= -Wall
QMAKE_CFLAGS_WARN_ON -= -W
QMAKE_CFLAGS_WARN_ON += -Werror-implicit-function-declaration -fdiagnostics-show-option

deng_debuginfo {
    # Inclusion of debug info was requested.
    QMAKE_CFLAGS += -g
    QMAKE_CXXFLAGS += -g
}

*-g++*|*-gcc* {
    # Allow //-comments and anonymous structs inside unions.
    QMAKE_CFLAGS += -std=c99 -fms-extensions
}
*-clang* {
    QMAKE_CFLAGS_WARN_ON += -Wno-tautological-compare
}

deng_qt5 {
    QMAKE_CXXFLAGS += -std=c++11
}

# Print include directories and other info.
#QMAKE_CFLAGS += -Wp,-v
#QMAKE_LFLAGS += -v

# Unix System Tools ----------------------------------------------------------

# Python to be used in generated scripts.
isEmpty(SCRIPT_PYTHON) {
    exists(/usr/bin/python): SCRIPT_PYTHON = /usr/bin/python
    exists(/usr/local/bin/python): SCRIPT_PYTHON = /usr/local/bin/python
}
isEmpty(SCRIPT_PYTHON) {
    # Check the system path.
    SCRIPT_PYTHON = $$system(which python)
}
