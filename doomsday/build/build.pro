# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = subdirs

include(../config.pri)

# This project file contains tasks that are done in the beginning of a build.

# Update the PK3 files.
system(cd $$PWD/scripts/ && python packres.py $$OUT_PWD/..)

# Install the launcher.
contains(DENG_CONFIG, installsb) {
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
        $${SB_ROOT}/lang
    sb.path = $$SB_DIR

    conf.files = \
        $${SB_ROOT}/conf/snowberry.conf \
        $${SB_ROOT}/conf/x-*.conf
    conf.path = $$SB_DIR/conf


    INSTALLS += sb conf
}
