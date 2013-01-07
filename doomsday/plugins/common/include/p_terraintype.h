/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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
 * Material -> TerrainType xlat database.
 */

#ifndef LIBCOMMON_TERRAINTYPE_H
#define LIBCOMMON_TERRAINTYPE_H

#define TTF_NONSOLID        0x1 /* Various implications:
                                   1) Bouncing mobjs destroyed on contact.
                                   2) Able mobjs can dive/surface. */
#define TTF_FLOORCLIP       0x2 // Mobjs contacting this terrain will sink into it.
#define TTF_FRICTION_LOW    0x4
#define TTF_FRICTION_HIGH   0x8

// Mobjs contacting this cause various fx to be spawned:
#if __JHERETIC__ || __JHEXEN__
#define TTF_SPAWN_SPLASHES  0x10
#define TTF_SPAWN_SMOKE     0x20
#define TTF_SPAWN_SLUDGE    0x40
#endif

#if __JHEXEN__
#define TTF_DAMAGING        0x80
#endif

typedef struct terraindef_s {
    char*           name; // Symbolic name for this terrain.
    short           flags; // TTF_* terrain type flags.
} terraintype_t;

void P_InitTerrainTypes(void);
void P_ShutdownTerrainTypes(void);
void P_ClearTerrainTypes(void);

const terraintype_t* P_TerrainTypeForMaterial(material_t* mat);
const terraintype_t* P_PlaneMaterialTerrainType(Sector* sec, int plane);

#endif /* LIBCOMMON_TERRAINTYPE_H */
