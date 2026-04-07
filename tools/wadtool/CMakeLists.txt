# WAD Creation Tool for Doomsday
# (c) 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
# License: GPL 2.0+

cmake_minimum_required (VERSION 3.10)
project (DE_WADTOOL)
include (../../cmake/Config.cmake)

add_executable (wadtool wadtool.c wadtool.h)
set_property (TARGET wadtool PROPERTY FOLDER Tools)
deng_link_libraries (wadtool PRIVATE DengCore)

deng_target_rpath (wadtool)
deng_install_tool (wadtool)
