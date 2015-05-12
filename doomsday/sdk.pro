# The Doomsday Engine Project
# Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>

greaterThan(QT_MAJOR_VERSION, 4): cache()

!deng_sdk: error(\"deng_sdk\" must be defined in CONFIG!)

include(config.pri)

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = \
    build \
    libcore \
    libshell

!deng_noclient|macx {
    SUBDIRS += \
        libgui \
        libappfw 
}

SUBDIRS += \
    tests

OTHER_FILES += doomsday_sdk.pri

# Install the main .pri file of the SDK.
INSTALLS += sdk_pri
sdk_pri.files = doomsday_sdk.pri
sdk_pri.path = $$DENG_SDK_DIR
