# The Doomsday Engine Project -- Test Suite
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.1)
project (Tests)
include (../cmake/Config.cmake)

option (DENG_ENABLE_TESTS "Enable/disable the test suite" OFF)

if (DENG_ENABLE_TESTS)
    add_subdirectory (test_archive)
    add_subdirectory (test_bitfield)
    add_subdirectory (test_commandline)
    add_subdirectory (test_info)
    add_subdirectory (test_log)
    add_subdirectory (test_record)
    add_subdirectory (test_script)
    add_subdirectory (test_string)
    add_subdirectory (test_stringpool)
    add_subdirectory (test_vectors)
    if (DENG_ENABLE_GUI)
        add_subdirectory (test_appfw)
        add_subdirectory (test_glsandbox)
    endif ()
endif ()
