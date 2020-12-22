/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * p_setup.h: Map setup routines
 */

#ifndef __P_SETUP_H__
#define __P_SETUP_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

// Map objects and their properties:
enum {
    MO_NONE = 0,
    MO_THING,
    MO_XLINEDEF,
    MO_XSECTOR,
    MO_X,
    MO_Y,
    MO_Z,
    MO_ANGLE,
    MO_TYPE,
    MO_DOOMEDNUM,
    MO_SKILLMODES,
    MO_FLAGS,
    MO_TAG,
};

#ifdef __cplusplus
extern "C" {
#endif

void            P_RegisterMapObjs(void);

int             P_HandleMapDataPropertyValue(uint id, int dtype, int prop, valuetype_t type, void* data);
int             P_HandleMapObjectStatusReport(int code, uint id, int dtype, void* data);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
