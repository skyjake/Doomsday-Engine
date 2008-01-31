/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * cl_oldworld.c: Obsolete Clientside World Management
 *
 * This file contains obsolete delta routines. They are preserved so that
 * backwards compatibility is retained with older versions of the network
 * protocol.
 *
 * These routines should be considered FROZEN. Do not change them.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"

#include "r_util.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Reads a sector delta from the message buffer and applies it to
 * the world. Returns false only if the end marker is found.
 *
 * THIS FUNCTION IS NOW OBSOLETE (only used with PSV_FRAME packets)
 */
int Cl_ReadSectorDelta(void)
{
    short       num = Msg_ReadPackedShort();
    sector_t   *sec;
    int         df;

    // Sector number first (0 terminates).
    if(!num)
        return false;

    sec = SECTOR_PTR(--num);

    // Flags.
    df = Msg_ReadPackedShort();

    if(df & SDF_FLOORPIC)
        sec->SP_floormaterial =
            R_GetMaterial(Msg_ReadPackedShort(), MAT_FLAT);
    if(df & SDF_CEILINGPIC)
        sec->SP_ceilmaterial =
            R_GetMaterial(Msg_ReadPackedShort(), MAT_FLAT);
    if(df & SDF_LIGHT)
        sec->lightLevel = Msg_ReadByte() / 255.0f;
    if(df & SDF_FLOOR_TARGET)
        sec->planes[PLN_FLOOR]->target = FIX2FLT(Msg_ReadShort() << 16);
    if(df & SDF_FLOOR_SPEED)
    {
        sec->planes[PLN_FLOOR]->speed =
            FIX2FLT((fixed_t) (Msg_ReadByte() << (df & SDF_FLOOR_SPEED_44 ? 12 : 15)));
    }
    if(df & SDF_FLOOR_TEXMOVE)
    {   // Old clients might include these.
        fixed_t movex = Msg_ReadShort() << 8;
        fixed_t movey = Msg_ReadShort() << 8;
    }
    if(df & SDF_CEILING_TARGET)
        sec->planes[PLN_CEILING]->target = FIX2FLT(Msg_ReadShort() << 16);
    if(df & SDF_CEILING_SPEED)
    {
        sec->planes[PLN_CEILING]->speed =
            FIX2FLT((fixed_t) (Msg_ReadByte() << (df & SDF_CEILING_SPEED_44 ? 12 : 15)));
    }
    if(df & SDF_CEILING_TEXMOVE)
    {   // Old clients might include these.
        fixed_t movex = Msg_ReadShort() << 8;
        fixed_t movey = Msg_ReadShort() << 8;
    }
    if(df & SDF_COLOR_RED)
        sec->rgb[0] = Msg_ReadByte() / 255.f;
    if(df & SDF_COLOR_GREEN)
        sec->rgb[1] = Msg_ReadByte() / 255.f;
    if(df & SDF_COLOR_BLUE)
        sec->rgb[2] = Msg_ReadByte() / 255.f;

    if(df & SDF_FLOOR_COLOR_RED)
        sec->SP_floorrgb[0] = Msg_ReadByte() / 255.f;
    if(df & SDF_FLOOR_COLOR_GREEN)
        sec->SP_floorrgb[1] = Msg_ReadByte() / 255.f;
    if(df & SDF_FLOOR_COLOR_BLUE)
        sec->SP_floorrgb[2] = Msg_ReadByte() / 255.f;

    if(df & SDF_CEIL_COLOR_RED)
        sec->SP_ceilrgb[0] = Msg_ReadByte() / 255.f;
    if(df & SDF_CEIL_COLOR_GREEN)
        sec->SP_ceilrgb[1] = Msg_ReadByte() / 255.f;
    if(df & SDF_CEIL_COLOR_BLUE)
        sec->SP_ceilrgb[2] = Msg_ReadByte() / 255.f;

    if(df & SDF_FLOOR_GLOW_RED)
        sec->SP_floorglowrgb[0] = Msg_ReadByte() / 255.f;
    if(df & SDF_FLOOR_GLOW_GREEN)
        sec->SP_floorglowrgb[1] = Msg_ReadByte() / 255.f;
    if(df & SDF_FLOOR_GLOW_BLUE)
        sec->SP_floorglowrgb[2] = Msg_ReadByte() / 255.f;

    if(df & SDF_CEIL_GLOW_RED)
        sec->SP_ceilglowrgb[0] = Msg_ReadByte() / 255.f;
    if(df & SDF_CEIL_GLOW_GREEN)
        sec->SP_ceilglowrgb[1] = Msg_ReadByte() / 255.f;
    if(df & SDF_CEIL_GLOW_BLUE)
        sec->SP_ceilglowrgb[2] = Msg_ReadByte() / 255.f;

    if(df & SDF_FLOOR_GLOW)
        sec->SP_floorglow = (float) (Msg_ReadShort() / DDMAXSHORT);
    if(df & SDF_CEIL_GLOW)
        sec->SP_ceilglow = (float) (Msg_ReadShort() / DDMAXSHORT);

    // Do we need to start any moving planes?
    if(df & (SDF_FLOOR_TARGET | SDF_FLOOR_SPEED))
    {
        Cl_AddMover(num, MVT_FLOOR, sec->planes[PLN_FLOOR]->target,
                    sec->planes[PLN_FLOOR]->speed);
    }
    if(df & (SDF_CEILING_TARGET | SDF_CEILING_SPEED))
    {
        Cl_AddMover(num, MVT_CEILING, sec->planes[PLN_CEILING]->target,
                    sec->planes[PLN_CEILING]->speed);
    }

    // Continue reading.
    return true;
}

/**
 * Reads a side delta from the message buffer and applies it to
 * the world. Returns false only if the end marker is found.
 *
 * THIS FUNCTION IS NOW OBSOLETE (only used with PSV_FRAME packets)
 */
int Cl_ReadSideDelta(void)
{
    short       num = Msg_ReadPackedShort(); // \fixme we support > 32768 sidedefs!
    side_t     *sid;
    int         df;

    // Side number first (0 terminates).
    if(!num)
        return false;

    sid = SIDE_PTR(--num);

    // Flags.
    df = Msg_ReadByte();

    if(df & SIDF_TOPTEX)
        sid->SW_topmaterial =
            R_GetMaterial(Msg_ReadPackedShort(), MAT_TEXTURE);
    if(df & SIDF_MIDTEX)
        sid->SW_middlematerial =
            R_GetMaterial(Msg_ReadPackedShort(), MAT_TEXTURE);
    if(df & SIDF_BOTTOMTEX)
        sid->SW_bottommaterial =
            R_GetMaterial(Msg_ReadPackedShort(), MAT_TEXTURE);

    if(df & SIDF_LINE_FLAGS)
    {
        byte    updatedFlags = Msg_ReadByte();
        line_t *line = R_GetLineForSide(num);

        if(line)
        {
            // The delta includes the lowest byte.
            line->flags &= ~0xff;
            line->flags |= updatedFlags;
#if _DEBUG
Con_Printf("lineflag %i: %02x\n", GET_LINE_IDX(line),
           updatedFlags);
#endif
        }
    }

    if(df & SIDF_TOP_COLOR_RED)
        sid->SW_toprgba[0] = Msg_ReadByte() / 255.f;
    if(df & SIDF_TOP_COLOR_GREEN)
        sid->SW_toprgba[1] = Msg_ReadByte() / 255.f;
    if(df & SIDF_TOP_COLOR_BLUE)
        sid->SW_toprgba[2] = Msg_ReadByte() / 255.f;

    if(df & SIDF_MID_COLOR_RED)
        sid->SW_middlergba[0] = Msg_ReadByte() / 255.f;
    if(df & SIDF_MID_COLOR_GREEN)
        sid->SW_middlergba[1] = Msg_ReadByte() / 255.f;
    if(df & SIDF_MID_COLOR_BLUE)
        sid->SW_middlergba[2] = Msg_ReadByte() / 255.f;
    if(df & SIDF_MID_COLOR_ALPHA)
        sid->SW_middlergba[3] = Msg_ReadByte() / 255.f;

    if(df & SIDF_BOTTOM_COLOR_RED)
        sid->SW_bottomrgba[0] = Msg_ReadByte() / 255.f;
    if(df & SIDF_BOTTOM_COLOR_GREEN)
        sid->SW_bottomrgba[1] = Msg_ReadByte() / 255.f;
    if(df & SIDF_BOTTOM_COLOR_BLUE)
        sid->SW_bottomrgba[2] = Msg_ReadByte() / 255.f;

    if(df & SIDF_MID_BLENDMODE)
        sid->SW_middleblendmode = Msg_ReadShort() << 16;

    if(df & SIDF_FLAGS)
    {
        byte    updatedFlags = Msg_ReadByte();

        // The delta includes the lowest byte.
        sid->flags &= ~0xff;
        sid->flags |= updatedFlags;
    }

    // Continue reading.
    return true;
}

/**
 * Reads a poly delta from the message buffer and applies it to
 * the world. Returns false only if the end marker is found.
 *
 * THIS FUNCTION IS NOW OBSOLETE (only used with PSV_FRAME packets)
 */
int Cl_ReadPolyDelta(void)
{
    int         df;
    short       num = Msg_ReadPackedShort();
    polyobj_t  *po;

    // Check the number. A zero terminates.
    if(!num)
        return false;

    po = polyobjs[--num];

    // Flags.
    df = Msg_ReadPackedShort();

    if(df & PODF_DEST_X)
        po->dest.pos[VX] = FIX2FLT((Msg_ReadShort() << 16) + ((char) Msg_ReadByte() << 8));
    if(df & PODF_DEST_Y)
        po->dest.pos[VY] = FIX2FLT((Msg_ReadShort() << 16) + ((char) Msg_ReadByte() << 8));
    if(df & PODF_SPEED)
        po->speed = FIX2FLT(Msg_ReadShort() << 8);
    if(df & PODF_DEST_ANGLE)
        po->destAngle = Msg_ReadShort() << 16;
    if(df & PODF_ANGSPEED)
        po->angleSpeed = Msg_ReadShort() << 16;

    /*CON_Printf("num=%i:\n", num);
       CON_Printf("  destang=%x angsp=%x\n", po->destAngle, po->angleSpeed); */

    if(df & PODF_PERPETUAL_ROTATE)
        po->destAngle = -1;

    // Update the polyobj's mover thinkers.
    Cl_SetPolyMover(num, df & (PODF_DEST_X | PODF_DEST_Y | PODF_SPEED),
                    df & (PODF_DEST_ANGLE | PODF_ANGSPEED |
                          PODF_PERPETUAL_ROTATE));

    // Continue reading.
    return true;
}
