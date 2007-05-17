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
 * dam_main.h: Doomsday Archived Map (DAM) reader
 *
 * Engine-internal header for DAM.
 */

#ifndef __DOOMSDAY_ARCHIVED_MAP_H__
#define __DOOMSDAY_ARCHIVED_MAP_H__

// number of map data lumps for a level
#define NUM_MAPLUMPS 12

// well, there is GL_PVIS too but we arn't interested in that
#define NUM_GLLUMPS 5

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
    DAM_SEG,
    DAM_SUBSECTOR,
    DAM_NODE,
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

    DAM_TOP_TEXTURE_OFFSET_X,
    DAM_TOP_TEXTURE_OFFSET_Y,
    DAM_MIDDLE_TEXTURE_OFFSET_X,
    DAM_MIDDLE_TEXTURE_OFFSET_Y,
    DAM_BOTTOM_TEXTURE_OFFSET_X,
    DAM_BOTTOM_TEXTURE_OFFSET_Y,
    DAM_TOP_TEXTURE,
    DAM_MIDDLE_TEXTURE,
    DAM_BOTTOM_TEXTURE,
    DAM_FRONT_SECTOR,

    DAM_FLOOR_HEIGHT,
    DAM_FLOOR_TEXTURE,
    DAM_CEILING_HEIGHT,
    DAM_CEILING_TEXTURE,
    DAM_LIGHT_LEVEL,

    DAM_ANGLE,
    DAM_OFFSET,
    DAM_BACK_SEG,

    DAM_SEG_COUNT,
    DAM_SEG_FIRST,

    DAM_BBOX_RIGHT_TOP_Y,
    DAM_BBOX_RIGHT_LOW_Y,
    DAM_BBOX_RIGHT_LOW_X,
    DAM_BBOX_RIGHT_TOP_X,
    DAM_BBOX_LEFT_TOP_Y,
    DAM_BBOX_LEFT_LOW_Y,
    DAM_BBOX_LEFT_LOW_X,
    DAM_BBOX_LEFT_TOP_X,
    DAM_CHILD_RIGHT,
    DAM_CHILD_LEFT,
    NUM_DAM_PROPERTIES
};

typedef struct mapdatalumpformat_s {
    int         hversion;
    char       *magicid;
    char       *formatName;
    boolean     isText;     // True if the lump is a plain text lump.
} mapdatalumpformat_t;

typedef struct mapdatalumpinfo_s {
    int         lumpNum;
    byte       *lumpp;      // ptr to the lump data
    mapdatalumpformat_t *format;
    int         lumpClass;
    int         startOffset;
    uint        elements;
    size_t      length;
} mapdatalumpinfo_t;

typedef struct {
    char       *lumpname;
    int         mdLump;
    int         glLump;
    int         dataType;
    int         lumpclass;
    int         required;
    boolean precache;
} maplumpinfo_t;

typedef struct {
    uint        id;
    int         valueType;
} selectprop_t;

extern int      createBMap;
extern int      createReject;

void        DAM_Register(void);

void        DAM_Init(void);
void        DAM_LockCustomPropertys(void);

const char* DAM_Str(int prop);
maplumpinfo_t* DAM_MapLumpInfoForLumpClass(int lumpClass);
long        DAM_VertexIdx(long idx);

uint        P_RegisterCustomMapProperty(int type, valuetype_t dataType,
                                        char *name);

boolean     P_AttemptMapLoad(char *levelId);
#endif
