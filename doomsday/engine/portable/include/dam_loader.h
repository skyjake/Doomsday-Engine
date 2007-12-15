/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * dam_loader.h: Doomsday Archived Map (DAM) reader
 */

#ifndef __DOOMSDAY_ARCHIVED_MAP_LOADER_H__
#define __DOOMSDAY_ARCHIVED_MAP_LOADER_H__

#include "dam_main.h"
#include "p_mapdata.h"

// Common map format properties.
enum {
    DAM_UNKNOWN = -2,

    DAM_ALL = -1,
    DAM_NONE,

    // Object/Data types
    DAM_THING,
    DAM_VERTEX,
    DAM_LINE,
    DAM_SIDE,
    DAM_SECTOR,
    DAM_MAPBLOCK,
    DAM_SECREJECT,
    DAM_ACSSCRIPT,

    // Object properties
    DAM_X,
    DAM_Y,
    DAM_DX,
    DAM_DY,

    DAM_VERTEX1,
    DAM_VERTEX2,
    DAM_FLAGS,
    DAM_SIDE0,
    DAM_SIDE1,

    DAM_MATERIAL_OFFSET_X,
    DAM_MATERIAL_OFFSET_Y,
    DAM_TOP_MATERIAL,
    DAM_MIDDLE_MATERIAL,
    DAM_BOTTOM_MATERIAL,
    DAM_FRONT_SECTOR,

    DAM_FLOOR_HEIGHT,
    DAM_FLOOR_MATERIAL,
    DAM_CEILING_HEIGHT,
    DAM_CEILING_MATERIAL,
    DAM_LIGHT_LEVEL,
    NUM_DAM_PROPERTIES
};

typedef struct {
    char       *lumpname;
    int         mdLump;
    int         dataType;
    int         lumpclass;
    boolean     required;
} mapdatalump_t;

const char* DAM_Str(int prop);
int         DAM_DataTypeForLumpClass(int lumpClass);

#endif
