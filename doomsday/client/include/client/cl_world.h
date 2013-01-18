/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * cl_world.h: Clientside World Management
 */

#ifndef __DOOMSDAY_CLIENT_WORLD_H__
#define __DOOMSDAY_CLIENT_WORLD_H__

#ifdef __cplusplus
extern "C" {
#endif

void            Cl_WorldInit(void);
void            Cl_WorldReset(void);

/**
 * Handles the PSV_MATERIAL_ARCHIVE packet sent by the server. The list of
 * materials is stored until the client disconnects.
 */
void Cl_ReadServerMaterials(void);

/**
 * Handles the PSV_MOBJ_TYPE_ID_LIST packet sent by the server.
 */
void Cl_ReadServerMobjTypeIDs(void);

/**
 * Handles the PSV_MOBJ_STATE_ID_LIST packet sent by the server.
 */
void Cl_ReadServerMobjStateIDs(void);

int Cl_LocalMobjType(int serverMobjType);
int Cl_LocalMobjState(int serverMobjState);

void            Cl_SetPolyMover(uint number, int move, int rotate);

void            Cl_ReadSectorDelta2(int deltaType, boolean skip);
void            Cl_ReadSideDelta2(int deltaType, boolean skip);
void            Cl_ReadPolyDelta2(boolean skip);

int             Cl_ReadSectorDelta(void); // obsolete
int             Cl_ReadSideDelta(void); // obsolete
int             Cl_ReadPolyDelta(void); // obsolete

#ifdef __cplusplus
} // extern "C"
#endif

#endif
