
//**************************************************************************
//**
//** SV_MAIN.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int net_remoteuser = 0;		// The client who is currently logged in.
char *net_password = "";	// Remote login password.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

// Returns gametic - cmdtime.
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
	int i, mask, from = netbuffer.player;
	ddplayer_t *pl = &players[from];
	client_t *sender = &clients[from];
	playerinfo_packet_t info;
	int msgfrom;
	char *msg;

	switch(netbuffer.msg.type)
	{
	case pcl_hello:
		// Get the ID of the client.
		sender->id = Msg_ReadLong();
		Con_Printf("Sv_HandlePacket: Hello from client %i (%08X).\n", from,
			sender->id);
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
			sender->lagStress = 0;
		}
		else if(pl->ingame)
		{
			// The player is already in the game but requests a new 
			// handshake. Perhaps it's starting to record a demo.
			Sv_Handshake(from, false);
		}
		break;

	case pkt_ok:
		if(sender->handshake)
		{
			// Not any more. The client has acknowledged the handshake
			// and sends its regards.
			sender->handshake = false;
			sender->ready = true;
			// Send a clock sync message.
			Msg_Begin(psv_sync);
			Msg_WriteLong(gametic + (sender->shakePing*35)/2000);
			Net_SendBuffer(from, SPF_RELIABLE | 10001);	// High-prio.
			// Send welcome string.
			Sv_SendText(from, SV_CONSOLE_FLAGS, SV_WELCOME_STRING"\n");
			// Send position and view angles.
			//players[from].flags |= DDPF_FIXANGLES | DDPF_FIXPOS;
		}
		break;

	case pkt_chat:
		// The first byte contains the sender.
		msgfrom = Msg_ReadByte();	
		// Is the message for us?
		mask = Msg_ReadShort();	
		// Copy the message into a buffer.
		msg = Z_Malloc(netbuffer.length - 3, PU_STATIC, 0);
		strcpy(msg, netbuffer.cursor);
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
				Net_SendBuffer(i, SPF_RELIABLE);
		Z_Free(msg);
		break;

	case pkt_player_info:
		Msg_Read(&info, sizeof(info));
		Con_FPrintf(CBLF_TRANSMIT | SV_CONSOLE_FLAGS,
			"%s renamed to %s.\n", sender->name, info.name);
		strcpy(sender->name, info.name);
		Net_SendPacket(DDSP_RELIABLE | DDSP_ALL_PLAYERS, pkt_player_info,
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
		Sv_SendText(netbuffer.player, SV_CONSOLE_FLAGS,
			"Sv_Login: A client is already logged in.\n");
		return;
	}
	// Check the password.
	if(strcmp(netbuffer.cursor, net_password))
	{
		Sv_SendText(netbuffer.player, SV_CONSOLE_FLAGS,
			"Sv_Login: Invalid password.\n");
		return;
	}
	// OK!
	net_remoteuser = netbuffer.player;
	Con_Printf("Sv_Login: %s (client %i) logged in.\n",
		clients[net_remoteuser].name, net_remoteuser);
	// Send a confirmation packet to the client.
	Msg_Begin(pkt_login);
	Msg_WriteByte(true);	// Yes, you're logged in.
	Net_SendBuffer(net_remoteuser, SPF_RELIABLE);
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
	if(strlen(netbuffer.cursor) != (unsigned) len-1)
	{
		Con_Printf("Sv_ExecuteCommand: Damaged packet?\n");
		return;
	}
	Con_Execute(netbuffer.cursor, silent);		
}

//========================================================================
//
// Sv_GetPackets
//
// Server's packet handler.
//
//========================================================================

void Sv_GetPackets (void)
{
	int             netconsole;
	int				start, num, i, time;
	client_t		*sender;
	ticcmd_t		*cmd;

	while(Net_GetPacket())
	{
		switch(netbuffer.msg.type)
		{
		case pkt_ticcmd:
			// Determine who sent this packet.
			netconsole = netbuffer.player;
			if(netconsole < 0 || netconsole >= MAXPLAYERS) continue; 
			
			sender = &clients[netconsole];

			// If the client isn't ready, don't accept any cmds.
			if(!clients[netconsole].ready) continue;

			// Now we know this client is alive, update the frame send count.
			// Clients will only be refreshed if their updateCount is greater
			// than zero.
			sender->updateCount = UPDATECOUNT;
			
			// Add the tics into the client's ticcmd buffer, if there is room.
			// If the buffer overflows, the rest of the cmds will be forgotten.
			num = Msg_ReadByte();
			if(sender->numtics + num > BACKUPTICS) 
				num = BACKUPTICS - sender->numtics;
			start = sender->firsttic + sender->numtics;
			// Increase the counter.
			sender->numtics += num;
			// Copy as many as fits.
			for(i=start; num > 0; num--, i++)
			{
				if(i >= BACKUPTICS) i -= BACKUPTICS;
				cmd = (ticcmd_t*) &sender->ticcmds[TICCMD_IDX(i)];
				Msg_Read(cmd, TICCMD_SIZE);
				// Check the time stamp.
				time = gametic - Sv_Latency(cmd->time);//, sender->enterTime);
				if(time > sender->time) sender->time = time;
			}
			break;

		case pcl_ack_sets:
			// The client is acknowledging that it has received a number of
			// delta sets.
			while(!Msg_End())
				Sv_AckDeltaSet(netbuffer.player, Msg_ReadByte());
			break;

		case pkt_coords:
			Sv_ClientCoords(netbuffer.player);
			break;

		case pcl_ack_shake:
			// The client has acknowledged our handshake.
			// Note the time (this isn't perfectly accurate, though).
			netconsole = netbuffer.player;
			if(netconsole < 0 || netconsole >= MAXPLAYERS) continue; 
			clients[netconsole].shakePing = Sys_GetRealTime() - 
				clients[netconsole].shakePing;
			Con_Printf("Cl%i handshake ping: %i ms\n", netconsole, 
				clients[netconsole].shakePing);
			break;

		case pkt_ping:
			Net_PingResponse();
			break;

		case pcl_hello:
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
			if(netbuffer.msg.type >= pkt_game_marker)
			{
				// A client has sent a game specific packet.
				gx.HandlePacket(netbuffer.player,
					netbuffer.msg.type, netbuffer.msg.data,
					netbuffer.length);
			}
		}
	}
}

// Called by jtNet.
void Sv_PlayerArrives(int newnode)
{
	int		i;
	char	**nameList, newname[PLAYERNAMELEN];
	unsigned int nodeids[MAXPLAYERS];

	if(!netgame)
	{
		Con_Message("Sv_NetPlayerArrives called when not in netgame!\n");
		return;
	}
	if(isClient) return;

	nameList = jtNetGetStringList(JTNET_PLAYER_NAME_LIST, NULL);
	strncpy(newname, nameList[newnode], PLAYERNAMELEN-1);
	newname[PLAYERNAMELEN-1] = 0;	
	Con_Printf( "%s has arrived.\n", newname);

	N_UpdateNodes();
	
	jtNetGetPlayerIDs(nodeids);

	// We need to find the new player a client entry.
	for(i = 1; i < MAXPLAYERS; i++)
		if(!clients[i].connected)
		{
			// This'll do.
			clients[i].connected = true;
			clients[i].ready = false;
			clients[i].jtNetNode = newnode;
			clients[i].nodeID = nodeids[newnode];
			clients[i].viewConsole = i;
			clients[i].lastTransmit = -1;
			strcpy(clients[i].name, newname);
			Sv_InitPoolForClient(i);

			Con_Printf("Sv_PlayerArrives: %s designated player %i, node %i (%x)\n",
				clients[i].name, i, newnode, nodeids[newnode]);

			// In order to get in the game, the client must first
			// shake hands. It'll request this by sending a Hello packet.
			clients[i].handshake = false;
			clients[i].updateCount = UPDATECOUNT;
			break;
		}
}

// Called by jtNet.
void Sv_PlayerLeaves(jtnetplayer_t *plrdata)
{
	int i, pNumber = -1;

	if(isClient) return;

	// First let's find out the player number.
	for(i=0; i<MAXPLAYERS; i++)
		if(clients[i].nodeID == plrdata->id)
		{
			pNumber = i;
			break;
		}
	if(pNumber == -1) return; // Bogus?

	// Log off automatically.
	if(net_remoteuser == pNumber) net_remoteuser = 0;

	// Print something in the console.
	Con_Printf("Sv_PlayerLeaves: %s (player %i) has left.\n", plrdata->name, pNumber);
	players[pNumber].ingame = false;
	clients[pNumber].connected = false;
	clients[pNumber].ready = false;
	clients[pNumber].updateCount = 0;
	clients[pNumber].handshake = false;
	N_UpdateNodes();
	
	// Inform the DLL about this.
	gx.NetPlayerEvent(pNumber, DDPE_EXIT, NULL);

	// Inform other clients about this.
	Msg_Begin(psv_player_exit);
	Msg_WriteByte(pNumber);
	Net_SendBuffer(NSP_BROADCAST, SPF_RELIABLE);
}

// The player will be sent the introductory handshake packets.
void Sv_Handshake(int playernum, boolean newplayer)
{
	int						i;
	handshake_packet_t		shake;
	playerinfo_packet_t		info;

	Con_Printf("Sv_Handshake: shaking hands with player %i.\n", playernum);

	shake.version = SV_VERSION;
	shake.yourConsole = playernum;
	shake.playerMask = 0;
	shake.gameTime = gametic;
	for(i = 0; i < MAXPLAYERS; i++)
		if(clients[i].connected)
			shake.playerMask |= 1<<i;
	Net_SendPacket(playernum | DDSP_RELIABLE, psv_handshake, &shake, 
		sizeof(shake));

#if _DEBUG
	Con_Message("Sv_Handshake: plmask=%x\n", shake.playerMask);
#endif

	if(newplayer)
	{
		// Note the time when the handshake was sent.
		clients[playernum].shakePing = Sys_GetRealTime();
	}
	
	// Propagate client information.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(clients[i].connected)
		{
			info.console = i;
			strcpy(info.name, clients[i].name);
			Net_SendPacket(playernum | DDSP_RELIABLE, pkt_player_info, 
				&info, sizeof(info));
		}
		// Send the new player's info to other players.
		if(newplayer && i != 0 && i != playernum && clients[i].connected)
		{
			info.console = playernum;
			strcpy(info.name, clients[playernum].name);
			Net_SendPacket(i | DDSP_RELIABLE, pkt_player_info, 
				&info, sizeof(info));
		}
	}

	// The game DLL wants to shake hands as well?
 	gx.NetWorldEvent(DDWE_HANDSHAKE, playernum, (void*) newplayer);

	if(!newplayer)
	{
		// This is not a new player (just a re-handshake) but we'll
		// nevertheless re-init the client's state register. For new 
		// players this is done in Sv_PlayerArrives.
		Sv_InitPoolForClient(playernum);
	}

	players[playernum].flags |= DDPF_FIXANGLES | DDPF_FIXPOS;

	// This player can't be a camera.
	//players[playernum].flags &= ~DDPF_CAMERA;
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
		clients[i].numtics = 0;
		clients[i].firsttic = 0;
		clients[i].enterTime = 0;
		clients[i].runTime = -1;
		clients[i].time = 0;
		clients[i].lagStress = 0;
		clients[i].lastTransmit = -1;
		clients[i].updateCount = UPDATECOUNT;
		clients[i].fov = 90;
		clients[i].viewConsole = i;
		strcpy(clients[i].name, "");
		players[i].flags &= ~DDPF_CAMERA;
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
	Net_SendBuffer(to, SPF_RELIABLE);
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
	Net_SendBuffer(who, SPF_RELIABLE);
	//players[who].ingame = false;
}

void Sv_Ticker(void)
{
	int i;

	if(netgame)
	{
		// Update master every 3 minutes.
		if(masterAware && jtNetGetInteger(JTNET_SERVICE) == 
			JTNET_SERVICE_TCPIP && !(systics % (180*TICRATE)))
		{
			jtNetUpdateServerInfo();
		}
	}
	// Note last angles for all players.
	for(i=0; i<MAXPLAYERS; i++)
	{
		if(!players[i].ingame || !players[i].mo) continue;
		players[i].lastangle = players[i].mo->angle;
	}
	Sv_PoolTicker();
}

//========================================================================
// CCmdLogout
//========================================================================
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
	Net_SendBuffer(net_remoteuser, SPF_RELIABLE);	
	net_remoteuser = 0;
	return true;
}