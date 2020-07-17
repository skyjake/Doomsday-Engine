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

#ifndef DE_NETWORK_H
#define DE_NETWORK_H

#include <stdio.h>
#include "dd_share.h"
#include "net_msg.h"
#include <de/record.h>
#include <de/legacy/smoother.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIT(x)              (1 << (x))

#define NSP_BROADCAST       -1     // For Net_SendBuffer.

// Flags for console text from the server.
// Change with server version?
#define SV_CONSOLE_PRINT_FLAGS    (CPF_WHITE|CPF_LIGHT|CPF_GREEN)

// A modest acktime used by default for new clients (1 sec ping).
#define ACK_DEFAULT         1000

#define MONITORTICS         7

#define LOCALTICS           10     // Built ticcmds are stored here.
#define BACKUPTICS          70     // Two seconds worth of tics.

// The number of mobjs that can be stored in the input/visible buffer.
// The server won't send more mobjs than this.
#define MAX_CLMOBJS         80

#define DEFAULT_TCP_PORT    13209
#define DEFAULT_UDP_PORT    13209

typedef void (*expectedresponder_t)(int, const byte*, int);

// If a master action fails, the action queue is emptied.
typedef enum {
    MAC_REQUEST, // Retrieve the list of servers from the master.
    MAC_WAIT, // Wait for the server list to arrive.
    MAC_LIST // Print the server list in the console.
} masteraction_t;

// Packet types.
// PKT = sent by anyone
// PSV = only sent by server
// PCL = only sent by client
enum {
    // Messages and responses.
    PCL_HELLO = 0,
    PKT_OK = 1,
    PKT_CANCEL = 2,                 // unused?
    PKT_PLAYER_INFO = 3,
    PKT_CHAT = 4,
    PSV_FINALE = 5,
    PKT_PING = 6,
    PSV_HANDSHAKE = 7,
    PSV_SERVER_CLOSE = 8,
    PSV_FRAME = 9,                  // obsolete
    PSV_PLAYER_EXIT = 10,
    PSV_CONSOLE_TEXT = 11,
    PCL_ACK_SHAKE = 12,
    PSV_SYNC = 13,
    PSV_MATERIAL_ARCHIVE = 14,
    PCL_FINALE_REQUEST = 15,
    PKT_LOGIN = 16,
    PCL_ACK_SETS = 17,
    PKT_COORDS = 18,
    PKT_DEMOCAM = 19,
    PKT_DEMOCAM_RESUME = 20,
    PCL_HELLO2 = 21,                // Includes game ID
    PSV_FRAME2 = 22,                // Frame packet v2
    PSV_FIRST_FRAME2 = 23,          // First PSV_FRAME2 after map change
    PSV_SOUND2 = 24,                // unused?
    PSV_STOP_SOUND = 25,
    PCL_ACKS = 26,
    PSV_PLAYER_FIX_OBSOLETE = 27,   // Fix angles/pos/mom (without console number).
    PCL_ACK_PLAYER_FIX = 28,        // Acknowledge player fix. /* 28 */
    PKT_COMMAND2 = 29,
    PSV_PLAYER_FIX = 30,            // Fix angles/pos/mom.
    PCL_GOODBYE = 31,
    PSV_MOBJ_TYPE_ID_LIST = 32,
    PSV_MOBJ_STATE_ID_LIST = 33,

    // Game specific events.
    PKT_GAME_MARKER = DDPT_FIRST_GAME_EVENT, // 64
};

// Use the number defined in dd_share.h for sound packets.
// This is for backwards compatibility.
#define PSV_SOUND           71     /* DDPT_SOUND */

#define RESENDCOUNT         10
#define HANDSHAKECOUNT      17
//#define UPDATECOUNT         20

// These dd-flags are packed (i.e. included in mobj deltas).
#define DDMF_PACK_MASK      0x3cfff1ff

// A client's acknowledgement threshold depends on the average of his
// acknowledgement times.
#define NUM_ACK_TIMES       8

// The consolePlayer's camera position is written to the demo file
// every 3rd tic.
#define LOCALCAM_WRITE_TICS 3

// Maximum length of a token in the textual representation of
// serverinfo.
#define SVINFO_TOKEN_LEN        128
#define SVINFO_VALID_LABEL_LEN  16

typedef struct netstate_s
{
    dd_bool firstUpdate;
    int     netGame;  // a networked game is in progress
    int     isServer; // this computer is an open server
    int     isClient; // this computer is a client
    float   simulatedLatencySeconds;
    int     gotFrame; // a frame packet has been received
} netstate_t;

extern netstate_t netState;

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
void            Net_Ticker(timespan_t time);

dd_bool Net_IsLocalPlayer(int pNum);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DE_NETWORK_H */
