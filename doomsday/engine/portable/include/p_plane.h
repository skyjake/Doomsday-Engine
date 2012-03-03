/**\file p_plane.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Map plane.
 */

#ifndef LIBDENG_MAP_PLANE_H
#define LIBDENG_MAP_PLANE_H

#include "r_data.h"
#include "p_dmu.h"

// Return the index of plane within a sector's planes array.
#define GET_PLANE_IDX(pln)      ( (int) ((pln) - (pln)->sector->planes[0]) )

/**
 * Get the value of a plane property, selected by DMU_* name.
 */
int Plane_GetProperty(const plane_t* plane, setargs_t* args);

/**
 * Update the plane, property is selected by DMU_* name.
 */
int Plane_SetProperty(plane_t* plane, const setargs_t* args);

#endif /* LIBDENG_MAP_PLANE_H */
