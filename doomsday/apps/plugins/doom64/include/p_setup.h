/**\file p_setup.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Map setup routines.
 */

#ifndef LIBDOOM64_SETUP_H
#define LIBDOOM64_SETUP_H

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

// Map objects and their properties:
enum {
    MO_NONE = 0,
    MO_THING,
    MO_XLINEDEF,
    MO_XSECTOR,
    MO_LIGHT,
    MO_X,
    MO_Y,
    MO_Z,
    MO_ID,
    MO_ANGLE,
    MO_TYPE,
    MO_DOOMEDNUM,
    MO_SKILLMODES,
    MO_USETYPE,
    MO_FLAGS,
    MO_TAG,
    MO_DRAWFLAGS,
    MO_TEXFLAGS,
    MO_COLORR,
    MO_COLORG,
    MO_COLORB,
    MO_FLOORCOLOR,
    MO_CEILINGCOLOR,
    MO_UNKNOWNCOLOR,
    MO_WALLTOPCOLOR,
    MO_WALLBOTTOMCOLOR,
    MO_XX0,
    MO_XX1,
    MO_XX2,
};

#ifdef __cplusplus
extern "C" {
#endif

void            P_RegisterMapObjs(void);

int             P_HandleMapDataPropertyValue(uint id, int dtype, int prop,
                                             valuetype_t type, void* data);
int             P_HandleMapObjectStatusReport(int code, uint id, int dtype,
                                              void* data);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDOOM64_SETUP_H */
