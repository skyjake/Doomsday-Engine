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
 * sv_main.c: Network Server
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_misc.h"

#include "r_world.h"

// MACROS ------------------------------------------------------------------

// This is absolute maximum bandwidth rating. Frame size is practically
// unlimited with this score.
#define MAX_BANDWIDTH_RATING	100

// Maximum length of a token in the textual representation of
// serverinfo.
#define	TOKEN_LEN 128
#define VALID_LABEL_LEN	16

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void Sv_ClientCoords(int playerNum);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int net_remoteuser = 0;		// The client who is currently logged in.
char *net_password = "";	// Remote login password.

// This is the limit when accepting new clients.
int sv_maxPlayers = MAXPLAYERS;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Fills the provided struct with information about the local server.
 */
void Sv_GetInfo(serverinfo_t *info)
{
	int i;

	memset(info, 0, sizeof(*info));

	// Let's figure out what we want to tell about ourselves.
	info->version = DOOMSDAY_VERSION;
	strncpy(info->game, gx.Get(DD_GAME_ID), sizeof(info->game) - 1);
	strncpy(info->gameMode, gx.Get(DD_GAME_MODE), 
		sizeof(info->gameMode) - 1);
	strncpy(info->gameConfig, gx.Get(DD_GAME_CONFIG), 
		sizeof(info->gameConfig) - 1);
	strncpy(info->name, serverName, sizeof(info->name) - 1);
	strncpy(info->description, serverInfo, sizeof(info->description) - 1);
	info->numPlayers = Sv_GetNumPlayers();

	// The server player is there, it's just hidden.
	info->maxPlayers = MAXPLAYERS - (isDedicated? 1 : 0);

	// Don't go over the limit.
	if(info->maxPlayers > sv_maxPlayers) info->maxPlayers = sv_maxPlayers;

	info->canJoin = (isServer != 0 && Sv_GetNumPlayers() < sv_maxPlayers);

	// Identifier of the current map.
	strncpy(info->map, R_GetCurrentLevelID(), sizeof(info->map) - 1);
	
	// These are largely unused at the moment... Mainly intended for 
	// the game's custom values.
	memcpy(info->data, serverData, sizeof(info->data));

	// Also include the port we're using.
	info->port = nptIPPort;

	// Let's compile a list of client names.
	for(i = 0; i < MAXPLAYERS; ++i)
		if(clients[i].connected)
		{
			M_LimitedStrCat(clients[i].name, 15, ';', info->clientNames, 
				sizeof(info->clientNames));
		}

	// Some WAD names.
	W_GetIWADFileName(info->iwad, sizeof(info->iwad) - 1);
	W_GetPWADFileNames(info->pwads, sizeof(info->pwads), ';');

	// This should be a CRC number that describes all the loaded data.
	info->wadNumber = W_CRCNumber();
}

/*
 * Returns the length of the string.
 */
int Sv_InfoToString(serverinfo_t *info, ddstring_t *msg)
{
	int i;
	
	Str_Appendf(msg, "port:%i\n", info->port);
	Str_Appendf(msg, "name:%s\n", info->name);
	Str_Appendf(msg, "info:%s\n", info->description);
	Str_Appendf(msg, "ver:%i\n", info->version);
	Str_Appendf(msg, "game:%s\n", info->game);
	Str_Appendf(msg, "mode:%s\n", info->gameMode);
	Str_Appendf(msg, "setup:%s\n", info->gameConfig);
	Str_Appendf(msg, "iwad:%s\n", info->iwad);
	Str_Appendf(msg, "wcrc:%i\n", info->wadNumber);
	Str_Appendf(msg, "pwads:%s\n", info->pwads);
	Str_Appendf(msg, "map:%s\n", info->map);
	Str_Appendf(msg, "nump:%i\n", info->numPlayers);
	Str_Appendf(msg, "maxp:%i\n", info->maxPlayers);
	Str_Appendf(msg, "open:%i\n", info->canJoin);
	Str_Appendf(msg, "plrn:%s\n", info->clientNames);
	for(i = 0; i < sizeof(info->data)/sizeof(info->data[0]); i++)
	{
		Str_Appendf(msg, "data%i:%x\n", i, info->data[i]);
	}
	return Str_Length(msg);
}

/*
 * Extracts the label and value from a string.  'max' is the maximum
 * allowed length of a token, including terminating \0.
 */
static boolean Sv_Tokenize(const char *line, char *label, char *value,
						   int max)
{
	const char *src = line;
	const char *colon = strchr(src, ':');

	// The colon must exist near the beginning.
	if(!colon || colon - src >= VALID_LABEL_LEN) return false;

	// Copy the label.
	memset(label, 0, max);
	strncpy(label, src, MIN_OF(colon - src, max - 1));

	// Copy the value.
	memset(value, 0, max);
	strncpy(value, colon + 1, 
		MIN_OF(strlen(line) - (colon - src + 1), max - 1));

	// Everything is OK.
	return true;
}

/*
 * Converts textual data to a serverinfo struct. Returns true if the
 * label/value pair is recognized.
 */
boolean Sv_StringToInfo(const char *valuePair, serverinfo_t *info)
{
	char label[TOKEN_LEN], value[TOKEN_LEN];
	
	// Extract the label and value. The maximum length of a value is
	// TOKEN_LEN. Labels are returned in lower case.
	if(!Sv_Tokenize(valuePair, label, value, sizeof(value)))
	{
		// Badly formed lines are ignored.
		return false;
	}

	if(!strcmp(label, "at"))
	{
		strncpy(info->address, value, sizeof(info->address) - 1);
	}
	else if(!strcmp(label, "port"))
	{
		info->port = strtol(value, 0, 0);
	}
	else if(!strcmp(label, "ver"))
	{
		info->version = strtol(value, 0, 0);
	}
	else if(!strcmp(label, "map"))
	{
		strncpy(info->map, value, sizeof(info->map) - 1);
	}
	else if(!strcmp(label, "game"))
	{
		strncpy(info->game, value, sizeof(info->game) - 1);
	}
	else if(!strcmp(label, "name"))
	{
		strncpy(info->name, value, sizeof(info->name) - 1);
	}
	else if(!strcmp(label, "info"))
	{
		strncpy(info->description, value, sizeof(info->description) - 1);
	}
	else if(!strcmp(label, "nump"))
	{
		info->numPlayers = strtol(value, 0, 0);
	}
	else if(!strcmp(label, "maxp"))
	{
		info->maxPlayers = strtol(value, 0, 0);
	}
	else if(!strcmp(label, "open"))
	{
		info->canJoin = strtol(value, 0, 0);
	}
	else if(!strcmp(label, "mode"))
	{
		strncpy(info->gameMode, value, sizeof(info->gameMode) - 1);
	}
	else if(!strcmp(label, "setup"))
	{
		strncpy(info->gameConfig, value, sizeof(info->gameConfig) - 1);
	}
	else if(!strcmp(label, "iwad"))
	{
		strncpy(info->iwad, value, sizeof(info->iwad) - 1);
	}
	else if(!strcmp(label, "wcrc"))
	{
		info->wadNumber = strtol(value, 0, 0);
	}
	else if(!strcmp(label, "pwads"))
	{
		strncpy(info->pwads, value, sizeof(info->pwads) - 1);
	}
	else if(!strcmp(label, "plrn"))
	{
		strncpy(info->clientNames, value, sizeof(info->clientNames) - 1);
	}
	else if(!strcmp(label, "data0"))
	{
		info->data[0] = strtol(value, 0, 16);
	}
	else if(!strcmp(label, "data1"))
	{
		info->data[1] = strtol(value, 0, 16);
	}
	else if(!strcmp(label, "data2"))
	{
		info->data[2] = strtol(value, 0, 16);
	}
	else
	{
		// Unknown labels are ignored.
		return false;
	}
	return true;
}

/*
 * Returns gametic - cmdtime.
 */
int Sv_Latency(byte cmdtime)
{
	return Net_TimeDelta(gametic, cmdtime);
}

//===========================================================================
// Sv_FixLocalAngles
//	For local players.
//===========================================================================
void Sv_FixLocalAngles()
{
	ddplayer_t *pl;
	int i;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		pl = players + i;
		if(!pl->ingame || !(pl->flags & DDPF_LOCAL)) continue;
	
		// This is not for clients.
		if(isDedicated && i == 0) continue;
	
		if(pl->flags & DDPF_FIXANGLES)
		{
			pl->flags &= ~DDPF_FIXANGLES;
			pl->clAngle = pl->mo->angle;
			pl->clLookDir = pl->lookdir;
		}
	}
}

void Sv_HandlePacket(void)
{
	ident_t id;
	int i, mask, from = netBuffer.player;
	ddplayer_t *pl = &players[from];
	client_t *sender = &clients[from];
	playerinfo_packet_t info;
	int msgfrom;
	char *msg;
	char buf[17];

	switch(netBuffer.msg.type)
	{
	case pcl_hello:
	case pcl_hello2:
		// Get the ID of the client.
		id = Msg_ReadLong();
		Con_Printf("Sv_HandlePacket: Hello from client %i (%08X).\n", 
			from, id);

		// Check for duplicate IDs.
		if(!pl->ingame && !sender->handshake)
		{
			for(i = 0; i < MAXPLAYERS; i++)
			{
				if(clients[i].connected && clients[i].id == id)
				{
					// Send a message to everybody.
					Con_FPrintf(CBLF_TRANSMIT | SV_CONSOLE_FLAGS, 
						"New client connection refused: Duplicate ID "
						"(%08x).\n", id);
					N_TerminateClient(from);
					break;
				}
			}
			if(i < MAXPLAYERS) break; // Can't continue, refused!
		}

		// This is OK.
		sender->id = id;
		
		if(netBuffer.msg.type == pcl_hello2)
		{
			// Check the game mode (max 16 chars).
			Msg_Read(buf, 16);
			if(strnicmp(buf, gx.Get(DD_GAME_MODE), 16))
			{
				Con_Printf("  Bad Game ID: %-.16s\n", buf);
				N_TerminateClient(from);
				break;
			}
		}
		
		// The client requests a handshake.
		if(!pl->ingame && !sender->handshake)
		{
			// This'll be true until the client says it's ready.
			sender->handshake = true;

			// The player is now in the game.
			players[from].ingame = true;

			// Tell the game about this.
			gx.NetPlayerEvent(from, DDPE_ARRIVAL, 0);

			// Send the handshake packets.
			Sv_Handshake(from, true);

			// Note the time when the player entered.
			sender->enterTime = gametic;
			sender->runTime = gametic-1;
			//sender->lagStress = 0;
		}
		else if(pl->ingame)
		{
			// The player is already in the game but requests a new 
			// handshake. Perhaps it's starting to record a demo.
			Sv_Handshake(from, false);
		}
		break;

	case pkt_ok:
		// The client says it's ready to receive frames.
		sender->ready = true;
#ifdef _DEBUG
		Con_Printf("Sv_HandlePacket: OK (\"ready!\") from client %i "
			"(%08X).\n", from, sender->id);
#endif
		if(sender->handshake)
		{
			// The handshake is complete. The client has acknowledged it 
			// and sends its regards.
			sender->handshake = false;
			// Send a clock sync message.
			Msg_Begin(psv_sync);
			Msg_WriteLong(gametic + (sender->shakePing*35)/2000);
			// Send reliably, although if it has to be resent, the tics
			// will already be way off...
			Net_SendBuffer(from, SPF_CONFIRM);
			// Send welcome string.
			Sv_SendText(from, SV_CONSOLE_FLAGS, SV_WELCOME_STRING"\n");
		}
		break;

	case pkt_chat:
		// The first byte contains the sender.
		msgfrom = Msg_ReadByte();	
		// Is the message for us?
		mask = Msg_ReadShort();	
		// Copy the message into a buffer.
		msg = Z_Malloc(netBuffer.length - 3, PU_STATIC, 0);
		strcpy(msg, netBuffer.cursor);
		// Message for us? Show it locally.
		if(mask & 1) 
		{
			Net_ShowChatMessage();
			gx.NetPlayerEvent(msgfrom, DDPE_CHAT_MESSAGE, msg);
		}
		// Servers relay chat messages to all the recipients.
		Msg_Begin(pkt_chat);
		Msg_WriteByte(msgfrom);
		Msg_WriteShort(mask);
		Msg_Write(msg, strlen(msg) + 1);
		for(i = 1; i < MAXPLAYERS; i++)
			if(players[i].ingame && mask & (1<<i) && i != from)
			{
				Net_SendBuffer(i, SPF_ORDERED);
			}
		Z_Free(msg);
		break;

	case pkt_player_info:
		Msg_Read(&info, sizeof(info));
		Con_FPrintf(CBLF_TRANSMIT | SV_CONSOLE_FLAGS,
			"%s renamed to %s.\n", sender->name, info.name);
		strcpy(sender->name, info.name);
		Net_SendPacket(DDSP_CONFIRM | DDSP_ALL_PLAYERS, pkt_player_info,
			&info, sizeof(info));
		break;
	}
}

//========================================================================
//
// Sv_Login
//
// Handles a login packet. If the password is OK and no other client
// is current logged in, a response is sent.
//
//========================================================================

void Sv_Login(void)
{
	if(net_remoteuser) 
	{
		Sv_SendText(netBuffer.player, SV_CONSOLE_FLAGS,
			"Sv_Login: A client is already logged in.\n");
		return;
	}
	// Check the password.
	if(strcmp(netBuffer.cursor, net_password))
	{
		Sv_SendText(netBuffer.player, SV_CONSOLE_FLAGS,
			"Sv_Login: Invalid password.\n");
		return;
	}
	// OK!
	net_remoteuser = netBuffer.player;
	Con_Printf("Sv_Login: %s (client %i) logged in.\n",
		clients[net_remoteuser].name, net_remoteuser);
	// Send a confirmation packet to the client.
	Msg_Begin(pkt_login);
	Msg_WriteByte(true);	// Yes, you're logged in.
	Net_SendBuffer(net_remoteuser, SPF_ORDERED);
}

//========================================================================
//
// Sv_ExecuteCommand
//
// Executes the command in the message buffer.
// Usually sent by Con_Send.
//
//========================================================================

void Sv_ExecuteCommand(void)
{
	unsigned short len;
	boolean silent;
	
	if(!net_remoteuser) 
	{
		Con_Printf("Sv_ExecuteCommand: Cmd received but no one's logged in!\n");
		return; 
	}
	// The command packet is very simple.
	len = Msg_ReadShort();
	silent = (len & 0x8000) != 0;
	len &= 0x7fff;
	// Verify using string length.
	if(strlen(netBuffer.cursor) != (unsigned) len-1)
	{
		Con_Printf("Sv_ExecuteCommand: Damaged packet?\n");
		return;
	}
	Con_Execute(netBuffer.cursor, silent);		
}

/*
 * Server's packet handler.
 */
void Sv_GetPackets(void)
{
	int             netconsole;
	int				start, num, i;
	client_t		*sender;
	byte			*unpacked;

	while(Net_GetPacket())
	{
		switch(netBuffer.msg.type)
		{
		case pcl_commands:
			// Determine who sent this packet.
			netconsole = netBuffer.player;
			if(netconsole < 0 || netconsole >= MAXPLAYERS) continue; 
			
			sender = &clients[netconsole];

			// If the client isn't ready, don't accept any cmds.
			if(!clients[netconsole].ready) continue;

			// Now we know this client is alive, update the frame send count.
			// Clients will only be refreshed if their updateCount is greater
			// than zero.
			sender->updateCount = UPDATECOUNT;
			
			// Unpack the commands in the packet. Since the game defines the
			// ticcmd_t structure, it is the only one who can do this.
			unpacked = (byte*) gx.NetPlayerEvent(netBuffer.length, 
				DDPE_READ_COMMANDS, netBuffer.msg.data);

			// The first two bytes contain the number of commands.
			num = *(ushort*) unpacked;
			unpacked += 2;

			// Add the tics into the client's ticcmd buffer, if there is room.
			// If the buffer overflows, the rest of the cmds will be forgotten.
			if(sender->numTics + num > BACKUPTICS) 
			{
				num = BACKUPTICS - sender->numTics;
			}
			start = sender->firstTic + sender->numTics;
			
			// Increase the counter.
			sender->numTics += num;

			// Copy as many as fits (circular buffer).
			for(i = start; num > 0; num--, i++)
			{
				if(i >= BACKUPTICS) i -= BACKUPTICS;
				memcpy(sender->ticCmds+TICCMD_IDX(i), unpacked, TICCMD_SIZE);
				unpacked += TICCMD_SIZE;
			}
			break;

		case pcl_ack_sets:
			// The client is acknowledging that it has received a number of
			// delta sets.
			while(!Msg_End())
			{
				Sv_AckDeltaSet(netBuffer.player, Msg_ReadByte(), 0);
			}
			break;

		case pcl_acks:
			// The client is acknowledging both entire sets and resent deltas.
			// The first byte contains the acked set.
			Sv_AckDeltaSet(netBuffer.player, Msg_ReadByte(), 0);

			// The rest of the packet contains resend IDs.
			while(!Msg_End())
			{
				Sv_AckDeltaSet(netBuffer.player, 0, Msg_ReadByte());
			}
			break;

		case pkt_coords:
			Sv_ClientCoords(netBuffer.player);
			break;

		case pcl_ack_shake:
			// The client has acknowledged our handshake.
			// Note the time (this isn't perfectly accurate, though).
			netconsole = netBuffer.player;
			if(netconsole < 0 || netconsole >= MAXPLAYERS) continue; 

			sender = &clients[netconsole];
			sender->shakePing = Sys_GetRealTime() - sender->shakePing;
			Con_Printf("Cl%i handshake ping: %i ms\n", netconsole, 
				sender->shakePing);

			// Update the initial ack time accordingly. Since the ping
			// fluctuates, assume the a poor case.
			Net_SetInitialAckTime(netconsole, 2 * sender->shakePing);
			break;

		case pkt_ping:
			Net_PingResponse();
			break;

		case pcl_hello:
		case pcl_hello2:
		case pkt_ok:
		case pkt_chat:
		case pkt_player_info:
			Sv_HandlePacket();
			break;

		case pkt_login:
			Sv_Login();
			break;

		case pkt_command:
			Sv_ExecuteCommand();
			break;

		default:
			if(netBuffer.msg.type >= pkt_game_marker)
			{
				// A client has sent a game specific packet.
				gx.HandlePacket(netBuffer.player,
					netBuffer.msg.type, netBuffer.msg.data,
					netBuffer.length);
			}
		}
	}
}

/*
 * Assign a new console to the player. Returns true if successful.
 * Called by N_Update().
 */
boolean Sv_PlayerArrives(unsigned int nodeID, char *name)
{
	int		i;

	Con_Message("Sv_PlayerArrives: '%s' has arrived.\n", name);

	// We need to find the new player a client entry.
	for(i = 1; i < MAXPLAYERS; i++)
		if(!clients[i].connected)
		{
			// This'll do.
			clients[i].connected = true;
			clients[i].ready = false;
			clients[i].nodeID = nodeID;
			clients[i].viewConsole = i;
			clients[i].lastTransmit = -1;
			strncpy(clients[i].name, name, PLAYERNAMELEN);
			
			Sv_InitPoolForClient(i);

			VERBOSE( Con_Printf("Sv_PlayerArrives: '%s' assigned to "
				"console %i (node: %x)\n", clients[i].name, i, nodeID) );

			// In order to get in the game, the client must first
			// shake hands. It'll request this by sending a Hello packet.
			// We'll be waiting...
			clients[i].handshake = false;
			clients[i].updateCount = UPDATECOUNT;
			return true;
		}

	return false;
}

/*
 * Remove the specified player from the game. Called by N_Update().
 */
void Sv_PlayerLeaves(unsigned int nodeID)
{
	int i, pNumber = -1;
	boolean wasInGame;

	// First let's find out who this node actually is.
	for(i = 0; i < MAXPLAYERS; i++)
		if(clients[i].nodeID == nodeID)
		{
			pNumber = i;
			break;
		}
	if(pNumber == -1) return; // Bogus?

	// Log off automatically.
	if(net_remoteuser == pNumber) net_remoteuser = 0;

	// Print a little something in the console.
	Con_Message("Sv_PlayerLeaves: '%s' (console %i) has left.\n", 
		clients[pNumber].name, pNumber);

	wasInGame = players[pNumber].ingame;
	players[pNumber].ingame = false;
	clients[pNumber].connected = false;
	clients[pNumber].ready = false;
	clients[pNumber].updateCount = 0;
	clients[pNumber].handshake = false;
	clients[pNumber].nodeID = 0;
	clients[pNumber].bandwidthRating = BWR_DEFAULT;
	
	// Set a modest ack time by default.
	Net_SetInitialAckTime(pNumber, ACK_DEFAULT);

	// Remove the player's data from the register.
	Sv_PlayerRemoved(pNumber);

	if(wasInGame)
	{
		// Inform the DLL about this.
		gx.NetPlayerEvent(pNumber, DDPE_EXIT, NULL);

		// Inform other clients about this.
		Msg_Begin(psv_player_exit);
		Msg_WriteByte(pNumber);
		Net_SendBuffer(NSP_BROADCAST, SPF_CONFIRM);
	}

	// This client no longer has an ID number.
	clients[pNumber].id = 0;
}

// The player will be sent the introductory handshake packets.
void Sv_Handshake(int playernum, boolean newplayer)
{
	int						i;
	handshake_packet_t		shake;
	playerinfo_packet_t		info;

	Con_Printf("Sv_Handshake: Shaking hands with player %i.\n", playernum);

	shake.version = SV_VERSION;
	shake.yourConsole = playernum;
	shake.playerMask = 0;
	shake.gameTime = gametic;
	for(i = 0; i < MAXPLAYERS; i++)
		if(clients[i].connected)
			shake.playerMask |= 1<<i;
	Net_SendPacket(playernum | DDSP_ORDERED, psv_handshake, &shake, 
		sizeof(shake));

#if _DEBUG
	Con_Message("Sv_Handshake: plmask=%x\n", shake.playerMask);
#endif

	if(newplayer)
	{
		// Note the time when the handshake was sent.
		clients[playernum].shakePing = Sys_GetRealTime();
	}
	
	// The game DLL wants to shake hands as well?
 	gx.NetWorldEvent(DDWE_HANDSHAKE, playernum, (void*) newplayer);

	// Propagate client information.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(clients[i].connected)
		{
			info.console = i;
			strcpy(info.name, clients[i].name);
			Net_SendPacket(playernum | DDSP_ORDERED, pkt_player_info, 
				&info, sizeof(info));
		}
		// Send the new player's info to other players.
		if(newplayer && i != 0 && i != playernum && clients[i].connected)
		{
			info.console = playernum;
			strcpy(info.name, clients[playernum].name);
			Net_SendPacket(i | DDSP_CONFIRM, pkt_player_info, 
				&info, sizeof(info));
		}
	}

	if(!newplayer)
	{
		// This is not a new player (just a re-handshake) but we'll
		// nevertheless re-init the client's state register. For new 
		// players this is done in Sv_PlayerArrives.
		Sv_InitPoolForClient(playernum);
	}

	players[playernum].flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
}

void Sv_StartNetGame()
{
	int		i;

	// Reset all the counters and other data.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		players[i].ingame = false;
		clients[i].connected = false;
		clients[i].ready = false;
		clients[i].nodeID = 0;
		clients[i].numTics = 0;
		clients[i].firstTic = 0;
		clients[i].enterTime = 0;
		clients[i].runTime = -1;
		clients[i].lastTransmit = -1;
		clients[i].updateCount = UPDATECOUNT;
		clients[i].fov = 90;
		clients[i].viewConsole = i;
		memset(clients[i].name, 0, sizeof(clients[i].name));
		players[i].flags &= ~DDPF_CAMERA;
		clients[i].bandwidthRating = BWR_DEFAULT;
		clients[i].bwrAdjustTime = 0;
		memset(clients[i].ackTimes, 0, sizeof(clients[i].ackTimes));
	}
	gametic = 0;
	firstNetUpdate = true;
	net_remoteuser = 0;

	// The server is always player number zero.
	consoleplayer = displayplayer = 0; 

	netgame = true;
	isServer = true;
	
	if(!isDedicated)
	{
		players[consoleplayer].ingame = true;
		clients[consoleplayer].connected = true;
		clients[consoleplayer].ready = true;
		strcpy(clients[consoleplayer].name, playerName);	
	}
}

void Sv_SendText(int to, int con_flags, char *text)
{
	Msg_Begin(psv_console_text);
	Msg_WriteLong(con_flags & ~CBLF_TRANSMIT);
	Msg_Write(text, strlen(text)+1);
	Net_SendBuffer(to, SPF_ORDERED);
}

//===========================================================================
// Sv_Kick
//	Asks a client to disconnect. Clients will immediately disconnect
//	after receiving the psv_server_close message.
//===========================================================================
void Sv_Kick(int who)
{
	if(!clients[who].connected) return;
	Sv_SendText(who, SV_CONSOLE_FLAGS, "You were kicked out!\n");
	Msg_Begin(psv_server_close);
	Net_SendBuffer(who, SPF_ORDERED);
	//players[who].ingame = false;
}

//===========================================================================
// Sv_Ticker
//===========================================================================
void Sv_Ticker(timespan_t time)
{
	static trigger_t fixed = { 1.0/35 };
	int i;

	if(!M_CheckTrigger(&fixed, time)) return;

	// Note last angles for all players.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!players[i].ingame || !players[i].mo) continue;
		players[i].lastangle = players[i].mo->angle;

		if(clients[i].bwrAdjustTime > 0)
		{
			// BWR adjust time tics away.
			clients[i].bwrAdjustTime--;
		}
	}
}

/*
 * Returns the number of players in the game.
 */
int Sv_GetNumPlayers(void)
{
	int i, count;

	// Clients can't count.
	if(isClient) return 1;

	for(i = count = 0; i < MAXPLAYERS; i++)
	{
		if(players[i].ingame && players[i].mo) count++;
	}
	return count;
}

/*
 * Returns the number of connected clients.
 */
int Sv_GetNumConnected(void)
{
	int i, count = 0;

	// Clients can't count.
	if(isClient) return 1;

	for(i = isDedicated? 1 : 0; i < MAXPLAYERS; ++i)
		if(clients[i].connected)
			count++;

	return count;
}

/*
 * The bandwidth rating is updated according to the status of the 
 * player's send queue. Returns true if a new packet may be sent.
 */
boolean Sv_CheckBandwidth(int playerNumber)
{
	client_t *client = &clients[playerNumber];
	uint qSize = N_GetSendQueueSize(playerNumber);
	uint limit = 400; /*Sv_GetMaxFrameSize(playerNumber);*/

	// If there are too many messages in the queue, the player's bandwidth
	// is overrated.
	if(qSize > limit)
	{
		// Drop quickly to allow the send queue to clear out sooner.
		client->bandwidthRating -= 10;
	}

	// If the send queue is practically empty, we can use more bandwidth.
	// (Providing we have BWR adjust time.)
	if(qSize < limit/20 && client->bwrAdjustTime > 0)
	{
		client->bandwidthRating++;

		// Increase BWR only once during the adjust time.
		client->bwrAdjustTime = 0;
	}

	// Do not go past the boundaries, though.
	if(client->bandwidthRating < 0)
	{
		client->bandwidthRating = 0;
	}
	if(client->bandwidthRating > MAX_BANDWIDTH_RATING)
	{
		 client->bandwidthRating = MAX_BANDWIDTH_RATING;
	}

	// New messages will not be sent if there's too much already.
	return qSize <= 10*limit;
}

/*
 * Reads a pkt_coords packet from the message buffer. We trust the 
 * client's position and change ours to match it. The client better not 
 * be cheating.
 */
void Sv_ClientCoords(int playerNum)
{
/*	client_t *cl = clients + playerNum; */
	mobj_t *mo = players[playerNum].mo;
	int clx, cly, clz;
	boolean onFloor = false;

	// If mobj or player is invalid, the message is discarded.
	if(!mo || !players[playerNum].ingame
		|| players[playerNum].flags & DDPF_DEAD) return;

	clx = Msg_ReadShort() << 16;
	cly = Msg_ReadShort() << 16;
	clz = Msg_ReadShort() << 16;

	if(clz == (DDMININT & 0xffff0000))
	{
		clz = mo->floorz;
		onFloor = true;
	}

	// If we aren't about to forcibly change the client's position, update 
	// with new pos if it's valid.
	if(!(players[playerNum].flags & DDPF_FIXPOS)
		&& P_CheckPosXYZ(mo, clx, cly, clz)) // But it must be a valid pos.
	{
		P_UnlinkThing(mo);
		mo->x = clx;
		mo->y = cly;
		mo->z = clz;
		P_LinkThing(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
		mo->floorz = tmfloorz;
		mo->ceilingz = tmceilingz;
		if(onFloor)
		{
			mo->z = mo->floorz;
		}
	}
}

/*
 * Console command for terminating a remote console connection.
 */
int CCmdLogout(int argc, char **argv)
{
	// Only servers can execute this command.
	if(!net_remoteuser || !isServer) return false;
	// Notice that the server WILL execute this command when a client
	// is logged in and types "logout".
	Sv_SendText(net_remoteuser, SV_CONSOLE_FLAGS, "Goodbye...\n");
	// Send a logout packet.
	Msg_Begin(pkt_login);
	Msg_WriteByte(false);	// You're outta here.
	Net_SendBuffer(net_remoteuser, SPF_ORDERED);	
	net_remoteuser = 0;
	return true;
}


