/* $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * Handle jDoom specific map data properties.
 */

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "r_defs.h"
#include "p_setup.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Doomsday will call this while loading in map data when a value is read
 * that is not part of the internal data structure for the particular element.
 * This is where game specific data is added to game-side map data structures
 * (eg sector->tag, line->args etc).
 *
 * Returns true unless there is a critical problem with the data supplied.
 *
 * @param id:       index of the current element being read.
 * @param dtype:    lump type class id this value is for.
 * @param prop:     propertyid of the game-specific variable (as declared via DED).
 * @param type:     data type id of the value pointed to by *data.
 * @param *data:    ptr to the data value (has already been expanded, size
 *                  converted and endian converted where necessary).
 */
int P_HandleMapDataProperty(int id, int dtype, int prop, int type, void *data)
{
    switch(prop)
    {
    case DAM_SECTOR_SPECIAL:
        xsectors[id].special = *(short *)data;
        break;
    case DAM_SECTOR_TAG:
        xsectors[id].tag = *(short *)data;
        break;
    case DAM_LINE_SPECIAL:
        xlines[id].special = *(short *)data;
        break;
    case DAM_LINE_TAG:
        xlines[id].tag = *(short *)data;
        break;
    case DAM_THING_X:
        things[id].x = *(short *)data;
        break;
    case DAM_THING_Y:
        things[id].y = *(short *)data;
        break;
    case DAM_THING_HEIGHT:
        things[id].height = *(short *)data;
        break;
    case DAM_THING_ANGLE:
        things[id].angle = *(short *)data;
        break;
    case DAM_THING_TYPE:
        things[id].type = *(short *)data;
        break;
    case DAM_THING_OPTIONS:
        things[id].options = *(short *)data;
        break;
    default:
        Con_Error("P_HandleMapDataProperty: Unknown property id %i.\n",prop);
    }

    return 1;
}

/*
 * Doomsday will call this when loading the map data if it encounters a
 * value that it doesn't understand for a property IT handles.
 *
 * Doomsday thinks we might know what to do with it...
 * If we don't know what to do we'll return -1.
 *
 * @param id:       index of the current element being read.
 * @param dtype:    lump type class id this value is for.
 * @param prop:     propertyid of the map structure.
 * @param type:     data type id of the value pointed to by *data.
 * @param *data:    ptr to the data value (has already been expanded, size
 *                  converted and endian converted where necessary).
 */
int P_HandleMapDataPropertyValue(int id, int dtype, int prop,
                                 int type, void *data)
{
    switch(dtype)
    {
    case DAM_SIDE:
        switch(prop)
        {
        case DAM_TOP_TEXTURE:
        case DAM_MIDDLE_TEXTURE:
        case DAM_BOTTOM_TEXTURE:
            // It could be a BOOM overloaded texture name?
            // In this context Doomsday expects either -1 (a bad texture name)
            // Or the id of a wall texture it should set to this section.

            // TODO: Add code to determine what to do.
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }

    return -1; // We ain't got a clue what to do with it...
}

/*
 * These status reports inform us of what Doomsday is doing to a particular
 * map data object (at any time) that we might want to react to.
 *
 * For example, during post processing of map data during level load - if
 * Doomsday encounters a sector with 0 lines it will send us a "benign sector"
 * report. In jDoom we respond to this by making sure we remove the sector's
 * special tag to prevent wayward line specials from misbehaving.
 *
 * If we arn't interested in the report - we should simply return true and
 * take no further action.
 *
 * @param code      ID code of the status report (enum in dd_share.h)
 * @param id        Map data object id.
 * @param type      Map data object type eg DMU_SECTOR.
 * @param data      Any relevant data for this report (currently unused).
 */
int P_HandleMapObjectStatusReport(int code, int id, int dtype, void *data)
{
    switch(code)
    {
    case DMUSC_BENIGNSECTOR:
        // A benign sector is one which has zero lines.
        // Zero it's tag to prevent it from being selected while searching for
        // sectors to act on (eg XG and the "built-in" line specials).
        xsectors[id].tag = 0;
        break;

    default:
        break;
    }

    return 1;
}
