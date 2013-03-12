# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

# This project file contains tasks that are done in the beginning of a build.

TEMPLATE = subdirs

# Let's print the build configuration during this qmake invocation.
CONFIG += deng_verbosebuildconfig

include(../config.pri)

# We are not building any binaries here; disable stripping.
QMAKE_STRIP = true

# Update the PK3 files.
!deng_nopackres {
    system(cd $$PWD/scripts/ && python packres.py --quiet \"$$OUT_PWD/..\")
}

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

    # Make may not have yet created the output directory at this point.
    system(mkdir -p \"$$OUT_PWD\")

    isEmpty(SCRIPT_PYTHON) {
        error("Variable SCRIPT_PYTHON not set (path of Python interpreter to be used in generated scripts)")
    }

    # Generate a script for starting the laucher.
    LAUNCH_FILE = launch-doomsday
    !system(sed \"s:PYTHON:$$SCRIPT_PYTHON:; s:SB_DIR:$$SB_DIR:\" \
        <\"../../distrib/linux/$$LAUNCH_FILE\" \
        >\"$$OUT_PWD/$$LAUNCH_FILE\" && \
        chmod 755 \"$$OUT_PWD/$$LAUNCH_FILE\"): error(Can\'t build $$LAUNCH_FILE)
    launch.files = $$OUT_PWD/$$LAUNCH_FILE
    launch.path = $$DENG_BIN_DIR

    # Generate a .desktop file for the applications menu.
    DESKTOP_FILE = doomsday-engine.desktop
    !system(sed \"s:BIN_DIR:$$DENG_BIN_DIR:; s:SB_DIR:$$SB_DIR:\" \
        <\"../../distrib/linux/$$DESKTOP_FILE\" \
        >\"$$OUT_PWD/$$DESKTOP_FILE\"): error(Can\'t build $$DESKTOP_FILE)
    desktop.files = $$OUT_PWD/$$DESKTOP_FILE
    desktop.path = $$PREFIX/share/applications

    INSTALLS += conf plugins sb launch desktop
}

deng_aptunstable {
    # Include the Unstable repository for apt.
    INSTALLS += repo

    repo.files += ../../distrib/linux/doomsday-builds-unstable.list
    repo.path += /etc/apt/sources.list.d
}

deng_aptstable {
    # Include the Stable repository for apt.
    INSTALLS += repo

    repo.files += ../../distrib/linux/doomsday-builds-stable.list
    repo.path += /etc/apt/sources.list.d
}
