# The Doomsday Engine Project
# Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config.pri)

TEMPLATE = subdirs

deng_tests: SUBDIRS += \
    test_archive \
    test_bitfield \
    test_glsandbox \
    test_info \
    test_log \
    test_record \
    test_script \
    test_string \
    test_stringpool \
    test_vectors
    #basiclink
