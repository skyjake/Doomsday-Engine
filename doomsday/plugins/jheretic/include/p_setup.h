/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
 * p_setup.h: Map setup routines
 */

#ifndef __P_SETUP_H__
#define __P_SETUP_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

/** Game specific map format properties for ALL games.
* (notice jHeretic/jHexen properties are here too temporarily).
*
* \todo we don't need  to know about all of them once they
* are registered via DED.
*
* Common map format properties
*/
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

void        P_RegisterCustomMapProperties(void);
int         P_HandleMapDataProperty(uint id, int dtype, int prop,
                                    int type, void *data);
int         P_HandleMapDataPropertyValue(uint id, int dtype, int prop,
                                         int type, void *data);
void        P_Init(void);
boolean     P_MapExists(int episode, int map);
#endif
