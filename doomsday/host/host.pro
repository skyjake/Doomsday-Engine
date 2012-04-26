# The Doomsday Engine Project
# Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012 Daniel Swanson <danij@dengine.net>

include(../config.pri)

TEMPLATE = subdirs

# We are not building any binaries here; disable stripping.
QMAKE_STRIP = true

# The host is not installed by default.
deng_host {
    INSTALLS += host

    unix {
        host.files += doomsday-host
        host.path = $$DENG_BIN_DIR
    }
}

OTHER_FILES += \
    doomsday-host \
    doomsdayhostrc-example
