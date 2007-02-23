/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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
 * dam_read.c: Doomsday Archived Map (DAM), reader.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_dam.h"
#include "de_defs.h"
#include "de_misc.h"

#include "p_mapdata.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// Game specific map format properties
// TODO: These need to be identified from DED.
enum {
    DAM_LINE_TAG = NUM_DAM_PROPERTIES,
    DAM_LINE_SPECIAL,
    DAM_SECTOR_TAG,
    DAM_SECTOR_SPECIAL,
    DAM_THING_X,
    DAM_THING_Y,
    DAM_THING_ANGLE,
    DAM_THING_TYPE,
    DAM_THING_OPTIONS,
    DAM_THING_HEIGHT,
    DAM_THING_TID,
    DAM_LINE_ARG1,
    DAM_LINE_ARG2,
    DAM_LINE_ARG3,
    DAM_LINE_ARG4,
    DAM_LINE_ARG5,
    DAM_THING_SPECIAL,
    DAM_THING_ARG1,
    DAM_THING_ARG2,
    DAM_THING_ARG3,
    DAM_THING_ARG4,
    DAM_THING_ARG5,
    DAM_PROPERTY_COUNT
};

typedef struct {
    gamemap_t *map;
    size_t      elmsize;
    uint        elements;
    uint        numProps;
    readprop_t *props;
} damargs_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void *DAM_IndexToPtr(gamemap_t *map, int objectType, uint id);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int P_CallbackEX(int dataType, unsigned int startIndex,
                        const byte *buffer, void* context,
                        int (*callback)(gamemap_t  *map, int dataType,
                                void* ptr, uint elmIdx, const readprop_t* prop,
                                const byte *buffer));

static int ReadMapProperty(gamemap_t  *map, int dataType, void* ptr,
                           uint elmIdx, const readprop_t* prop,
                           const byte *buffer);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

boolean DAM_ReadMapDataFromLump(gamemap_t *map, mapdatalumpinfo_t *mapLump,
                                uint startIndex, readprop_t *props,
                                uint numProps)
{
    int         type = DAM_MapLumpInfoForLumpClass(mapLump->lumpClass)->dataType;
    damargs_t   args;

    // Is this a supported lump type?
    switch(type)
    {
    case DAM_THING:
    case DAM_VERTEX:
    case DAM_LINE:
    case DAM_SIDE:
    case DAM_SECTOR:
    case DAM_SEG:
    case DAM_SUBSECTOR:
    case DAM_NODE:
        break;

    default:
        return false; // Read from this lump type is not supported.
    }

     // Select the lump size, number of elements etc...
    args.map = map;
    args.elmsize = Def_GetMapLumpFormat(mapLump->format->formatName)->elmsize;
    args.elements = mapLump->elements;
    args.numProps = numProps;
    args.props = props;

    // Have we cached the lump yet?
    if(mapLump->lumpp == NULL)
        mapLump->lumpp = (byte *) W_CacheLumpNum(mapLump->lumpNum, PU_STATIC);

    // Read in that data!
    // NOTE: We'll leave the lump cached, our caller probably knows better
    // than us whether it should be free'd.
    return P_CallbackEX(type, startIndex,
                        (mapLump->lumpp + mapLump->startOffset), &args,
                        ReadMapProperty);
}

/**
 * Reads a value from the (little endian) source buffer. Does some basic
 * type checking so that incompatible types are not assigned.
 * Simple conversions are also done, e.g., float to fixed.
 */
static void ReadValue(gamemap_t* map, valuetype_t valueType, void* dst,
                      const byte *src, const readprop_t* prop, uint element)
{
    int flags = prop->flags;

    if(valueType == DDVT_BYTE)
    {
        byte* d = dst;
        switch(prop->size)
        {
        case 1:
            *d = *src;
            break;
        case 2:
            *d = *src;
            break;
        case 4:
            *d = *src;
            break;
        default:
            Con_Error("ReadValue: DDVT_BYTE incompatible with value type %s\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        float* d = dst;
        switch(prop->size)
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = FIX2FLT(USHORT(*((short*)(src))) << FRACBITS);
                else
                    *d = FIX2FLT(USHORT(*((short*)(src))));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = FIX2FLT(SHORT(*((short*)(src))) << FRACBITS);
                else
                    *d = FIX2FLT(SHORT(*((short*)(src))));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = FIX2FLT(ULONG(*((long*)(src))) << FRACBITS);
                else
                    *d = FIX2FLT(ULONG(*((long*)(src))));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = FIX2FLT(LONG(*((long*)(src))) << FRACBITS);
                else
                    *d = FIX2FLT(LONG(*((long*)(src))));
            }
            break;

        default:
            Con_Error("ReadValue: DDVT_FLOAT incompatible with value type %s\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short* d = dst;
        switch(prop->size)
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = SHORT(*((short*)(src)));
            }
            break;

        case 8:
            {
            if(flags & DT_TEXTURE)
            {
                *d = P_CheckTexture((char*)((long long*)(src)), false, valueType,
                                    element, prop->id);
            }
            else if(flags & DT_FLAT)
            {
                *d = P_CheckTexture((char*)((long long*)(src)), true, valueType,
                                    element, prop->id);
            }
            break;
            }
         default:
            Con_Error("ReadValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(prop->size));
         }
    }
    else if(valueType == DDVT_FIXED)
    {
        fixed_t* d = dst;

        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = SHORT(*((short*)(src)));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                *d = ULONG(*((long*)(src)));
            else
                *d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_ULONG)
    {
        unsigned long* d = dst;

        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = SHORT(*((short*)(src)));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                *d = ULONG(*((long*)(src)));
            else
                *d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_ULONG incompatible with value type %s.\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_UINT)
    {
        unsigned int* d = dst;

        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((short*)(src)));

                    *d = NO_INDEX;

                    if(num != ((unsigned short)-1))
                        *d = num;
                }
                else
                {
                    if(flags & DT_FRACBITS)
                        *d = SHORT(*((short*)(src))) << FRACBITS;
                    else
                        *d = USHORT(*((short*)(src)));
                }
            }
            if((flags & DT_MSBCONVERT) && (*d & 0x8000))
            {
                *d &= ~0x8000;
                *d |= 0x80000000;
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                *d = ULONG(*((long*)(src)));
            else
                *d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_INT)
    {
        int* d = dst;

        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((short*)(src)));

                    *d = NO_INDEX;

                    if(num != ((unsigned short)-1))
                        *d = num;
                }
                else
                {
                    if(flags & DT_FRACBITS)
                        *d = SHORT(*((short*)(src))) << FRACBITS;
                    else
                        *d = SHORT(*((short*)(src)));
                }
            }
            if((flags & DT_MSBCONVERT) && (*d & 0x8000))
            {
                *d &= ~0x8000;
                *d |= 0x80000000;
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                *d = ULONG(*((long*)(src)));
            else
                *d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t* d = dst;
        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_FRACBITS)
                *d = (angle_t) (SHORT(*((short*)(src))) << FRACBITS);
            else
                *d = (angle_t) SHORT(*((short*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(prop->size));
        }
    }
    // TODO:
    // Once we have a way to convert internal member to property we should no
    // longer need these special case constants (eg DDVT_SECT_PTR).
    else if(valueType == DDVT_SECT_PTR || valueType == DDVT_VERT_PTR ||
            valueType == DDVT_LINE_PTR || valueType == DDVT_SIDE_PTR ||
            valueType == DDVT_SEG_PTR)
    {
        long idx = NO_INDEX;

        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                idx = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((short*)(src)));

                    if(num != ((unsigned short)-1))
                        idx = num;
                }
                else
                {
                    idx = SHORT(*((short*)(src)));
                }
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                idx = ULONG(*((long*)(src)));
            else
                idx = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: %s incompatible with value type %s.\n",
                      valueType == DDVT_SECT_PTR? "DDVT_SECT_PTR" :
                      valueType == DDVT_VERT_PTR? "DDVT_VERT_PTR" :
                      valueType == DDVT_SIDE_PTR? "DDVT_SIDE_PTR" :
                      valueType == DDVT_SEG_PTR? "DDVT_SEG_PTR" :
                      "DDVT_LINE_PTR", value_Str(prop->size));
        }

        switch(valueType)
        {
        case DDVT_LINE_PTR:
            *(line_t **) dst = DAM_IndexToPtr(map, DAM_LINE, (unsigned) idx);
            break;

        case DDVT_SIDE_PTR:
            *(side_t **) dst = DAM_IndexToPtr(map, DAM_SIDE, (unsigned) idx);
            break;

        case DDVT_SECT_PTR:
            *(sector_t **) dst = DAM_IndexToPtr(map, DAM_SECTOR, (unsigned) idx);
            break;

        case DDVT_SEG_PTR:
            *(seg_t **) dst = DAM_IndexToPtr(map, DAM_SEG, (unsigned) idx);
            break;

        case DDVT_VERT_PTR:
            // FIXME:
            // There has to be a better way to do this...
            idx = DAM_VertexIdx(idx);
            // end FIXME
            *(vertex_t **) dst = DAM_IndexToPtr(map, DAM_VERTEX, (unsigned) idx);
            break;

        default:
            // TODO: Need to react?
            break;
        }
    }
    else
    {
        Con_Error("ReadValue: unknown value type %s.\n", value_Str(valueType));
    }
}

static int ReadCustomMapProperty(gamemap_t* map, int dataType, void *ptr,
                                 uint elmIdx, const readprop_t* prop,
                                 const byte *src)
{
    byte        tmpbyte = 0;
    short       tmpshort = 0;
    fixed_t     tmpfixed = 0;
    int         tmpint = 0;
    float       tmpfloat = 0;
    void       *dest = NULL;

    switch(prop->type)
    {
    case DDVT_BYTE:
        dest = &tmpbyte;
        break;
    case DDVT_SHORT:
        dest = &tmpshort;
        break;
    case DDVT_FIXED:
        dest = &tmpfixed;
        break;
    case DDVT_INT:
        dest = &tmpint;
        break;
    case DDVT_FLOAT:
        dest = &tmpfloat;
        break;
    default:
        Con_Error("ReadCustomMapProperty: Unsupported data type id %s.\n",
                  value_Str(prop->type));
    };

    ReadValue(map, prop->type, dest, src, prop, elmIdx);
    gx.HandleMapDataProperty(elmIdx, dataType, prop->id, prop->type, dest);

    return true;
}

static int ReadMapProperty(gamemap_t* map, int dataType, void* ptr,
                           uint elmIdx, const readprop_t* prop, const byte *src)
{
    // Handle unknown (game specific) properties.
    if(prop->id >= NUM_DAM_PROPERTIES)
        return ReadCustomMapProperty(map, dataType, ptr, elmIdx, prop, src);

    // These are the exported map data properties that can be
    // assigned to when reading map data.
    switch(dataType)
    {
    case DAM_VERTEX:
        {
        vertex_t* p = ptr;

        switch(prop->id)
        {
        case DAM_X:
            ReadValue(map, DMT_VERTEX_POS, &p->pos[VX], src, prop, elmIdx);
            break;

        case DAM_Y:
            ReadValue(map, DMT_VERTEX_POS, &p->pos[VY], src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_VERTEX has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_LINE:
        {
        line_t* p = ptr;

        switch(prop->id)
        {
        case DAM_VERTEX1:
            // TODO: should be DMT_LINE_V1 but we require special case logic
            ReadValue(map, DDVT_VERT_PTR, &p->L_v1, src, prop, elmIdx);
            break;

        case DAM_VERTEX2:
            // TODO: should be DMT_LINE_V2 but we require special case logic
            ReadValue(map, DDVT_VERT_PTR, &p->L_v2, src, prop, elmIdx);
            break;

        case DAM_FLAGS:
            ReadValue(map, DMT_LINE_FLAGS, &p->flags, src, prop, elmIdx);
            break;

        case DAM_SIDE0:
            ReadValue(map, DDVT_SIDE_PTR, &p->L_frontside, src, prop, elmIdx);
            break;

        case DAM_SIDE1:
            ReadValue(map, DDVT_SIDE_PTR, &p->L_backside, src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_LINE has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_SIDE:
        {
        side_t* p = ptr;

        switch(prop->id)
        {
        case DAM_TOP_TEXTURE_OFFSET_X:
            ReadValue(map, DMT_SURFACE_OFFX, &p->SW_topoffx, src, prop, elmIdx);
            break;

        case DAM_TOP_TEXTURE_OFFSET_Y:
            ReadValue(map, DMT_SURFACE_OFFY, &p->SW_topoffy, src, prop, elmIdx);
            break;

        case DAM_MIDDLE_TEXTURE_OFFSET_X:
            ReadValue(map, DMT_SURFACE_OFFX, &p->SW_middleoffx, src, prop, elmIdx);
            break;

        case DAM_MIDDLE_TEXTURE_OFFSET_Y:
            ReadValue(map, DMT_SURFACE_OFFY, &p->SW_middleoffy, src, prop, elmIdx);
            break;

        case DAM_BOTTOM_TEXTURE_OFFSET_X:
            ReadValue(map, DMT_SURFACE_OFFX, &p->SW_bottomoffx, src, prop, elmIdx);
            break;

        case DAM_BOTTOM_TEXTURE_OFFSET_Y:
            ReadValue(map, DMT_SURFACE_OFFY, &p->SW_bottomoffy, src, prop, elmIdx);
            break;

        case DAM_TOP_TEXTURE:
            ReadValue(map, DMT_SURFACE_TEXTURE, &p->SW_toppic, src, prop, elmIdx);
            break;

        case DAM_MIDDLE_TEXTURE:
            ReadValue(map, DMT_SURFACE_TEXTURE, &p->SW_middlepic, src, prop, elmIdx);
            break;

        case DAM_BOTTOM_TEXTURE:
            ReadValue(map, DMT_SURFACE_TEXTURE, &p->SW_bottompic, src, prop, elmIdx);
            break;

        case DAM_FRONT_SECTOR:
            // TODO: should be DMT_SIDE_SECTOR but we require special case logic
            ReadValue(map, DDVT_SECT_PTR, &p->sector, src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_SIDE has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_SECTOR:
        {
        sector_t* p = ptr;

        switch(prop->id)
        {
        case DAM_FLOOR_HEIGHT:
            ReadValue(map, DMT_PLANE_HEIGHT, &p->SP_floorheight, src, prop, elmIdx);
            break;

        case DAM_CEILING_HEIGHT:
            ReadValue(map, DMT_PLANE_HEIGHT, &p->SP_ceilheight, src, prop, elmIdx);
            break;

        case DAM_FLOOR_TEXTURE:
            ReadValue(map, DMT_SURFACE_TEXTURE, &p->SP_floorpic, src, prop, elmIdx);
            break;

        case DAM_CEILING_TEXTURE:
            ReadValue(map, DMT_SURFACE_TEXTURE, &p->SP_ceilpic, src, prop, elmIdx);
            break;

        case DAM_LIGHT_LEVEL:
            ReadValue(map, DMT_SECTOR_LIGHTLEVEL, &p->lightlevel, src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_SECTOR has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_SEG:
        {
        seg_t* p = ptr;

        switch(prop->id)
        {
        case DAM_VERTEX1:
            // TODO: should be DMT_SEG_V  but we require special case logic
            ReadValue(map, DDVT_VERT_PTR, &p->SG_v1, src, prop, elmIdx);
            break;

        case DAM_VERTEX2:
            // TODO: should be DMT_SEG_V  but we require special case logic
            ReadValue(map, DDVT_VERT_PTR, &p->SG_v2, src, prop, elmIdx);
            break;

        case DAM_ANGLE:
            ReadValue(map, DMT_SEG_ANGLE, &p->angle, src, prop, elmIdx);
            break;

        case DAM_LINE:
            // KLUDGE: Set the data type implicitly as DAM_LINE is DDVT_PTR
            ReadValue(map, DDVT_LINE_PTR, &p->linedef, src, prop, elmIdx);
            break;

        case DAM_SIDE:
            ReadValue(map, DMT_SEG_SIDE, &p->side, src, prop, elmIdx);
            break;

        case DAM_OFFSET:
            ReadValue(map, DMT_SEG_OFFSET, &p->offset, src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_SEG has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_SUBSECTOR:
        {
        subsector_t* p = ptr;

        switch(prop->id)
        {
        case DAM_SEG_COUNT:
            ReadValue(map, DMT_SUBSECTOR_SEGCOUNT, &p->segcount, src, prop, elmIdx);
            break;

        case DAM_SEG_FIRST:
            // TODO: should be DMT_SUBSECTOR_FIRSTSEG  but we require special case logic.
            ReadValue(map, DDVT_SEG_PTR, &p->firstseg, src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_SUBSECTOR has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_NODE:
        {
        node_t* p = ptr;

        switch(prop->id)
        {
        case DAM_X:
            ReadValue(map, DMT_NODE_X, &p->x, src, prop, elmIdx);
            break;

        case DAM_Y:
            ReadValue(map, DMT_NODE_Y, &p->y, src, prop, elmIdx);
            break;

        case DAM_DX:
            ReadValue(map, DMT_NODE_DX, &p->dx, src, prop, elmIdx);
            break;

        case DAM_DY:
            ReadValue(map, DMT_NODE_DY, &p->dy, src, prop, elmIdx);
            break;

        // TODO: the following should use DMT_NODE_?
        // Constants not defined as yet by the maptypes script.
        case DAM_BBOX_RIGHT_TOP_Y:
            ReadValue(map, DDVT_FLOAT, &p->bbox[0][0], src, prop, elmIdx);
            break;

        case DAM_BBOX_RIGHT_LOW_Y:
            ReadValue(map, DDVT_FLOAT, &p->bbox[0][1], src, prop, elmIdx);
            break;

        case DAM_BBOX_RIGHT_LOW_X:
            ReadValue(map, DDVT_FLOAT, &p->bbox[0][2], src, prop, elmIdx);
            break;

        case DAM_BBOX_RIGHT_TOP_X:
            ReadValue(map, DDVT_FLOAT, &p->bbox[0][3], src, prop, elmIdx);
            break;

        case DAM_BBOX_LEFT_TOP_Y:
            ReadValue(map, DDVT_FLOAT, &p->bbox[1][0], src, prop, elmIdx);
            break;

        case DAM_BBOX_LEFT_LOW_Y:
            ReadValue(map, DDVT_FLOAT, &p->bbox[1][1], src, prop, elmIdx);
            break;

        case DAM_BBOX_LEFT_LOW_X:
            ReadValue(map, DDVT_FLOAT, &p->bbox[1][2], src, prop, elmIdx);
            break;

        case DAM_BBOX_LEFT_TOP_X:
            ReadValue(map, DDVT_FLOAT, &p->bbox[1][3], src, prop, elmIdx);
            break;

        case DAM_CHILD_RIGHT:
            ReadValue(map, DDVT_UINT, &p->children[0], src, prop, elmIdx);
            break;

        case DAM_CHILD_LEFT:
            ReadValue(map, DDVT_UINT, &p->children[1], src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_NODE has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    default:
        Con_Error("ReadMapProperty: Type cannot be assigned to from a map format.\n");
    }

    return true; // Continue iteration
}

/*
 * Make multiple calls to a callback function on a selection of archived
 * map data objects.
 *
 * This function is essentially the same as P_Callback in p_dmu.c but with
 * the following key differences:
 *
 *  1  Multiple callbacks can be made for each object.
 *  2  Any number of properties (of different types) per object
 *     can be manipulated. To accomplish the same result using
 *     P_Callback would require numerous rounds of iteration.
 *  3  Optimised for bulk processing.
 *
 * Returns true if all the calls to the callback function return true. False
 * is returned when the callback function returns false; in this case, the
 * iteration is aborted immediately when the callback function returns false.
 *
 * NOTE: Not very pretty to look at but it IS pretty quick :-)
 *
 * NOTE2: I would suggest these manual optimizations be removed. The compiler
 *       is pretty good in unrolling loops, if need be. -jk
 *
 * TODO: Make required changes to make signature compatible with the other
 *       P_Callback function (may need to add a dummy parameter to P_Callback
 *       since (byte*) buffer needs to be accessed here.).
 *       Do NOT access the contents of (void*) context since we can't be
 *       sure what it is.
 *       DAM specific parameters could be passed to the callback function by
 *       using a (void*) context parameter. This would allow our callbacks to
 *       use the same signature as the DMU callbacks.
 *       Think of a way to combine index and startIndex.
 */
static int P_CallbackEX(int dataType, uint startIndex, const byte *buffer,
                        void* context,
                        int (*callback)(gamemap_t* map, int dataType,
                                 void* ptr, uint elmIdx, const readprop_t* prop,
                                 const byte *buffer))
{
#define NUMBLOCKS 8
#define BLOCK ptr = dataType == DAM_THING? &idx : DAM_IndexToPtr(map, dataType, idx);  \
        for(prop = &args->props[k = 0]; k < args->numProps; prop = &args->props[++k]) \
        { \
            if(!callback(map, dataType, ptr, idx, prop, buffer + prop->offset)) \
                return false; \
        } \
        buffer += args->elmsize; \
        ++idx;

    uint        idx;
    uint        i = 0, k;
    damargs_t  *args = (damargs_t*) context;
    gamemap_t *map = args->map;
    uint        blockLimit = (args->elements / NUMBLOCKS) * NUMBLOCKS;
    void       *ptr;
    const readprop_t *prop;

    // Have we got enough to do some in blocks?
    if(args->elements >= blockLimit)
    {
        idx = startIndex + i;
        while(i < blockLimit)
        {
            BLOCK BLOCK BLOCK BLOCK BLOCK BLOCK BLOCK BLOCK

            i += NUMBLOCKS;
        }
    }

    // Have we got any left to do?
    if(i < args->elements)
    {
        // Yes, jump in at the number of elements remaining
        idx = startIndex + i;
        switch(args->elements - i)
        {
        case 7: BLOCK
        case 6: BLOCK
        case 5: BLOCK
        case 4: BLOCK
        case 3: BLOCK
        case 2: BLOCK
        case 1:
            ptr = dataType == DAM_THING? &idx : DAM_IndexToPtr(map, dataType, idx);
            for(prop = &args->props[k = 0]; k < args->numProps; prop = &args->props[++k])
            {
                if(!callback(map, dataType, ptr, idx, prop, buffer + prop->offset))
                    return false;
            }
        }
    }

    return true;
}
