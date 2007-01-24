/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * dam_read.h: Doomsday Archived Map (DAM), reader
 *
 * Engine-internal header for DAM.
 */

#ifndef __DOOMSDAY_ARCHIVED_MAP_READ_H__
#define __DOOMSDAY_ARCHIVED_MAP_READ_H__

#include "de_play.h"

// Data type flags
#define DT_UNSIGNED   0x01
#define DT_FRACBITS   0x02
#define DT_FLAT       0x04
#define DT_TEXTURE    0x08
#define DT_NOINDEX    0x10
#define DT_MSBCONVERT 0x20

typedef struct {
    // Dest
    uint        id;
    int         type;
    // Src
    int         flags;
    int         size;   // num of bytes
    int         offset;
} readprop_t;

boolean DAM_ReadMapDataFromLump(gamemap_t *map, mapdatalumpinfo_t *mapLump,
                                uint startIndex, readprop_t *props,
                                uint numProps);
#endif
