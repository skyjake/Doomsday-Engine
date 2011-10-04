# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011 Daniel Swanson <danij@dengine.net>

# This project file contains tasks that are done in the beginning of a build.

TEMPLATE = subdirs

include(../config.pri)

# Update the PK3 files.
system(cd $$PWD/scripts/ && python packres.py --quiet \"$$OUT_PWD/..\")

# Install the launcher.
deng_snowberry {
    SB_ROOT = ../../snowberry
    SB_DIR = $$DENG_BASE_DIR/snowberry

    sb.files = \
        $${SB_ROOT}/cfparser.py \
        $${SB_ROOT}/events.py \
        $${SB_ROOT}/host.py \
        $${SB_ROOT}/language.py \
        $${SB_ROOT}/logger.py \
        $${SB_ROOT}/paths.py \
        $${SB_ROOT}/plugins.py \
        $${SB_ROOT}/snowberry.py \
        $${SB_ROOT}/ui.py \
        $${SB_ROOT}/widgets.py \
        $${SB_ROOT}/graphics \
        $${SB_ROOT}/lang \
        $${SB_ROOT}/profiles \
        $${SB_ROOT}/sb
    sb.path = $$SB_DIR

    conf.files = \
        $${SB_ROOT}/conf/snowberry.conf \
        $${SB_ROOT}/conf/x-*.conf
    conf.path = $$SB_DIR/conf

    plugins.files = \
        $${SB_ROOT}/plugins/about.py \
        $${SB_ROOT}/plugins/help.py \
        $${SB_ROOT}/plugins/launcher.py \
        $${SB_ROOT}/plugins/observer.py \
        $${SB_ROOT}/plugins/preferences.py \
        $${SB_ROOT}/plugins/profilelist.py \
        $${SB_ROOT}/plugins/tab* \
        $${SB_ROOT}/plugins/wizard.py
    plugins.path = $$SB_DIR/plugins

    # Include the launch script if it exists.
    LAUNCHER = ../../distrib/linux/launch-doomsday
    exists($$LAUNCHER) {
        launch.files = $$LAUNCHER
        launch.path = $$DENG_BIN_DIR
        INSTALLS += launch

        message(Installing the launch-doomsday script.)
    }

    INSTALLS += sb conf plugins
}

deng_aptunstable {
    # Include the Unstable repository for apt.
    INSTALLS += repo

    repo.files = ../../distrib/linux/doomsday-builds-unstable.list
    repo.path = /etc/apt/source.list.d
}
