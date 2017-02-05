/** @file sv_def.h  Server Definitions.
 *
 * @ingroup server
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef SERVER_DEF_H
#define SERVER_DEF_H

#ifndef __cplusplus
#  error "server/sv_def.h requires C++"
#endif

#include <de/libcore.h>
#include <de/Record>
#include "dd_def.h"
#include "network/protocol.h"

struct material_s;

#define SV_WELCOME_STRING   "Doomsday " DOOMSDAY_VERSION_TEXT " Server (R22)"

// Anything closer than this is always taken into consideration when
// deltas are being generated.
#define CLOSE_MOBJ_DIST     512

// Anything farther than this will never be taken into consideration.
#define FAR_MOBJ_DIST       1500

extern de::dint svMaxPlayers;
extern de::dint allowFrames;    ///< Allow sending of frames.
extern de::dint frameInterval;  ///< In tics.
extern de::dint netRemoteUser;  ///< The client who is currently logged in.
extern char *netPassword;       ///< Remote login password.

void Sv_Shutdown();

void Sv_StartNetGame();

void Sv_StopNetGame();

dd_bool Sv_PlayerArrives(nodeid_t nodeID, char const *name);

void Sv_PlayerLeaves(nodeid_t nodeID);

void Sv_Handshake(de::dint playernum, dd_bool newplayer);

void Sv_GetPackets();

/**
 * Sends a console message to one or more clients.
 *
 * @param to     Client number to send @a msg to. Use a negative number to broadcast
 *               the message to everyone.
 * @param flags  @ref consolePrintFlags
 * @param msg    Message to send.
 */
void Sv_SendText(de::dint to, de::dint flags, char const *msg);

void Sv_Ticker(timespan_t ticLength);

de::dint Sv_Latency(byte cmdTime);

void Sv_Kick(de::dint who);

//void Sv_GetInfo(serverinfo_t *info);

//de::dsize Sv_InfoToString(serverinfo_t *info, ddstring_t *msg);

//de::Record *Sv_InfoToRecord(serverinfo_t *info);

de::dint Sv_GetNumPlayers();

de::dint Sv_GetNumConnected();

dd_bool Sv_CheckBandwidth(de::dint playerNumber);

dd_bool Sv_CanTrustClientPos(de::dint plrNum);

/**
 * Returns a unique id for material @a mat that can be passed on to clients.
 */
de::duint Sv_IdForMaterial(world::Material *mat);

#endif  // SERVER_DEF_H
