/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sv_def.h: Server Definitions
 */

#ifndef __DOOMSDAY_SERVER_H__
#define __DOOMSDAY_SERVER_H__

#include "dd_def.h"
#include "m_string.h"

#define SV_WELCOME_STRING "Doomsday "DOOMSDAY_VERSION_TEXT" Server (R6)"

// Anything closer than this is always taken into consideration when 
// deltas are being generated.
#define CLOSE_MOBJ_DIST		512

// Anything farther than this will never be taken into consideration.
#define FAR_MOBJ_DIST		1500

extern int sv_maxPlayers;
extern int allowFrames;	// Allow sending of frames.
extern int send_all_players;
extern int frameInterval;	// In tics.
extern int net_remoteuser;	// The client who is currently logged in.
extern char *net_password;	// Remote login password.

void Sv_Shutdown(void);
void Sv_StartNetGame();
boolean Sv_PlayerArrives(unsigned int nodeID, char *name);
void Sv_PlayerLeaves(unsigned int nodeID);
void Sv_Handshake(int playernum, boolean newplayer);
void Sv_GetPackets(void);
void Sv_SendText(int to, int con_flags, char *text);
void Sv_FixLocalAngles();
void Sv_Ticker(timespan_t time);
int Sv_Latency(byte cmdtime);
void Sv_Kick(int who);
void Sv_GetInfo(serverinfo_t *info);
int Sv_InfoToString(serverinfo_t *info, ddstring_t *msg);
boolean Sv_StringToInfo(const char *valuePair, serverinfo_t *info);
int Sv_GetNumPlayers(void);
int Sv_GetNumConnected(void);

boolean	Sv_CheckBandwidth(int playerNumber);

#endif
