/**\file
 *\section License
 * License: GPL
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
 */

/**
 * r_world.h: World Setup and Refresh
 */

#ifndef __DOOMSDAY_REFRESH_WORLD_H__
#define __DOOMSDAY_REFRESH_WORLD_H__

#include "r_data.h"

// Used for vertex sector owners, side line owners and reverb subsectors.
typedef struct ownernode_s {
    void       *data;
    struct ownernode_s* next;
} ownernode_t;

typedef struct {
    ownernode_t *head;
    uint        count;
} ownerlist_t;

extern int      rendSkyLight;      // cvar

// Map Info flags.
#define MIF_FOG             0x1    // Fog is used in the level.
#define MIF_DRAW_SPHERE     0x2    // Always draw the sky sphere.

const float    *R_GetSectorLightColor(sector_t *sector);
boolean         R_IsSkySurface(surface_t *surface);
void            R_SetupMap(int mode, int flags);
void            R_InitLinks(gamemap_t *map);
void            R_PolygonizeMap(gamemap_t *map);
void            R_PrepareForBias(gamemap_t *map);
void            R_BuildSectorLinks(gamemap_t *map);
void            R_SetupFog(float start, float end, float density, float *rgb);
void            R_SetupFogDefaults(void);
void            R_SetupSky(ded_mapinfo_t *mapInfo);
sector_t       *R_GetLinkedSector(subsector_t *startssec, uint plane);
void            R_UpdatePlanes(void);
void            R_ClearSectorFlags(void);
void            R_BuildVertexOwners(gamemap_t *map);
void            R_InitSkyFix(void);
void            R_SkyFix(boolean fixFloors, boolean fixCeilings);
void            R_OrderVertices(const line_t *line, const sector_t *sector,
                                vertex_t *verts[2]);
plane_t        *R_NewPlaneForSector(sector_t *sec);
void            R_DestroyPlaneOfSector(uint id, sector_t *sec);

void            R_CreateSurfaceDecoration(surface_t *suf, float pos[3],
                                          ded_decorlight_t *def);
void            R_ClearSurfaceDecorations(surface_t *suf);

void            R_UpdateWatchedPlanes(watchedplanelist_t *wpl);
void            R_InterpolateWatchedPlanes(watchedplanelist_t *wpl,
                                           boolean resetNextViewer);
void            R_AddWatchedPlane(watchedplanelist_t *wpl, plane_t *pln);
boolean         R_RemoveWatchedPlane(watchedplanelist_t *wpl,
                                     const plane_t *pln);

lineowner_t    *R_GetVtxLineOwner(vertex_t *vtx, line_t *line);
line_t         *R_FindLineNeighbor(sector_t *sector, line_t *line,
                                   lineowner_t *own,
                                   boolean antiClockwise, binangle_t *diff);
line_t         *R_FindSolidLineNeighbor(sector_t *sector, line_t *line,
                                        lineowner_t *own,
                                        boolean antiClockwise,
                                        binangle_t *diff);
line_t         *R_FindLineBackNeighbor(sector_t *sector, line_t *line,
                                       lineowner_t *own,
                                       boolean antiClockwise,
                                       binangle_t *diff);
#endif
