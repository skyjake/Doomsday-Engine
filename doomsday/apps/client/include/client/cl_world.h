/** @file cl_world.h  Clientside world management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_WORLD_MAP_H
#define DE_CLIENT_WORLD_MAP_H

void Cl_InitTransTables();
void Cl_ResetTransTables();

/**
 * Handles the PSV_MATERIAL_ARCHIVE packet sent by the server. The list of
 * materials is stored until the client disconnects.
 */
void Cl_ReadServerMaterials();

/**
 * Handles the PSV_MOBJ_TYPE_ID_LIST packet sent by the server.
 */
void Cl_ReadServerMobjTypeIDs();

/**
 * Handles the PSV_MOBJ_STATE_ID_LIST packet sent by the server.
 */
void Cl_ReadServerMobjStateIDs();

int Cl_LocalMobjType(int serverMobjType);
int Cl_LocalMobjState(int serverMobjState);

/**
 * Reads a sector delta from the PSV_FRAME2 message buffer and applies it to the world.
 */
void Cl_ReadSectorDelta(int deltaType);

/**
 * Reads a side delta from the message buffer and applies it to the world.
 */
void Cl_ReadSideDelta(int deltaType);

/**
 * Reads a poly delta from the message buffer and applies it to the world.
 */
void Cl_ReadPolyDelta();

#endif // DE_CLIENT_WORLD_MAP_H
