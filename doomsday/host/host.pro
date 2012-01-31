# The Doomsday Engine Project
# Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012 Daniel Swanson <danij@dengine.net>

include(../config.pri)

# The host is not installed by default.
deng_host {
	INSTALLS += host
}

unix {
	host.files += doomsday-host
	host.path = $$DENG_BIN_DIR
}
