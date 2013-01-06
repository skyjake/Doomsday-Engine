# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>
#
# Windows-specific configuration.

win32-g++* {
    error("Sorry, gcc is not supported in the Windows build.")
}

DEFINES += WIN32 _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES

QMAKE_CFLAGS += -w14505
QMAKE_CXXFLAGS += -w14505

# Install locations:
DENG_BASE_DIR       = $$DENG_WIN_PRODUCTS_DIR
DENG_BIN_DIR        = $$DENG_BASE_DIR/bin
DENG_LIB_DIR        = $$DENG_BIN_DIR
DENG_PLUGIN_LIB_DIR = $$DENG_LIB_DIR/plugins
DENG_DATA_DIR       = $$DENG_BASE_DIR/data
DENG_DOCS_DIR       = $$DENG_BASE_DIR/doc

# Tell rc where to get the API headers.
QMAKE_RC = $$QMAKE_RC /I \"$$DENG_API_DIR\"

deng_debug: $$QMAKE_RC = $$QMAKE_RC /d _DEBUG

# Also build the OpenAL plugin.
CONFIG += deng_openal
