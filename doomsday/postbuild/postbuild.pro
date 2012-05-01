# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

# This project file contains tasks that are done in the end of a build.

TEMPLATE = subdirs

include(../config.pri)

# We are not building any binaries here; disable stripping.
QMAKE_STRIP = true

macx {
    QMAKE_EXTRA_TARGETS += bundleapp
    
    bundleapp.target = FORCE
    bundleapp.commands = \
        cd "$$OUT_PWD/.." && sh "$$PWD/bundleapp.sh" "$$PWD/.."
}
