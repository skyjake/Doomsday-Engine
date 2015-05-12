# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>
#
# Unix/X11 configuration (non-Mac).

include(config_unix_any.pri)

CONFIG += deng_nofixedasm

DEFINES += DENG_X11

!deng_nosnowberry: CONFIG += deng_snowberry

exists(/etc/apt) {
    # Choose the apt repository to include in the distribution.
    isStableRelease(): CONFIG += deng_aptstable
                 else: CONFIG += deng_aptunstable
}

# Link against standard math library.
LIBS += -lm

# Install prefix.
isEmpty(PREFIX) {
    freebsd-*: PREFIX = /usr/local
         else: PREFIX = /usr
}

# Binary location.
DENG_BIN_DIR = $$PREFIX/bin

# Library location.
isEmpty(DENG_LIB_DIR) {
    deng_sdk: DENG_LIB_DIR = $$findLibDir($$DENG_SDK_DIR)
        else: DENG_LIB_DIR = $$findLibDir($$PREFIX)
}

# Target location for plugin libraries.
DENG_PLUGIN_LIB_DIR = $$DENG_LIB_DIR/doomsday

# When installing libraries to a non-standard location, instruct
# the linker where to find them.
!contains(DENG_LIB_DIR, ^/usr/.*): QMAKE_LFLAGS += -Wl,-rpath,$$DENG_LIB_DIR

DENG_BASE_DIR = $$PREFIX/share/doomsday
DENG_DATA_DIR = $$DENG_BASE_DIR/data

DEFINES += DENG_BASE_DIR=\"\\\"$${DENG_BASE_DIR}/\\\"\"
DEFINES += DENG_LIBRARY_DIR=\"\\\"$${DENG_PLUGIN_LIB_DIR}/\\\"\"

echo(Binary directory: $$DENG_BIN_DIR)
echo(Library directory: $$DENG_LIB_DIR)
echo(Plugin directory: $$DENG_PLUGIN_LIB_DIR)
echo(Doomsday base directory: $$DENG_BASE_DIR)
