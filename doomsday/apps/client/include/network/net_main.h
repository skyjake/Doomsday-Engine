/** @file
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

/**
 * Network Subsystem.
 */

#pragma once

#include <stdio.h>
#include "dd_share.h"
#include "net_msg.h"
#include <de/record.h>
#include <de/legacy/smoother.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NSP_BROADCAST       -1     // For Net_SendBuffer.

void            Net_Register(void);
void            Net_Init(void);
void            Net_Shutdown(void);
dd_bool         Net_GetPacket(void);
void            Net_SendBuffer(int to_player, int sp_flags);
void            Net_SendPlayerInfo(int srcPlrNum, int destPlrNum);
void            Net_InitGame(void);
void            Net_StartGame(void);
void            Net_StopGame(void);
void            Net_SendPing(int player, int count);
void            Net_PingResponse(void);
void            Net_ShowPingSummary(int player);
void            Net_WriteChatMessage(int from, int toMask, const char* message);
void            Net_ShowChatMessage(int plrNum, const char* message);
int             Net_TimeDelta(byte now, byte then);
void            Net_Update(void);
void            Net_ResetTimer(void);
void            Net_Ticker(void);
dd_bool         Net_IsLocalPlayer(int pNum);

#ifdef __cplusplus
} // extern "C"
#endif
