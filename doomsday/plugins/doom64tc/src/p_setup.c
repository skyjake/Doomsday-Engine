/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2002-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * p_setup.c: Handle Doom64TC specific map data properties.
 */

// HEADER FILES ------------------------------------------------------------

#include "doom64tc.h"

#include "am_map.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

enum {
    CMP_LINE_TAG,
    CMP_LINE_SPECIAL,
    CMP_LINE_FLAGS,
    CMP_SECTOR_TAG,
    CMP_SECTOR_SPECIAL,
    CMP_THING_POS_X,
    CMP_THING_POS_Y,
    CMP_THING_ANGLE,
    CMP_THING_TYPE,
    CMP_THING_FLAGS,
    NUM_CUSTOM_MAP_PROPERTIES
} mappropid_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// We require direct access to the extra data arrays because DMU is not
// online during map setup, thus we can't convert indices to hardened ptrs.
extern xsector_t *xsectors;
extern xline_t   *xlines;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int customPropIds[NUM_CUSTOM_MAP_PROPERTIES];

// CODE --------------------------------------------------------------------

static int DDPropIDToID(int ddid)
{
    int         i;

    for(i = 0; i < NUM_CUSTOM_MAP_PROPERTIES; ++i)
        if(customPropIds[i] == ddid) // a match!
            return i;

    return -1;
}

/**
 * Called during pre-init.
 * Registers the custom properties we need Doomsday to read from a map format.
 */
void P_RegisterCustomMapProperties(void)
{
    struct prop_s {
        int         type;       // DAM object type e.g. DAM_LINE.
        valuetype_t datatype;   // Internal type used to store the value.
        char       *name;       // Name by which this property is referred.
        int         ourid;      // The id we've assigned to it.
    } properties[] =
    {
        // Line properties:
        {DAM_LINE,      DDVT_SHORT,     "Tag",          CMP_LINE_TAG},
        {DAM_LINE,      DDVT_SHORT,     "Special",      CMP_LINE_SPECIAL},
        {DAM_LINE,      DDVT_SHORT,     "Flags",        CMP_LINE_FLAGS},
        // Sector properties:
        {DAM_SECTOR,    DDVT_SHORT,     "Tag",          CMP_SECTOR_TAG},
        {DAM_SECTOR,    DDVT_SHORT,     "Special",      CMP_SECTOR_SPECIAL},
        // Thing properties:
        {DAM_THING,     DDVT_SHORT,     "X",            CMP_THING_POS_X},
        {DAM_THING,     DDVT_SHORT,     "Y",            CMP_THING_POS_Y},
        {DAM_THING,     DDVT_SHORT,     "Angle",        CMP_THING_ANGLE},
        {DAM_THING,     DDVT_SHORT,     "Type",         CMP_THING_TYPE},
        {DAM_THING,     DDVT_SHORT,     "Options",      CMP_THING_FLAGS},
        {0,             0,              NULL,           0} // Terminate.
    };
    uint        i, idx;

    i = 0;
    while(properties[i].name)
    {
        // Will return the id by which Doomsday will refer to this property.
        idx = P_RegisterCustomMapProperty(properties[i].type, properties[i].datatype,
                                          properties[i].name);
        // Store the id returned to us by Doosmday into the conversion LUT,
        // using our id as the index.
        customPropIds[properties[i].ourid] = idx;
        i++;
    }
}

/**
 * Doomsday will call this while loading in map data when a value is read
 * that is not part of the internal data structure for the particular element.
 * This is where game specific data is added to game-side map data structures
 * (eg sector->tag, line->args etc).
 *
 * @param id        Index of the current element being read.
 * @param dtype     Lump type class id this value is for.
 * @param prop      Property id of the game-specific variable (as declared via DED).
 * @param type      Data type id of the value pointed to by *data.
 * @param *data     Ptr to the data value (has already been expanded, size
 *                  converted and endian converted where necessary).
 *
 * @return          @c true, unless there is a critical problem with
 *                  the data supplied.
 */
int P_HandleMapDataProperty(uint id, int dtype, int prop, int type, void *data)
{
    // Make sure the property id Doomsday passed makes sense.
    int         pid = DDPropIDToID(prop);
    void       *dest = NULL;

    if(pid == -1)
        Con_Error("P_HandleMapDataProperty: Invalid property ID %i", prop);

    // Assign the value for this property to it's correct place.
    switch(pid)
    {
    // Sector properties
    case CMP_SECTOR_SPECIAL:
        xsectors[id].special = *(short *)data;
        break;
    case CMP_SECTOR_TAG:
        xsectors[id].tag = *(short *)data;
        break;
    // Line properties
    case CMP_LINE_SPECIAL:
        xlines[id].special = *(short *)data;
        break;
    case CMP_LINE_TAG:
        xlines[id].tag = *(short *)data;
        break;
    case CMP_LINE_FLAGS:
        xlines[id].flags = *(short *)data;
        break;
    // Thing properties
    case CMP_THING_POS_X:
        things[id].x = *(short *)data;
        break;
    case CMP_THING_POS_Y:
        things[id].y = *(short *)data;
        break;
    case CMP_THING_ANGLE:
        things[id].angle = *(short *)data;
        break;
    case CMP_THING_TYPE:
        things[id].type = *(short *)data;
        break;
    case CMP_THING_FLAGS:
        things[id].options = *(short *)data;
        break;

    default:
        break;
    }

    return 1;
}

/**
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
int P_HandleMapDataPropertyValue(uint id, int dtype, int prop,
                                 int type, void *data)
{
    switch(dtype)
    {
    case DAM_SIDE:
        switch(prop)
        {
        case DAM_TOP_MATERIAL:
        case DAM_MIDDLE_MATERIAL:
        case DAM_BOTTOM_MATERIAL:
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
int P_HandleMapObjectStatusReport(int code, uint id, int dtype, void *data)
{
    switch(code)
    {
    case DMUSC_SECTOR_ISBENIGN:
        // A benign sector is one which has zero lines.
        // Zero it's tag to prevent it from being selected while searching for
        // sectors to act on (eg XG and the "built-in" line specials).
        xsectors[id].tag = 0;
        break;

    case DMUSC_LINE_FIRSTRENDERED:
        // Called the first time the given line is rendered.
        // *data is a pointer to int, giving the player id which has seen it.
        // We'll utilize this to mark it as being visible in the automap.
        AM_UpdateLinedef(*(int *) data, id, true);
        break;

    default:
        break;
    }

    return 1;
}
