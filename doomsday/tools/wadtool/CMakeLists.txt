# WAD Creation Tool for Doomsday
# (c) 2005-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
# License: GPL 2.0+

cmake_minimum_required (VERSION 3.0)
project (DENG_WADTOOL)
include (../../cmake/Config.cmake)

find_package (DengLegacy QUIET)

add_executable (wadtool wadtool.c wadtool.h)
target_link_libraries (wadtool Deng::deng_legacy)

install (TARGETS wadtool DESTINATION bin)
