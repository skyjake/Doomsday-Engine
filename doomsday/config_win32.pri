# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>
#
# Windows-specific configuration.

win32-g++* {
    CONFIG += deng_mingw
    DEFINES += MINGW32 GNU_X86_FIXED_ASM
}
else {
    CONFIG += deng_msvc
    DEFINES += MSVC WIN32_MSVC
}

DEFINES += WIN32 _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES

deng_msvc {
    # Disable warnings about unreferenced formal parameters (C4100).
    QMAKE_CFLAGS += -w14505 -wd4100 -wd4748
    QMAKE_CXXFLAGS += -w14505 -wd4100 -wd4748
}

DENG_WIN_PRODUCTS_DIR = $$PWD/../distrib/products

# Install locations:
DENG_BASE_DIR       = $$DENG_WIN_PRODUCTS_DIR
DENG_BIN_DIR        = $$DENG_BASE_DIR/bin
DENG_LIB_DIR        = $$DENG_BIN_DIR
DENG_PLUGIN_LIB_DIR = $$DENG_LIB_DIR/plugins
DENG_DATA_DIR       = $$DENG_BASE_DIR/data
DENG_DOCS_DIR       = $$DENG_BASE_DIR/doc

deng_msvc {
    # Tell rc where to get the API headers.
    QMAKE_RC = $$QMAKE_RC /I \"$$DENG_API_DIR\"

    deng_debug: QMAKE_RC = $$QMAKE_RC /d _DEBUG
    
    !deng_debug:deng_debuginfo {
        QMAKE_CFLAGS += -Z7 -DEBUG
        QMAKE_CXXFLAGS += -Z7 -DEBUG
    }
}
deng_mingw {
    QMAKE_RC = $$QMAKE_RC --include-dir=\"$$DENG_API_DIR\"
}

# Also build the OpenAL plugin.
CONFIG += deng_openal
