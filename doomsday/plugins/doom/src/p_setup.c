/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2002-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * p_setup.c: Handle jDoom specific map data properties.
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "am_map.h"

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
 * Called during pre-init.
 * Register the map object data types we want to Doomsday to make public via
 * it's MPE interface.
 */
void P_RegisterMapObjs(void)
{
    P_RegisterMapObj(MO_THING, "Thing");
    P_RegisterMapObjProperty(MO_THING, MO_X, "X", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_THING, MO_Y, "Y", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_THING, MO_Z, "Z", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_THING, MO_ANGLE, "Angle", DDVT_ANGLE);
    P_RegisterMapObjProperty(MO_THING, MO_DOOMEDNUM, "DoomEdNum", DDVT_INT);
    P_RegisterMapObjProperty(MO_THING, MO_SKILLMODES, "SkillModes", DDVT_INT);
    P_RegisterMapObjProperty(MO_THING, MO_FLAGS, "Flags", DDVT_INT);

    P_RegisterMapObj(MO_XLINEDEF, "XLinedef");
    P_RegisterMapObjProperty(MO_XLINEDEF, MO_TAG, "Tag", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_XLINEDEF, MO_TYPE, "Type", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_XLINEDEF, MO_FLAGS, "Flags", DDVT_SHORT);

    P_RegisterMapObj(MO_XSECTOR, "XSector");
    P_RegisterMapObjProperty(MO_XSECTOR, MO_TAG, "Tag", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_XSECTOR, MO_TYPE, "Type", DDVT_SHORT);
}

/**
 * \todo This is actually unnecessary. If we say that in the case of BOOM
 * overloaded texture names, rather than setting the surface material as
 * normal, the map converter should instead write to a property in XLinedef
 * which the game can then look up later.
 *
 * Doomsday will call this when loading the map data if it encounters a
 * value that it doesn't understand for a property IT handles.
 *
 * Doomsday thinks we might know what to do with it...
 * If we don't know what to do we'll return -1.
 *
 * @param id            Index of the current element being read.
 * @param dtype         DMU type identifier.
 * @param prop          DMU property identifier.
 * @param type          Data type id of the value pointed to by *data.
 * @param *data         Ptr to the data value (has already been expanded,
 *                      size converted and endian converted where necessary).
 */
int P_HandleMapDataPropertyValue(uint id, int dtype, int prop,
                                 valuetype_t type, void *data)
{
    switch(dtype)
    {
    case DMU_SURFACE:
        switch(prop)
        {
        case DMU_MATERIAL:
            /**
             * It could be a BOOM overloaded texture name?
             * In this context Doomsday expects either -1 (a bad texture name)
             * Or the id of a wall texture it should set to this section.
             * \todo Add code to determine what to do.
             */
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

/**
 * These status reports inform us of what Doomsday is doing to a particular
 * map data object (at any time) that we might want to react to.
 *
 * If we arn't interested in the report - we should simply return true and
 * take no further action.
 *
 * @param code          ID code of the status report (enum in dd_share.h)
 * @param id            Map data object id.
 * @param type          Map data object type eg DMU_SECTOR.
 * @param data          Any relevant data for this report (currently unused).
 */
int P_HandleMapObjectStatusReport(int code, uint id, int dtype, void *data)
{
    switch(code)
    {
    case DMUSC_LINE_FIRSTRENDERED:
        /**
         * Called the first time the given line is rendered.
         * *data is a pointer to int, giving the player id which has seen it.
         * We'll utilize this to mark it as being visible in the automap.
         */
        P_SetLineAutomapVisibility(*(int*)data, id, true);
        break;

    default:
        break;
    }

    return 1;
}
