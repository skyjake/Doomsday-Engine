# WAD Creation Tool for Doomsday
# (c) 2005-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
# License: GPL 2.0+

cmake_minimum_required (VERSION 3.1)
project (DENG_WADTOOL)
include (../../cmake/Config.cmake)

find_package (DengLegacy QUIET)

add_executable (wadtool wadtool.c wadtool.h)
set_property (TARGET wadtool PROPERTY FOLDER Tools)
target_link_libraries (wadtool Deng::liblegacy)

deng_target_rpath (wadtool)
deng_install_tool (wadtool)
