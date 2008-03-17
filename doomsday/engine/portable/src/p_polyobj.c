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
 * r_polyobj.c: World polyobjects.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_play.h"

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
 * Update the polyobj, property is selected by DMU_* name.
 */
boolean Polyobj_SetProperty(polyobj_t *po, const setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_START_SPOT_X:
        DMU_SetValue(DDVT_FLOAT, &po->startSpot.pos[VX], args, 0);
        break;
    case DMU_START_SPOT_Y:
        DMU_SetValue(DDVT_FLOAT, &po->startSpot.pos[VY], args, 0);
        break;
    case DMU_START_SPOT_XY:
        DMU_SetValue(DDVT_FLOAT, &po->startSpot.pos[VX], args, 0);
        DMU_SetValue(DDVT_FLOAT, &po->startSpot.pos[VY], args, 1);
        break;
    case DMU_DESTINATION_X:
        DMU_SetValue(DDVT_FLOAT, &po->dest.pos[VX], args, 0);
        break;
    case DMU_DESTINATION_Y:
        DMU_SetValue(DDVT_FLOAT, &po->dest.pos[VY], args, 0);
        break;
    case DMU_DESTINATION_XY:
        DMU_SetValue(DDVT_FLOAT, &po->dest.pos[VX], args, 0);
        DMU_SetValue(DDVT_FLOAT, &po->dest.pos[VY], args, 1);
        break;
    case DMU_ANGLE:
        DMU_SetValue(DDVT_ANGLE, &po->angle, args, 0);
        break;
    case DMU_DESTINATION_ANGLE:
        DMU_SetValue(DDVT_ANGLE, &po->destAngle, args, 0);
        break;
    case DMU_SPEED:
        DMU_SetValue(DDVT_FLOAT, &po->speed, args, 0);
        break;
    case DMU_ANGLE_SPEED:
        DMU_SetValue(DDVT_ANGLE, &po->angleSpeed, args, 0);
        break;
    case DMU_TAG:
        DMU_SetValue(DDVT_INT, &po->tag, args, 0);
        break;
    case DMU_CRUSH:
        DMU_SetValue(DDVT_BOOL, &po->crush, args, 0);
        break;
    case DMU_SEQUENCE_TYPE:
        DMU_SetValue(DDVT_INT, &po->seqType, args, 0);
        break;
    case DMU_SEG_COUNT:
        DMU_SetValue(DDVT_INT, &po->numSegs, args, 0);
        break;
    case DMU_SPECIAL_DATA:
        DMU_SetValue(DDVT_PTR, &po->specialData, args, 0);
        break;
    default:
        Con_Error("Polyobj_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
        break;
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a polyobj property, selected by DMU_* name.
 */
boolean Polyobj_GetProperty(const polyobj_t *po, setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_START_SPOT:
    {
        const void* spot = &po->startSpot;
        DMU_GetValue(DDVT_PTR, &spot, args, 0);
        break;
    }
    case DMU_START_SPOT_X:
        DMU_GetValue(DDVT_FLOAT, &po->startSpot.pos[VX], args, 0);
        break;
    case DMU_START_SPOT_Y:
        DMU_GetValue(DDVT_FLOAT, &po->startSpot.pos[VY], args, 0);
        break;
    case DMU_START_SPOT_XY:
        DMU_GetValue(DDVT_FLOAT, &po->startSpot.pos[VX], args, 0);
        DMU_GetValue(DDVT_FLOAT, &po->startSpot.pos[VY], args, 1);
        break;
    case DMU_ANGLE:
        DMU_GetValue(DDVT_ANGLE, &po->angle, args, 0);
        break;
    case DMU_DESTINATION_ANGLE:
        DMU_GetValue(DDVT_ANGLE, &po->destAngle, args, 0);
        break;
    case DMU_ANGLE_SPEED:
        DMU_GetValue(DDVT_ANGLE, &po->angleSpeed, args, 0);
        break;
    case DMU_TAG:
        DMU_GetValue(DDVT_INT, &po->tag, args, 0);
        break;
    case DMU_SEG_COUNT:
        DMU_GetValue(DDVT_INT, &po->numSegs, args, 0);
        break;
    case DMU_CRUSH:
        DMU_GetValue(DDVT_BOOL, &po->crush, args, 0);
        break;
    case DMU_SEQUENCE_TYPE:
        DMU_GetValue(DDVT_INT, &po->seqType, args, 0);
        break;
    case DMU_SPECIAL_DATA:
        DMU_GetValue(DDVT_PTR, &po->specialData, args, 0);
        break;
    default:
        Con_Error("Polyobj_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
