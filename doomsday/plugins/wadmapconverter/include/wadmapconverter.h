/**
 * @file wadmapconverter.h
 * Map converter plugin for id tech 1 format maps. @ingroup wadmapconverter
 *
 * The purpose of a wadmapconverter plugin is to transform a map into
 * Doomsday's native map format by use of the public map editing interface.
 *
 * @authors Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

/**
 * @defgroup wadmapconverter
 * WAD map converter plugin.
 */

#ifndef __WADMAPCONVERTER_H__
#define __WADMAPCONVERTER_H__

#include <stdio.h>
#include <cassert>
#include <iostream>
#include <string.h>
#include "doomsday.h"
#include "dd_plugin.h"

#ifdef WIN32
#  define stricmp _stricmp
#  define strnicmp _strnicmp
#endif

#ifdef DENG_WADMAPCONVERTER_DEBUG
#  define WADMAPCONVERTER_TRACE(args)  std::cerr << "[WadMapConverter] " << args << std::endl;
#else
#  define WADMAPCONVERTER_TRACE(args)
#endif

typedef enum {
    MF_UNKNOWN              = -1,
    MF_DOOM                 = 0,
    MF_HEXEN,
    MF_DOOM64,
    NUM_MAPFORMATS
} mapformatid_t;

#define VALID_MAPFORMATID(v)        ((v) >= MF_DOOM && (v) < NUM_MAPFORMATS)

extern int DENG_PLUGIN_GLOBAL(verbose);

#define VERBOSE(code)   { if(DENG_PLUGIN_GLOBAL(verbose) >= 1) { code; } }
#define VERBOSE2(code)  { if(DENG_PLUGIN_GLOBAL(verbose) >= 2) { code; } }

#endif /* end of include guard: __WADMAPCONVERTER_H__ */
