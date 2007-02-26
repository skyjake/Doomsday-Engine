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

typedef struct {
    gamemap_t *map;
    size_t      elmsize;
    uint        elements;
    uint        numProps;
    readprop_t *props;
} damlumpreadargs_t;

typedef struct setargs_s {
    int         type;
    uint        prop;

    valuetype_t valueType;
    boolean    *booleanValues;
    byte       *byteValues;
    short      *shortValues;
    int        *intValues;
    uint       *uintValues;
    fixed_t    *fixedValues;
    unsigned long *ulongValues;
    float      *floatValues;
    angle_t    *angleValues;
    void      **ptrValues;
} damsetargs_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void *DAM_IndexToPtr(gamemap_t *map, int objectType, uint id);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int P_CallbackEX(int dataType, unsigned int startIndex,
                        const byte *buffer, void* context,
                        int (*callback)(void *ptr, int dataType,
                                gamemap_t *map, uint elmIdx, const readprop_t* prop,
                                const byte *src));

static int ReadMapProperty(void *ptr, int dataType, gamemap_t *map,
                           uint elmIdx, const readprop_t* prop,
                           const byte *src);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Initializes a damsetargs struct.
 *
 * @param type  Type of the map data object (e.g., DAM_LINE).
 * @param prop  Property of the map data object.
 */
static void initArgs(damsetargs_t* args, int type, uint prop)
{
    memset(args, 0, sizeof(*args));
    args->type = type;
    args->prop = prop;
}

boolean DAM_ReadMapDataFromLump(gamemap_t *map, mapdatalumpinfo_t *mapLump,
                                uint startIndex, readprop_t *props,
                                uint numProps)
{
    int         type = DAM_MapLumpInfoForLumpClass(mapLump->lumpClass)->dataType;
    damlumpreadargs_t   args;

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
 * Sets a value. Does some basic type checking so that incompatible types are
 * not assigned. Simple conversions are also done, e.g., float to fixed.
 */
static void SetValue(valuetype_t valueType, void* dst, damsetargs_t* args,
                     uint index, gamemap_t *map)
{
    if(valueType == DDVT_FIXED)
    {
        fixed_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            *d = (args->byteValues[index] << FRACBITS);
            break;
        case DDVT_INT:
            *d = (args->intValues[index] << FRACBITS);
            break;
        case DDVT_FIXED:
            *d = args->fixedValues[index];
            break;
        case DDVT_FLOAT:
            *d = FLT2FIX(args->floatValues[index]);
            break;
        default:
            Con_Error("SetValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        float* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_SHORT:
            *d = args->shortValues[index];
            break;
        case DDVT_UINT:
            *d = args->uintValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FIXED:
            *d = FIX2FLT(args->fixedValues[index]);
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BOOL)
    {
        boolean* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BOOL incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BYTE)
    {
        byte* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_SHORT:
            *d = args->shortValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = (byte) args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BYTE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_INT)
    {
        int* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_UINT)
    {
        uint* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = (unsigned) args->intValues[index];
            break;
        case DDVT_UINT:
            *d = args->uintValues[index];
            break;
        case DDVT_FLOAT:
            *d = (unsigned) args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (unsigned) (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_UINT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_SHORT:
            *d = args->shortValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_ULONG:
            *d = args->ulongValues[index];
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_ANGLE:
            *d = args->angleValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        blendmode_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_INT:
            if(args->intValues[index] > DDNUM_BLENDMODES || args->intValues[index] < 0)
                Con_Error("SetValue: %d is not a valid value for DDVT_BLENDMODE.\n",
                          args->intValues[index]);

            *d = args->intValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BLENDMODE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_PTR)
    {
        void** d = dst;

        switch(args->valueType)
        {
        case DDVT_SECT_IDX:
            *(sector_t **) dst = DAM_IndexToPtr(map, DAM_SECTOR, args->uintValues[index]);
            break;
        case DDVT_VERT_IDX:
            {
            uint         value;
            // FIXME:
            // There has to be a better way to do this...
            value = DAM_VertexIdx(args->uintValues[index]);
            // end FIXME
            *(vertex_t **) dst = DAM_IndexToPtr(map, DAM_VERTEX, value);
            }
            break;
        case DDVT_LINE_IDX:
            *(line_t **) dst = DAM_IndexToPtr(map, DAM_LINE, args->uintValues[index]);
            break;
        case DDVT_SIDE_IDX:
            *(side_t **) dst = DAM_IndexToPtr(map, DAM_SIDE, args->uintValues[index]);
            break;
        case DDVT_SEG_IDX:
            *(seg_t **) dst = DAM_IndexToPtr(map, DAM_SEG, args->uintValues[index]);
            break;
        case DDVT_PTR:
            *d = args->ptrValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_PTR incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else
    {
        Con_Error("SetValue: unknown value type %d.\n", valueType);
    }
}

/**
 * Reads a value from the (little endian) source buffer. Does some basic
 * type checking so that incompatible types are not assigned.
 * Simple conversions are also done, e.g., float to fixed.
 */
static void ReadValue(valuetype_t valueType, size_t size, const byte *src,
                      damsetargs_t *args, uint index, int flags,
                      uint element)
{
    if(valueType == DDVT_BYTE)
    {
        switch(size)
        {
        case 1:
        case 2:
        case 4:
            break;
        default:
            Con_Error("ReadValue: DDVT_BYTE no conversion from %i bytes.",
                      size);
        }
        args->byteValues[index] = *src;
    }
    else if(valueType == DDVT_FLOAT)
    {
        float       d;
        switch(size)
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = FIX2FLT(USHORT(*((short*)(src))) << FRACBITS);
                else
                    d = FIX2FLT(USHORT(*((short*)(src))));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    d = FIX2FLT(SHORT(*((short*)(src))) << FRACBITS);
                else
                    d = FIX2FLT(SHORT(*((short*)(src))));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = FIX2FLT(ULONG(*((long*)(src))) << FRACBITS);
                else
                    d = FIX2FLT(ULONG(*((long*)(src))));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    d = FIX2FLT(LONG(*((long*)(src))) << FRACBITS);
                else
                    d = FIX2FLT(LONG(*((long*)(src))));
            }
            break;

        default:
            Con_Error("ReadValue: DDVT_FLOAT no conversion from %i bytes.",
                      size);
        }
        args->floatValues[index] = d;
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short       d;
        switch(size)
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    d = SHORT(*((short*)(src)));
            }
            break;

        case 8:
            {
            if(flags & DT_TEXTURE)
            {
                d = P_CheckTexture((char*)((long long*)(src)), false, valueType,
                                   element, args->prop);
            }
            else if(flags & DT_FLAT)
            {
                d = P_CheckTexture((char*)((long long*)(src)), true, valueType,
                                    element, args->prop);
            }
            break;
            }
         default:
            Con_Error("ReadValue: DDVT_SHORT no conversion from %i bytes.",
                      size);
         }
        args->shortValues[index] = d;
    }
    else if(valueType == DDVT_FIXED)
    {
        fixed_t     d;
        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    d = SHORT(*((short*)(src)));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                d = ULONG(*((long*)(src)));
            else
                d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_FIXED no conversion from %i bytes.",
                      size);
        }
        args->fixedValues[index] = d;
    }
    else if(valueType == DDVT_ULONG)
    {
        unsigned long   d;

        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    d = SHORT(*((short*)(src)));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                d = ULONG(*((long*)(src)));
            else
                d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_ULONG no conversion from %i bytes.",
                      size);
        }
        args->ulongValues[index] = d;
    }
    else if(valueType == DDVT_UINT)
    {
        unsigned int    d;

        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((short*)(src)));

                    d = NO_INDEX;

                    if(num != ((unsigned short)-1))
                        d = num;
                }
                else
                {
                    if(flags & DT_FRACBITS)
                        d = SHORT(*((short*)(src))) << FRACBITS;
                    else
                        d = SHORT(*((short*)(src)));
                }
            }
            if((flags & DT_MSBCONVERT) && (d & 0x8000))
            {
                d &= ~0x8000;
                d |= 0x80000000;
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                d = ULONG(*((long*)(src)));
            else
                d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_INT no conversion from %i bytes.",
                      size);
        }
        args->uintValues[index] = d;
    }
    else if(valueType == DDVT_INT)
    {
        int         d;
        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((short*)(src)));

                    d = NO_INDEX;

                    if(num != ((unsigned short)-1))
                        d = num;
                }
                else
                {
                    if(flags & DT_FRACBITS)
                        d = SHORT(*((short*)(src))) << FRACBITS;
                    else
                        d = SHORT(*((short*)(src)));
                }
            }
            if((flags & DT_MSBCONVERT) && (d & 0x8000))
            {
                d &= ~0x8000;
                d |= 0x80000000;
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                d = ULONG(*((long*)(src)));
            else
                d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_INT no conversion from %i bytes.",
                      size);
        }
        args->intValues[index] = d;
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t     d;
        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_FRACBITS)
                d = (angle_t) (SHORT(*((short*)(src))) << FRACBITS);
            else
                d = (angle_t) SHORT(*((short*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_ANGLE no conversion from %i bytes.",
                      size);
        }
        args->angleValues[index] = d;
    }
    else
    {
        Con_Error("ReadValue: unknown value type %s.\n", value_Str(valueType));
    }
}

static int ReadProperty(void *context, uint elmIdx,
                        const readprop_t* prop, const byte *src)
{
    damsetargs_t *args = (damsetargs_t*) context;

    // Handle unknown (game specific) properties.
    if(args->prop >= NUM_DAM_PROPERTIES)
    {
        ReadValue(args->valueType, prop->size, src, args, 0, prop->flags, elmIdx);
        return true;
    }

    // These are the exported map data properties that can be
    // assigned to when reading map data.
    switch(args->type)
    {
    case DAM_VERTEX:
        switch(args->prop)
        {
        case DAM_X:
            ReadValue(DMT_VERTEX_POS, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_Y:
            ReadValue(DMT_VERTEX_POS, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        default:
            Con_Error("GetProperty: DAM_VERTEX has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;

    case DAM_LINE:
        switch(args->prop)
        {
        case DAM_VERTEX1:
            // TODO: should be DMT_LINE_V1 but we require special case logic
            ReadValue(DDVT_UINT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_VERTEX2:
            // TODO: should be DMT_LINE_V2 but we require special case logic
            ReadValue(DDVT_UINT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_FLAGS:
            ReadValue(DMT_LINE_FLAGS, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_SIDE0:
            // TODO: should be DMT_LINE_SIDES but we require special case logic
            ReadValue(DDVT_UINT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_SIDE1:
            // TODO: should be DMT_LINE_SIDES but we require special case logic
            ReadValue(DDVT_UINT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        default:
            Con_Error("GetProperty: DAM_LINE has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;

    case DAM_SIDE:
        switch(args->prop)
        {
        case DAM_TOP_TEXTURE_OFFSET_X:
            ReadValue(DMT_SURFACE_OFFX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_TOP_TEXTURE_OFFSET_Y:
            ReadValue(DMT_SURFACE_OFFY, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_MIDDLE_TEXTURE_OFFSET_X:
            ReadValue(DMT_SURFACE_OFFX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_MIDDLE_TEXTURE_OFFSET_Y:
            ReadValue(DMT_SURFACE_OFFY, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_BOTTOM_TEXTURE_OFFSET_X:
            ReadValue(DMT_SURFACE_OFFX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_BOTTOM_TEXTURE_OFFSET_Y:
            ReadValue(DMT_SURFACE_OFFY, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_TOP_TEXTURE:
            ReadValue(DMT_SURFACE_TEXTURE, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_MIDDLE_TEXTURE:
            ReadValue(DMT_SURFACE_TEXTURE, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_BOTTOM_TEXTURE:
            ReadValue(DMT_SURFACE_TEXTURE, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_FRONT_SECTOR:
            // TODO: should be DMT_SIDE_SECTOR but we require special case logic
            ReadValue(DDVT_UINT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        default:
            Con_Error("GetProperty: DAM_SIDE has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;

    case DAM_SECTOR:
        switch(args->prop)
        {
        case DAM_FLOOR_HEIGHT:
            ReadValue(DMT_PLANE_HEIGHT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_CEILING_HEIGHT:
            ReadValue(DMT_PLANE_HEIGHT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_FLOOR_TEXTURE:
            ReadValue(DMT_SURFACE_TEXTURE, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_CEILING_TEXTURE:
            ReadValue(DMT_SURFACE_TEXTURE, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_LIGHT_LEVEL:
            ReadValue(DMT_SECTOR_LIGHTLEVEL, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        default:
            Con_Error("GetProperty: DAM_SECTOR has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;

    case DAM_SEG:
        switch(args->prop)
        {
        case DAM_VERTEX1:
            // TODO: should be DMT_SEG_V but we require special case logic
            ReadValue(DDVT_UINT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_VERTEX2:
            // TODO: should be DMT_SEG_V but we require special case logic
            ReadValue(DDVT_UINT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_ANGLE:
            ReadValue(DMT_SEG_ANGLE, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_LINE:
            // TODO: should be DMT_SEG_LINEDEF but we require special case logic
            ReadValue(DDVT_UINT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_SIDE:
            ReadValue(DMT_SEG_SIDE, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_OFFSET:
            ReadValue(DMT_SEG_OFFSET, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        default:
            Con_Error("GetProperty: DAM_SEG has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;

    case DAM_SUBSECTOR:
        switch(args->prop)
        {
        case DAM_SEG_COUNT:
            ReadValue(DMT_SUBSECTOR_SEGCOUNT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_SEG_FIRST:
            // TODO: should be DMT_SUBSECTOR_FIRSTSEG but we require special case logic.
            ReadValue(DDVT_UINT, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        default:
            Con_Error("GetProperty: DAM_SUBSECTOR has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;

    case DAM_NODE:
        switch(args->prop)
        {
        case DAM_X:
            ReadValue(DMT_NODE_X, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_Y:
            ReadValue(DMT_NODE_Y, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_DX:
            ReadValue(DMT_NODE_DX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_DY:
            ReadValue(DMT_NODE_DY, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_BBOX_RIGHT_TOP_Y:
            ReadValue(DMT_NODE_BBOX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_BBOX_RIGHT_LOW_Y:
            ReadValue(DMT_NODE_BBOX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_BBOX_RIGHT_LOW_X:
            ReadValue(DMT_NODE_BBOX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_BBOX_RIGHT_TOP_X:
            ReadValue(DMT_NODE_BBOX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_BBOX_LEFT_TOP_Y:
            ReadValue(DMT_NODE_BBOX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_BBOX_LEFT_LOW_Y:
            ReadValue(DMT_NODE_BBOX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_BBOX_LEFT_LOW_X:
            ReadValue(DMT_NODE_BBOX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_BBOX_LEFT_TOP_X:
            ReadValue(DMT_NODE_BBOX, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_CHILD_RIGHT:
            ReadValue(DMT_NODE_CHILDREN, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        case DAM_CHILD_LEFT:
            ReadValue(DMT_NODE_CHILDREN, prop->size, src, args, 0, prop->flags, elmIdx);
            break;

        default:
            Con_Error("GetProperty: DAM_NODE has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;

    default:
        Con_Error("GetProperty: Type cannot be assigned to from a map format.\n");
    }

    return true; // Continue iteration
}

static int setCustomProperty(void *ptr, void *context,
                             gamemap_t *map, uint elmIdx)
{
    damsetargs_t *args = (damsetargs_t*) context;
    void        *dest = NULL;

    if(!gx.HandleMapDataProperty)
        return false;

    switch(args->valueType)
    {
    case DDVT_BYTE:
        dest = &args->byteValues[0];
        break;
    case DDVT_SHORT:
        dest = &args->shortValues[0];
        break;
    case DDVT_FIXED:
        dest = &args->fixedValues[0];
        break;
    case DDVT_INT:
        dest = &args->intValues[0];
        break;
    case DDVT_FLOAT:
        dest = &args->floatValues[0];
        break;
    default:
        Con_Error("SetProperty: Unsupported data type id %s.\n",
                  value_Str(args->valueType));
    };
    gx.HandleMapDataProperty(elmIdx, args->type, args->prop, args->valueType, dest);

    return true;
}

static int SetProperty(void *ptr, void *context,
                       gamemap_t *map, uint elmIdx)
{
    damsetargs_t *args = (damsetargs_t*) context;

    // Handle unknown (game specific) properties.
    if(args->prop >= NUM_DAM_PROPERTIES)
    {
        setCustomProperty(ptr, context, map, elmIdx);
        return true;
    }

    // These are the exported map data properties that can be
    // assigned to when reading map data.
    switch(args->type)
    {
    case DAM_VERTEX:
        {
        vertex_t* p = ptr;

        switch(args->prop)
        {
        case DAM_X:
            SetValue(DMT_VERTEX_POS, &p->pos[VX], args, 0, map);
            break;

        case DAM_Y:
            SetValue(DMT_VERTEX_POS, &p->pos[VY], args, 0, map);
            break;

        default:
            Con_Error("SetProperty: DAM_VERTEX has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_LINE:
        {
        line_t* p = ptr;

        switch(args->prop)
        {
        case DAM_VERTEX1:
            SetValue(DMT_LINE_V, &p->L_v1, args, 0, map);
            break;

        case DAM_VERTEX2:
            SetValue(DMT_LINE_V, &p->L_v2, args, 0, map);
            break;

        case DAM_FLAGS:
            SetValue(DMT_LINE_FLAGS, &p->flags, args, 0, map);
            break;

        case DAM_SIDE0:
            SetValue(DMT_LINE_SIDES, &p->L_frontside, args, 0, map);
            break;

        case DAM_SIDE1:
            SetValue(DMT_LINE_SIDES, &p->L_backside, args, 0, map);
            break;

        default:
            Con_Error("SetProperty: DAM_LINE has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_SIDE:
        {
        side_t* p = ptr;

        switch(args->prop)
        {
        case DAM_TOP_TEXTURE_OFFSET_X:
            SetValue(DMT_SURFACE_OFFX, &p->SW_topoffx, args, 0, map);
            break;

        case DAM_TOP_TEXTURE_OFFSET_Y:
            SetValue(DMT_SURFACE_OFFY, &p->SW_topoffy, args, 0, map);
            break;

        case DAM_MIDDLE_TEXTURE_OFFSET_X:
            SetValue(DMT_SURFACE_OFFX, &p->SW_middleoffx, args, 0, map);
            break;

        case DAM_MIDDLE_TEXTURE_OFFSET_Y:
            SetValue(DMT_SURFACE_OFFY, &p->SW_middleoffy, args, 0, map);
            break;

        case DAM_BOTTOM_TEXTURE_OFFSET_X:
            SetValue(DMT_SURFACE_OFFX, &p->SW_bottomoffx, args, 0, map);
            break;

        case DAM_BOTTOM_TEXTURE_OFFSET_Y:
            SetValue(DMT_SURFACE_OFFY, &p->SW_bottomoffy, args, 0, map);
            break;

        case DAM_TOP_TEXTURE:
            SetValue(DMT_SURFACE_TEXTURE, &p->SW_toppic, args, 0, map);
            break;

        case DAM_MIDDLE_TEXTURE:
            SetValue(DMT_SURFACE_TEXTURE, &p->SW_middlepic, args, 0, map);
            break;

        case DAM_BOTTOM_TEXTURE:
            SetValue(DMT_SURFACE_TEXTURE, &p->SW_bottompic, args, 0, map);
            break;

        case DAM_FRONT_SECTOR:
            SetValue(DMT_SIDE_SECTOR, &p->sector, args, 0, map);
            break;

        default:
            Con_Error("SetProperty: DAM_SIDE has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_SECTOR:
        {
        sector_t* p = ptr;

        switch(args->prop)
        {
        case DAM_FLOOR_HEIGHT:
            SetValue(DMT_PLANE_HEIGHT, &p->SP_floorheight, args, 0, map);
            break;

        case DAM_CEILING_HEIGHT:
            SetValue(DMT_PLANE_HEIGHT, &p->SP_ceilheight, args, 0, map);
            break;

        case DAM_FLOOR_TEXTURE:
            SetValue(DMT_SURFACE_TEXTURE, &p->SP_floorpic, args, 0, map);
            break;

        case DAM_CEILING_TEXTURE:
            SetValue(DMT_SURFACE_TEXTURE, &p->SP_ceilpic, args, 0, map);
            break;

        case DAM_LIGHT_LEVEL:
            SetValue(DMT_SECTOR_LIGHTLEVEL, &p->lightlevel, args, 0, map);
            break;

        default:
            Con_Error("SetProperty: DAM_SECTOR has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_SEG:
        {
        seg_t* p = ptr;

        switch(args->prop)
        {
        case DAM_VERTEX1:
            SetValue(DMT_SEG_V, &p->SG_v1, args, 0, map);
            break;

        case DAM_VERTEX2:
            SetValue(DMT_SEG_V, &p->SG_v2, args, 0, map);
            break;

        case DAM_ANGLE:
            SetValue(DMT_SEG_ANGLE, &p->angle, args, 0, map);
            break;

        case DAM_LINE:
            SetValue(DMT_SEG_LINEDEF, &p->linedef, args, 0, map);
            break;

        case DAM_SIDE:
            SetValue(DMT_SEG_SIDE, &p->side, args, 0, map);
            break;

        case DAM_OFFSET:
            SetValue(DMT_SEG_OFFSET, &p->offset, args, 0, map);
            break;

        default:
            Con_Error("SetProperty: DAM_SEG has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_SUBSECTOR:
        {
        subsector_t* p = ptr;

        switch(args->prop)
        {
        case DAM_SEG_COUNT:
            SetValue(DMT_SUBSECTOR_SEGCOUNT, &p->segcount, args, 0, map);
            break;

        case DAM_SEG_FIRST:
            SetValue(DMT_SUBSECTOR_FIRSTSEG, &p->firstseg, args, 0, map);
            break;

        default:
            Con_Error("SetProperty: DAM_SUBSECTOR has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    case DAM_NODE:
        {
        node_t* p = ptr;

        switch(args->prop)
        {
        case DAM_X:
            SetValue(DMT_NODE_X, &p->x, args, 0, map);
            break;

        case DAM_Y:
            SetValue(DMT_NODE_Y, &p->y, args, 0, map);
            break;

        case DAM_DX:
            SetValue(DMT_NODE_DX, &p->dx, args, 0, map);
            break;

        case DAM_DY:
            SetValue(DMT_NODE_DY, &p->dy, args, 0, map);
            break;

        case DAM_BBOX_RIGHT_TOP_Y:
            SetValue(DMT_NODE_BBOX, &p->bbox[0][0], args, 0, map);
            break;

        case DAM_BBOX_RIGHT_LOW_Y:
            SetValue(DMT_NODE_BBOX, &p->bbox[0][1], args, 0, map);
            break;

        case DAM_BBOX_RIGHT_LOW_X:
            SetValue(DMT_NODE_BBOX, &p->bbox[0][2], args, 0, map);
            break;

        case DAM_BBOX_RIGHT_TOP_X:
            SetValue(DMT_NODE_BBOX, &p->bbox[0][3], args, 0, map);
            break;

        case DAM_BBOX_LEFT_TOP_Y:
            SetValue(DMT_NODE_BBOX, &p->bbox[1][0], args, 0, map);
            break;

        case DAM_BBOX_LEFT_LOW_Y:
            SetValue(DMT_NODE_BBOX, &p->bbox[1][1], args, 0, map);
            break;

        case DAM_BBOX_LEFT_LOW_X:
            SetValue(DMT_NODE_BBOX, &p->bbox[1][2], args, 0, map);
            break;

        case DAM_BBOX_LEFT_TOP_X:
            SetValue(DMT_NODE_BBOX, &p->bbox[1][3], args, 0, map);
            break;

        case DAM_CHILD_RIGHT:
            SetValue(DMT_NODE_CHILDREN, &p->children[0], args, 0, map);
            break;

        case DAM_CHILD_LEFT:
            SetValue(DMT_NODE_CHILDREN, &p->children[1], args, 0, map);
            break;

        default:
            Con_Error("SetProperty: DAM_NODE has no property %s.\n",
                      DAM_Str(args->prop));
        }
        break;
        }
    default:
        Con_Error("SetProperty: Type cannot be assigned to from a map format.\n");
    }

    return true; // Continue iteration
}

static int ReadMapProperty(void *ptr, int dataType, gamemap_t *map,
                           uint elmIdx, const readprop_t* prop, const byte *src)
{
    damsetargs_t args;
    angle_t     tmpangle;
    boolean     tmpboolean;
    byte        tmpbyte;
    fixed_t     tmpfixed;
    float       tmpfloat;
    short       tmpshort;
    int         tmpint;
    uint        tmpuint;
    unsigned long tmpulong;
    void       *tmpptr;

    initArgs(&args, dataType, prop->id);
    args.valueType = prop->valueType;
    args.angleValues = &tmpangle;
    args.booleanValues = &tmpboolean;
    args.byteValues = &tmpbyte;
    args.fixedValues = &tmpfixed;
    args.floatValues = &tmpfloat;
    args.shortValues = &tmpshort;
    args.intValues = &tmpint;
    args.uintValues = &tmpuint;
    args.ulongValues = &tmpulong;
    args.ptrValues = &tmpptr;

    ReadProperty(&args, elmIdx, prop, src);
    SetProperty(ptr, &args, map, elmIdx);

    return true; // Continue iteration.
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
                        int (*callback)(void *ptr, int dataType,
                                 gamemap_t* map, uint elmIdx, const readprop_t* prop,
                                 const byte *buffer))
{
#define NUMBLOCKS 8
#define BLOCK ptr = dataType == DAM_THING? &idx : DAM_IndexToPtr(map, dataType, idx);  \
        for(prop = &args->props[k = 0]; k < args->numProps; prop = &args->props[++k]) \
        { \
            if(!callback(ptr, dataType, map, idx, prop, buffer + prop->offset)) \
                return false; \
        } \
        buffer += args->elmsize; \
        ++idx;

    uint        idx;
    uint        i = 0, k;
    damlumpreadargs_t  *args = (damlumpreadargs_t*) context;
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
                if(!callback(ptr, dataType, map, idx, prop, buffer + prop->offset))
                    return false;
            }
        }
    }

    return true;
}
