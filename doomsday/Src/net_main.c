//**************************************************************************
//**
//** NET_MAIN.C
//** 
//** Client/server networking. 
//** Player number zero is always the server.
//** In single-player games there is only the server present.
//**
//** Once upon a time based on Hexen's peer-to-peer network code.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h> // for atoi()

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

#define OBSOLETE		CVF_NO_ARCHIVE|CVF_HIDE	// Old ccmds.

// The threshold is the average ack time * mul.
#define ACK_THRESHOLD_MUL		3

// Never wait a too short time for acks.
#define ACK_MINIMUM_THRESHOLD	50

// Clients don't send commands on every tic.
#define CLIENT_TICCMD_INTERVAL	3

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void ST_NetProgress(void);
extern void ST_NetDone(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int		net_skewdampen;		// In frames.
extern boolean	net_showskew;
//extern int		latest_frame_size;
//extern netdata_t latest_frame_packet;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte			*localticcmds;
int				numlocal;		// Number of cmds in the buffer.

ddplayer_t		players[MAXPLAYERS];
client_t		clients[MAXPLAYERS];// All network data for the players.

int				netgame;			// true if a netgame is in progress
int				isServer;			// true if this computer is an open server.
int				isClient;			// true if this computer is a client	
int				consoleplayer;
int				displayplayer;

int             gametic;
int				lasttime, availabletics, realtics;

// Gotframe is true if a frame packet has been received.
int				gotframe = false;

boolean			firstNetUpdate = true;

boolean			monitorSendQueue = false;
//boolean			net_timerefresh = false;
boolean			net_showlatencies = false;
//int				net_stressmargin = 10;
//int				net_dampentime = 7;
boolean			net_dev = false;
int				net_dontsleep = false;
int				net_ticsync = true;
float			net_connecttime;
int				net_coordtime = 10;
float			net_connecttimeout = 10;

// Local packets are stored into this buffer.
boolean		reboundpacket;
netbuffer_t	reboundstore;

cvar_t netCVars[] =
{
	"net_MSQ",			OBSOLETE,			CVT_BYTE,	&monitorSendQueue,	0, 1,	"Monitor send queue.",
//	"net_TimeRefresh",	OBSOLETE,			CVT_BYTE,	&net_timerefresh,	0, 1,	"Time client refresh.",
	"net_Latencies",	OBSOLETE,			CVT_BYTE,	&net_showlatencies,	0, 1,	"Show client latencies.",
//	"net_StressDampen",	OBSOLETE|CVF_NO_MAX, CVT_INT,	&net_dampentime,	0, 0,	"CmdLag stress dampening interval, in tics.",
//	"net_StressMargin",	OBSOLETE|CVF_NO_MAX, CVT_INT,	&net_stressmargin,	0, 0,	"CmdLag stress margin, in tics.",
//	"net_SkewDampen",	OBSOLETE|CVF_NO_MAX, CVT_INT,	&net_skewdampen,	0, 0,	"Client time skew dampening, in frames.",
//	"net_ShowSkew",		OBSOLETE,			CVT_BYTE,	&net_showskew,		0, 1,	"Show client time skew.",
	"net_Dev",			OBSOLETE,			CVT_BYTE,	&net_dev,			0, 1,	"Network development mode.",
	"net_DontSleep",	OBSOLETE,			CVT_BYTE,	&net_dontsleep,		0, 1,	"1=Don't sleep while waiting for tics.",
	"net_FrameInterval", OBSOLETE|CVF_NO_MAX, CVT_INT,	&frame_interval,	0, 0,	"Minimum number of tics between sent frames.",
//	"net_ResendTime",	OBSOLETE|CVF_NO_MAX,	CVT_INT,	&net_resendtime,	0, 0,	"Number of tics to wait before resending delta sets.",
	"net_Password",		OBSOLETE,			CVT_CHARPTR, &net_password,		0, 0,	"Password for remote login.",
//	"net_ShowSets",		OBSOLETE,			CVT_BYTE,	&net_showsets,		0, 1,	"1=Show what goes in delta sets.",
//	"net_MaxDif",		OBSOLETE,			CVT_INT,	&net_maxdif,		0, 256,	"Maximum difference between client pos vs. real pos.",
//	"net_MinSecUpdate",	OBSOLETE|CVF_NO_MAX,	CVT_INT,	&net_minsecupd,		0, 0,	"Block range for forced sector deltas.",
//	"net_FullSecUpdate", OBSOLETE|CVF_NO_MAX, CVT_INT,	&net_fullsecupd,	0, 0,	"Block range for full (w/reject) sector deltas.",
//	"net_MaxSecUpdate", OBSOLETE|CVF_NO_MAX, CVT_INT,	&net_maxsecupd,		0, 0,	"Maximum block range for sector deltas.",
//--------------------------------------------------------------------------
// Some of these are obsolete...
//--------------------------------------------------------------------------
	"net-queue-show",		0,			CVT_BYTE,	&monitorSendQueue,	0, 1,	"Monitor send queue.",
//	"net-stress-dampen",	CVF_NO_MAX, CVT_INT,	&net_dampentime,	0, 0,	"CmdLag stress dampening interval, in tics.",
//	"net-stress-margin",	CVF_NO_MAX, CVT_INT,	&net_stressmargin,	0, 0,	"CmdLag stress margin, in tics.",
	"net-dev",				0,			CVT_BYTE,	&net_dev,			0, 1,	"Network development mode.",
	"net-nosleep",			0,			CVT_BYTE,	&net_dontsleep,		0, 1,	"1=Don't sleep while waiting for tics.",
//	"client-skew-dampen",	CVF_NO_MAX,	CVT_INT,	&net_skewdampen,	0, 0,	"Client time skew dampening, in frames.",
//	"client-skew-show",		0,			CVT_BYTE,	&net_showskew,		0, 1,	"Show client time skew.",
	"client-pos-interval",	CVF_NO_MAX,	CVT_INT,	&net_coordtime,		0, 0,	"Number of tics between client coord packets.",
	"client-connect-timeout", CVF_NO_MAX, CVT_FLOAT, &net_connecttimeout, 0, 0, "Maximum number of seconds to attempt connecting to a server.",
	"server-password",		0,			CVT_CHARPTR, &net_password,		0, 0,	"Password for remote login.",
//	"server-delta-show",	0,			CVT_BYTE,	&net_showsets,		0, 1,	"1=Show what goes in delta sets.",
//	"server-refresh-show",	0,			CVT_BYTE,	&net_timerefresh,	0, 1,	"Time client refresh.",
	"server-latencies",		0,			CVT_BYTE,	&net_showlatencies,	0, 1,	"Show client latencies.",
	"server-frame-interval", CVF_NO_MAX, CVT_INT,	&frame_interval,	0, 0,	"Minimum number of tics between sent frames.",
//	"server-frame-resend",	CVF_NO_MAX,	CVT_INT,	&net_resendtime,	0, 0,	"Number of tics to wait before resending delta sets.",
//	"server-dif-max",		0,			CVT_INT,	&net_maxdif,		0, 256,	"Maximum difference between client pos vs. real pos.",
//	"server-sector-min",	CVF_NO_MAX,	CVT_INT,	&net_minsecupd,		0, 0,	"Block range for forced sector deltas.",
//	"server-sector-full",	CVF_NO_MAX,	CVT_INT,	&net_fullsecupd,	0, 0,	"Block range for full (w/reject) sector deltas.",
//	"server-sector-max",	CVF_NO_MAX,	CVT_INT,	&net_maxsecupd,		0, 0,	"Maximum block range for sector deltas.",
	"server-player-limit",	0,			CVT_INT,	&sv_maxPlayers,		0, MAXPLAYERS, "Maximum number of players on the server.",
	NULL
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int coordTimer = 0;

// CODE --------------------------------------------------------------------

//===========================================================================
// Net_Init
//===========================================================================
void Net_Init(void)
{
	Net_AllocArrays();
	memset(&netbuffer, 0, sizeof(netbuffer));
	netbuffer.headerLength = netbuffer.msg.data - (byte*) &netbuffer.msg;
	// The game is always started in single-player mode.
	netgame = false;
}

//===========================================================================
// Net_Shutdown
//===========================================================================
void Net_Shutdown(void)
{
	netgame = false;
	N_Shutdown();
	Net_DestroyArrays();
}

//===========================================================================
// Net_GetPlayerName
//	Returns the name of the specified player.
//===========================================================================
char *Net_GetPlayerName(int player)
{
	return clients[player].name;
}

//===========================================================================
// Net_GetPlayerID
//===========================================================================
id_t Net_GetPlayerID(int player)
{
	if(!clients[player].connected) return 0;
	return clients[player].id;
}

//===========================================================================
// Net_SendBuffer
//	Sends the contents of the netbuffer.
//===========================================================================
void Net_SendBuffer(int to_player, int sp_flags)
{
	// Don't send anything during demo playback.
	if(playback) return;

	// Update the length of the message.
	netbuffer.length = netbuffer.cursor - netbuffer.msg.data;
	netbuffer.player = to_player;

	// A rebound packet?
	if(sp_flags & SPF_REBOUND)
	{
		reboundstore = netbuffer;
		reboundpacket = true;
		return;
	}

	Demo_WritePacket(to_player);

	// Can we send the packet?
	if(sp_flags & SPF_DONT_SEND) return;

	// Send the packet to the network.
	N_SendPacket(sp_flags);
}

// Returns false if there are no packets waiting.
boolean Net_GetPacket(void)
{
	if(reboundpacket) // Local packets rebound.
	{
		netbuffer = reboundstore;
		netbuffer.player = consoleplayer;
		netbuffer.cursor = netbuffer.msg.data;
		reboundpacket = false;
		return true;
	}
	if(playback)
	{
		// We're playing a demo. This overrides all other packets.
		return Demo_ReadPacket();
	}
	if(!netgame)
	{
		// Packets cannot be received.
		return false;
	}
	if(!N_GetPacket()) return false;

	// Are we recording a demo?
	if(isClient && clients[consoleplayer].recording)
		Demo_WritePacket(consoleplayer);

	// Reset the cursor for Msg_* routines.
	netbuffer.cursor = netbuffer.msg.data;

	return true;
}

/*boolean Net_GetLatestFramePacket(void)
{
	if(!isClient || !latest_frame_size) return false;
	// Place the latest frame packet to the netbuffer.
	memcpy(&netbuffer.msg, &latest_frame_packet, latest_frame_size);
	netbuffer.cursor = netbuffer.msg.data;
	netbuffer.length = latest_frame_size - netbuffer.headerLength;
	// Clear the buffer.
	latest_frame_size = 0;
	return true;
}*/

//===========================================================================
// Net_SendPacket
//	This is the public interface of the message sender.
//===========================================================================
void Net_SendPacket(int to_player, int type, void *data, int length)
{
	int flags = 0;
	
	// What kind of delivery to use?
	if(to_player & DDSP_CONFIRM) flags |= SPF_CONFIRM;
	if(to_player & DDSP_ORDERED) flags |= SPF_ORDERED;

	Msg_Begin(type);
	if(data) Msg_Write(data, length);

	if(isClient)
	{
		// As a client we can only send messages to the server.
		Net_SendBuffer(0, flags);
	}
	else
	{
		// The server can send packets to any player.
		// Only allow sending to the sixteen possible players.
		Net_SendBuffer(to_player & DDSP_ALL_PLAYERS? NSP_BROADCAST 
			: (to_player & 0xf), flags); 
	}
}

//===========================================================================
// Net_ShowChatMessage
//	Prints the message in the console.
//===========================================================================
void Net_ShowChatMessage()
{
	// The current packet in the netbuffer is a chat message,
	// let's unwrap and show it.
	Con_FPrintf(CBLF_GREEN, "%s: %s\n", 
		clients[netbuffer.msg.data[0]].name, 
		netbuffer.msg.data + 3);
}

// All arguments are sent out as a chat message.
int CCmdChat(int argc, char **argv)
{
	char buffer[100];
	int	i, mode = !stricmp(argv[0], "chat") || !stricmp(argv[0], "say")? 0 
		: !stricmp(argv[0], "chatNum") || !stricmp(argv[0], "sayNum")? 1 : 2;
	unsigned short mask = 0;

	if(argc == 1)
	{
		Con_Printf("Usage: %s %s(text)\n", argv[0],
			!mode? "" : mode==1? "(plr#) " : "(name) ");
		Con_Printf("Chat messages are max. 80 characters long.\n");
		Con_Printf("Use quotes to get around arg processing.\n");
		return true;
	}

	// Chatting is only possible when connected.
	if(!netgame) return false;

	// Too few arguments?
	if(mode && argc < 3) return false;

	// Assemble the chat message.
	strcpy(buffer, argv[!mode? 1 : 2]);
	for(i = (!mode? 2 : 3); i < argc; i++)
	{
		strcat(buffer, " ");
		strncat(buffer, argv[i], 80 - (strlen(buffer)+strlen(argv[i])+1));
	}
	buffer[80] = 0;

	// Send the message.
	switch(mode)
	{
	case 0: // chat
		mask = ~0;
		break;

	case 1: // chatNum
		mask = 1 << atoi(argv[1]);
		break;

	case 2: // chatTo
		for(i = 0; i < MAXPLAYERS; i++)
			if(!stricmp(clients[i].name, argv[1]))
			{
				mask = 1 << i;
				break;
			}
	}
	Msg_Begin(pkt_chat);
	Msg_WriteByte(consoleplayer);
	Msg_WriteShort(mask);
	Msg_Write(buffer, strlen(buffer) + 1);

	if(!isClient)
	{
		if(mask == (unsigned short) ~0) 
		{
			Net_SendBuffer(NSP_BROADCAST, SPF_ORDERED);
		}
		else 
		{
			for(i = 1; i < MAXPLAYERS; i++) 
				if(players[i].ingame && mask & (1 << i)) 
					Net_SendBuffer(i, SPF_ORDERED);
		}
	}
	else
	{
		Net_SendBuffer(0, SPF_ORDERED);
	}
	
	// Show the message locally.
	Net_ShowChatMessage();

	// Inform the game, too.
	gx.NetPlayerEvent(consoleplayer, DDPE_CHAT_MESSAGE, buffer);
	return true;
}

// Returns a pointer to a new local ticcmd so it can be built.
// Returns NULL if the buffer is full.
ticcmd_t *Net_LocalCmd()
{
	// Check that the localtics buffer still has room.
	if(numlocal == LOCALTICS) return NULL;
	// One more command. The counter is cleared when the cmds are sent.
	return (ticcmd_t*) &localticcmds[TICCMD_IDX(numlocal++)]; 
}

/*
 * After a long period with no updates (map setup), calling this will
 * reset the tictimer so that no time seems to have passed.
 */
void Net_ResetTimer(void)
{
	lasttime = Sys_GetTime();
}

/*
 * Build ticcmds for console player, sends out a packet.
 */
void Net_Update(void)
{
	static int ticSendTimer = 0;
	int nowtime;
	int newtics;
	int	i;

	// Check time.
	nowtime = Sys_GetTime();

	// Synchronize tics with game time, not the clock?
	if(!net_ticsync) nowtime = gametic;

	if(firstNetUpdate)
	{
		firstNetUpdate = false;
		lasttime = nowtime;
	}
	newtics = nowtime - lasttime;
	if(newtics <= 0) goto listen;	// Nothing new to update.

	lasttime = nowtime;

	// Realtics counts the actual time that has passed.
	realtics += newtics;

	// Build new ticcmds for console player.
	for(i = 0; i < newtics; i++)
	{
		DD_StartTic();
		DD_ProcessEvents();	

		/*Con_Printf("mktic:%i gt:%i newtics:%i >> %i\n", 
				maketic, gametic, newtics, maketic-gametic);*/

		if(playback)
		{
			numlocal = 0;
			if(availabletics < LOCALTICS) availabletics++;
		}
		else if(!isDedicated && !ui_active)
		{
			// Place the new ticcmd in the local ticcmds buffer.
			ticcmd_t *cmd = Net_LocalCmd();
			if(cmd)
			{
				gx.BuildTicCmd(cmd);
				// Set the time stamp. Only the lowest byte is stored.
				cmd->time = gametic + availabletics;
				// Availabletics counts the tics that have cmds.
				availabletics++;
				if(isClient)
				{
					// When not playing a demo, this is the last command.
					// It is used in local movement prediction.
					memcpy(clients[consoleplayer].lastcmd, cmd, sizeof(*cmd));
				}
			}
		}
	}

	// This is as far as dedicated servers go.
	if(isDedicated) goto listen;

	if(!isClient || ++ticSendTimer > CLIENT_TICCMD_INTERVAL)
	{
		// Clients don't send commands on every tic.
		ticSendTimer = 0;

		// Send the proper number of tics to the server.
		// If we are the server, the tics will rebound back to us.
		Msg_Begin(pkt_ticcmd);
		Msg_WriteByte(numlocal);
		// Copy the tics to the outgoing packet.
		for(i = 0; i < numlocal; i++)
		{
			Msg_Write(localticcmds + TICCMD_IDX(i), TICCMD_SIZE);
		}
		// Send the packet to the server, i.e. player zero.
		Net_SendBuffer(0, isClient? 0 : SPF_REBOUND);
		// The buffer is cleared.
		numlocal = 0;
	}

	// Clients will periodically send their coordinates to the server so
	// any prediction errors can be fixed. Client movement is almost 
	// entirely local.
	if(isClient && allow_frames 
		&& coordTimer-- < 0
		&& players[consoleplayer].mo)
	{
		mobj_t *mo = players[consoleplayer].mo;
		coordTimer = net_coordtime; // 35/2
		Msg_Begin(pkt_coords);
		Msg_WriteShort(mo->x >> 16);
		Msg_WriteShort(mo->y >> 16);
		Msg_WriteShort(mo->z >> 16);
		Net_SendBuffer(0, 0);
	}

listen:
	// Listen for packets. Call the correct packet handler.
	if(isClient) 
		Cl_GetPackets();
	else // Single-player or server.
		Sv_GetPackets();
}

//===========================================================================
// Net_AllocArrays
//	Called from Net_Init to initialize the ticcmd arrays.
//===========================================================================
void Net_AllocArrays(void)
{
	int		i;

	// Local ticcmds are stored into this array before they're copied
	// to netplayer[0]'s ticcmds buffer.
	localticcmds = calloc(LOCALTICS, TICCMD_SIZE);
	numlocal = 0;	// Nothing in the buffer.

	for(i = 0; i < MAXPLAYERS; i++)
	{
		memset(clients+i, 0, sizeof(clients[i]));
		// The server stores ticcmds sent by the clients to these
		// buffers.
		clients[i].ticcmds = calloc(BACKUPTICS, TICCMD_SIZE);
		// The last cmd that was executed is stored here.
		clients[i].lastcmd = calloc(1, TICCMD_SIZE);
		clients[i].runTime = -1;
	}
}

void Net_DestroyArrays(void)
{
	int i;

	free(localticcmds);
	for(i=0; i<MAXPLAYERS; i++)
	{
		free(clients[i].ticcmds);
		free(clients[i].lastcmd);
	}
}

//
// This is the network one-time initialization.
// (into single-player mode)
//
void Net_InitGame (void)
{
	Cl_InitID();

	// In single-player mode there is only player number zero.
	consoleplayer = displayplayer = 0; 

	// We're in server mode if we aren't a client.
	isServer = true;
	
	// Netgame is true when we're aware of the network (i.e. other players).
	netgame = false;
	
	players[0].ingame = true;
	players[0].flags |= DDPF_LOCAL;
	clients[0].id = clientID;
	clients[0].ready = true;
	clients[0].connected = true;
	clients[0].viewConsole = 0;
	clients[0].lastTransmit = -1;

	// Are we timing a demo here?
	if(ArgCheck("-timedemo")) net_ticsync = false;
}

//===========================================================================
// Net_StopGame
//===========================================================================
void Net_StopGame(void)
{
	int		i;

	if(isServer)
	{
		// We are an open server. This means we should inform all the 
		// connected clients that the server is about to close.
		Msg_Begin(psv_server_close);
		Net_SendBuffer(NSP_BROADCAST, SPF_CONFIRM);
	}
	else
	{
		// Must stop recording, we're disconnecting.
		Demo_StopRecording(consoleplayer);
		Cl_CleanUp();
		isClient = false;
	}

	// Netgame has ended.
	netgame = false;
	isServer = true;
	
	// No more remote users.
	net_remoteuser = 0;
	net_loggedin = false;

	// All remote players are forgotten.
	for(i = 0; i < MAXPLAYERS; i++) 
	{
		players[i].ingame = false;
		clients[i].ready = clients[i].connected = false;
		players[i].flags &= ~(DDPF_CAMERA | DDPF_LOCAL);
	}

	// We're about to become player zero, so update it's view angles to
	// match our current ones.
	if(players[0].mo)
	{
		players[0].mo->angle = players[consoleplayer].clAngle;
		players[0].lookdir = players[consoleplayer].clLookDir;
	}
	consoleplayer = displayplayer = 0;
	players[0].ingame = true;
	clients[0].ready = true;	
	clients[0].connected = true;
	clients[0].viewConsole = 0;
	players[0].flags |= DDPF_LOCAL;
}

// Returns delta based on 'now' (- future, + past).
int Net_TimeDelta(byte now, byte then)
{
	int delta;

	if(now >= then) 
	{
		// Simple case.
		delta = now - then;	
	}
	else
	{
		// There's a wraparound.
		delta = 256 - then + now;
	}
	// The time can be in the future. We'll allow one second.
	if(delta > 220) delta -= 256;
	return delta;
}

//===========================================================================
// Net_GetTicCmd
//	This is a bit complicated and quite possibly unnecessarily so. The 
//	idea is, however, that because the ticcmds sent by clients arrive in 
//	bursts, we'll preserve the motion by 'executing' the commands in the
//	same order in which they were generated. If the client's connection
//	lags a lot, the difference between the serverside and clientside 
//	positions will be *large*, especially when the client is running.
//	If too many commands are buffered, the client's coord announcements
//	will be processed before the actual movement commands, resulting in 
//	serverside warping (which is perceived by all other clients).
//===========================================================================
int Net_GetTicCmd(void *pCmd, int player)
{
	client_t *client = &clients[player];
	ticcmd_t *cmd = pCmd;
/*	int doMerge = false;
	int future;*/

	if(client->numtics <= 0) 
	{
		// No more commands for this player.
		return false;
	}

	// Return the next ticcmd from the buffer.
	// There will be one less tic in the buffer after this.
	client->numtics--;
	memcpy(cmd, &client->ticcmds[ TICCMD_IDX(client->firsttic++) ], 
		TICCMD_SIZE);

	// This is the new last command.
	memcpy(client->lastcmd, cmd, TICCMD_SIZE);

	// Make sure the firsttic index is in range.
	client->firsttic %= BACKUPTICS;
	return true;

#if 0
	//Con_Printf("GetTicCmd: Cl=%i, GT=%i (%i)...\n", player, gametic, (byte)gametic);

	// Check the lag stress.
/*	if(cl->lagStress > net_stressmargin)
	{
		// Command lag should be increased, we're running out of cmds.
		cl->lagStress -= net_stressmargin;
		memcpy(cmd, cl->lastcmd, TICCMD_SIZE);	// Repeat lastcmd.
		//Con_Printf("  Increasing lag\n");
		return true;
	}
	else if(cl->lagStress < -net_stressmargin)
	{
		// Command lag should be decreased.
		cl->lagStress += net_stressmargin;
		cl->runTime++;
		//Con_Printf("  Decreasing lag\n");
	}*/

	for(;;)
	{
		if(Net_TimeDelta(gametic, cl->runTime) <= 0)
		{
			//Con_Printf("  Running in the future! Rt=%i\n", cl->runTime);
			future = true;
			//cl->lagStress++;
		}
		else 
		{
			future = false;
		}
		// Are there any commands left for this player?
		if(!cl->numtics) 
		{
			cl->lagStress++;
			// Reuse the last command.
			memcpy(cmd, cl->lastcmd, TICCMD_SIZE);
			// Runtime advances.
			if(!future) cl->runTime++;
			//Con_Printf("  Out of Cmds! Rt set to %i\n", cl->runTime);
			return false; 
		}
		// There's at least one command in the buffer.
		// There will be one less tic in the buffer after this.
		cl->numtics--;
		memcpy(cmd, &cl->ticcmds[TICCMD_IDX(cl->firsttic++)], TICCMD_SIZE);
		// Make sure the firsttic iterator is in range.
		if(cl->firsttic >= BACKUPTICS) cl->firsttic = 0;
		// Do we need to do any merging?
		if(doMerge && gx.DiscardTicCmd) gx.DiscardTicCmd(cl->lastcmd, cmd);
		// This is the new last command.
		memcpy(cl->lastcmd, cmd, TICCMD_SIZE);
		// Check that the command is valid (not if its tick has already passed).
		if(Net_TimeDelta(cl->runTime, cmd->time) >= 0)
		{
			//Con_Printf("  Obsolete Cmd! Rt=%i Ct=%i\n", cl->runTime, cmd->time);
			// Cmd is too old. Go to next command and merge this one with it.
			doMerge = true;
			continue;
		}		
		cl->runTime = cmd->time;
		// Note command lag.
		cl->lag = Sv_Latency(cmd->time);
		// The local player doesn't need lag stress, because we know it's
		// always running exactly synced with the server, i.e. itself.
		if(player && cl->numtics > 1) cl->lagStress--;
		//Con_Printf("  CmdLag=%i\n", cl->lag);
		break;
	}
	return true;
#endif
}

//===========================================================================
// Net_Drawer
//	Does drawing for the engine's HUD, not just the net.
//===========================================================================
void Net_Drawer(void)
{
	char buf[160], tmp[40];
	int i, c;
	boolean show_blink_r = false;

	for(i = 0; i < MAXPLAYERS; i++)
		if(players[i].ingame && clients[i].recording) 
			show_blink_r = true;

	if(!net_dev && !show_blink_r && !consoleShowFPS) return;

	// Go into screen projection mode.
	gl.MatrixMode(DGL_PROJECTION);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);
	
	if(show_blink_r && gametic & 8)
	{
		strcpy(buf, "[");
		for(i = c = 0; i < MAXPLAYERS; i++)
		{
			if(!players[i].ingame || !clients[i].recording) continue;
			if(c++) strcat(buf, ",");
			sprintf(tmp, "%i:%s", i, clients[i].recordpaused? "-P-" : "REC");
			strcat(buf, tmp);
		}
		strcat(buf, "]");
		i = screenWidth - FR_TextWidth(buf);
		gl.Color3f(0, 0, 0);
		FR_TextOut(buf, i - 8, 12);
		gl.Color3f(1, 1, 1);
		FR_TextOut(buf, i - 10, 10);
	}
	if(net_dev)
	{
/*		gl.Color3f(1, 1, 1);
		sprintf(buf, "G%i", gametic);
		FR_TextOut(buf, 10, 10);
		for(i = 0, cl = clients; i<MAXPLAYERS; i++, cl++)
			if(players[i].ingame)
			{
				sprintf(buf, "%02i:%+04i[%+03i](%02d/%03i) pf:%x", i, cl->lag,
					cl->lagStress, cl->numtics, cl->runTime, 
					players[i].flags);
				FR_TextOut(buf, 10, 10+10*(i+1));
			}*/
	}
	if(consoleShowFPS)
	{
		int x, y = 30, w = 70, h;
		sprintf(buf, "%i FPS", DD_GetFrameRate());
		h = FR_TextHeight(buf) + 16;
		x = screenWidth - w - 10;
		UI_DrawRectEx(x, y, w, h, 4, 
			UI_COL(UIC_BRD_HI), UI_COL(UIC_BRD_MED), UI_COL(UIC_BRD_LOW), .5f);
		UI_Gradient(x + 4, y + 4, w - 8, h - 8, 
			UI_COL(UIC_BG_MEDIUM), UI_COL(UIC_BG_LIGHT), .5f, .5f);
		UI_Color(UI_COL(UIC_TEXT));
		UI_TextOutEx(buf, x + 8, y + h/2, false, true, UI_COL(UIC_TEXT), 1);
	}
	
	// Restore original matrix.
	gl.MatrixMode(DGL_PROJECTION);
	gl.PopMatrix();
}

/*
 * Maintain the ack threshold average.
 */
void Net_SetAckTime(int clientNumber, uint period)
{
	client_t *client = &clients[clientNumber];

	// Add the new time into the array.
	client->ackTimes[ client->ackIdx++ ] = period;
	client->ackIdx %= NUM_ACK_TIMES;
}

/*
 * Returns the average ack time of the client.
 */
uint Net_GetAckTime(int clientNumber)
{
	client_t *client = &clients[clientNumber];
	uint average = 0;
	int i;

	// Calculate the average.
	for(i = 0; i < NUM_ACK_TIMES; i++)
	{
		average += client->ackTimes[i];
	}
	return average / NUM_ACK_TIMES;
}

/*
 * Sets all the ack times. Used to initial the ack times for new clients.
 */
void Net_SetInitialAckTime(int clientNumber, uint period)
{
	int i;

	for(i = 0; i < NUM_ACK_TIMES; i++)
	{
		clients[clientNumber].ackTimes[i] = period;
	}
}

/*
 * The ack threshold is the maximum period of time to wait before 
 * deciding an ack is not coming. The minimum threshold is 100 ms.
 */
uint Net_GetAckThreshold(int clientNumber)
{
	uint threshold = Net_GetAckTime(clientNumber) * ACK_THRESHOLD_MUL;

	if(threshold < ACK_MINIMUM_THRESHOLD)
	{
		threshold = ACK_MINIMUM_THRESHOLD;
	}
	return threshold;
}

int Sv_GetRegisteredMobj(pool_t*, thid_t, mobjdelta_t*);

//===========================================================================
// Net_Ticker
//===========================================================================
void Net_Ticker(void)
{
	int		i;
	client_t *cl;
	static int monitorCount = 0;
	static int dampenCount = 0;

	N_Ticker();

#if 0
	//----DEBUG----
	{
		int a, b;
		for(a = 0, i = 1; i < thingnodes.count; i++)
			if(thingnodes.nodes[i].ptr) a++;
		for(b = 0, i = 1; i < linenodes.count; i++)
			if(linenodes.nodes[i].ptr) b++;
		ST_Message("thingnodes:%-5i<%-5i>(%-5i) linenodes:%-5i<%-5i>(%-5i)\n", 
			thingnodes.count, a, thingnodes.pos,
			linenodes.count, b, linenodes.pos);
	}
	//----DEBUG----
#endif

#if 0
	if(net_dev)
	{
		static int whereCount = 0;
		int k;
		mobjdelta_t mod;
		if(whereCount++ > 35)
		{
			whereCount = 0;
			for(i = 0; i < MAXPLAYERS; i++)
			{
				if(!players[i].ingame) continue;
				for(k = 0; k < MAXPLAYERS; k++)
				{
					if(!players[k].ingame || i == k
						 || !players[k].mo) continue;
					// Get this player in the register.
					if(Sv_GetRegisteredMobj(pools + i, 
						players[k].mo->thinker.id, &mod))
					{
						ST_Message("Pool of %i: pl%i at (%i,%i)\n",
							i, k, mod.data.x >> 16, mod.data.y >> 16);
					}
				}
			}
		}
	}
#endif

	if(net_dev)
	{
		static int printTimer = 0;
		if(printTimer++ > TICSPERSEC)
		{
			printTimer = 0;
			for(i = 0; i < MAXPLAYERS; i++)
			{
				if(Sv_IsFrameTarget(i))
				{
					Con_Message("%i(rdy%i): avg=%05ims thres=%05ims bwr=%05i "
						"maxfs=%05ib\n",
						i, clients[i].ready, Net_GetAckTime(i), 
						Net_GetAckThreshold(i),
						clients[i].bandwidthRating, 
						Sv_GetMaxFrameSize(i));
				}
			}
		}
	}

/*	if(dampenCount-- <= 0)
	{
		dampenCount = net_dampentime;
		// Dampen lag stress.
		for(i = 0; i < MAXPLAYERS; i++)
		{
			if(!players[i].ingame) continue;
			if(clients[i].lagStress > 0)
				clients[i].lagStress--;
			else if(clients[i].lagStress < 0)
				clients[i].lagStress++;
		}
	}*/

	/*if(net_showlatencies)
	{
		Con_Printf("G%i ", gametic);
		for(i=0, cl=clients; i<MAXPLAYERS; i++, cl++)
			if(players[i].ingame)
				Con_Printf("%02i:%+04i[%+03i](%02d/%03i) ", i, gametic - cl->time,
					cl->lagStress, cl->numtics, cl->runTime);
		Con_Printf("\n");
	}*/

	// The following stuff is only for netgames.
	if(!netgame) return;

	/*if(monitorSendQueue && monitorCount-- <= 0)
	{
		int num, bytes;
		monitorCount = MONITORTICS;
		num = jtNetCheckQueue(clients[!consoleplayer].jtNetNode, &bytes);
		Con_Printf("Queue to %i: %i packets, %i bytes.\n",
			!consoleplayer, num, bytes);
	}*/

	// Check the pingers.
	for(i = 0, cl = clients; i < MAXPLAYERS; i++, cl++)
	{
		// Clients can only ping the server.
		if(isClient && i || i == consoleplayer) continue;
		if(cl->ping.sent)
		{
			// The pinger is active.
			if(Sys_GetRealTime() - cl->ping.sent > PING_TIMEOUT) // Timed out?
			{
				cl->ping.times[cl->ping.current] = -1;
				Net_SendPing(i, 0);
			}
		}
	}
}

//===========================================================================
// CCmdKick
//===========================================================================
int CCmdKick(int argc, char **argv)
{
	int num;

	if(argc != 2)
	{
		Con_Printf("Usage: %s (num)\n", argv[0]);
		Con_Printf("Server can use this command to kick clients out of the game.\n");
		return true;
	}
	if(!netgame) 
	{
		Con_Printf("This is not a netgame.\n");
		return false;
	}
	if(!isServer)
	{
		Con_Printf("This command is for the server only.\n");
		return false;
	}
	num = atoi(argv[1]);
	if(num < 1 || num >= MAXPLAYERS)
	{
		Con_Printf("Invalid client number.\n");
		return false;
	}
	if(net_remoteuser == num)
	{
		Con_Printf("Can't kick the client who's logged in.\n");
		return false;
	}
	Sv_Kick(num);
	return true;
}

//===========================================================================
// CCmdSetName
//===========================================================================
int CCmdSetName(int argc, char **argv)
{
	playerinfo_packet_t info;

	if(argc != 2)
	{
		Con_Printf("Usage: %s (name)\n", argv[0]);
		Con_Printf("Use quotes to include spaces in the name.\n");
		return true;
	}
	Con_SetString("net-name", argv[1]);
	if(!netgame) return true;

	// In netgames, a notification is sent to other players.
	memset(&info, 0, sizeof(info));
	info.console = consoleplayer;
	strncpy(info.name, argv[1], PLAYERNAMELEN-1);	

	// Serverplayers can update their name right away.
	if(!isClient) strcpy(clients[0].name, info.name);

	Net_SendPacket(DDSP_CONFIRM | (isClient? 0 : DDSP_ALL_PLAYERS),
		pkt_player_info, &info, sizeof(info));			
	return true;
}

//===========================================================================
// CCmdSetTicks
//===========================================================================
int CCmdSetTicks(int argc, char **argv)
{
	extern double lastSharpFrameTime;

	if(argc != 2)
	{
		Con_Printf("Usage: %s (tics)\n", argv[0]);
		Con_Printf("Sets the number of game tics per second.\n");
		return true;
	}
	firstNetUpdate = true;
	Sys_TicksPerSecond(strtod(argv[1], 0));
	lastSharpFrameTime = Sys_GetTimef();
	return true;
}

//===========================================================================
// CCmdMakeCamera
//===========================================================================
int CCmdMakeCamera(int argc, char **argv)
{
/*	int cp;
	mobj_t *mo;
	ddplayer_t *conp = players + consoleplayer;

	if(argc < 2) return true;
	cp = atoi(argv[1]);
	clients[cp].connected = true;
	clients[cp].ready = true;
	clients[cp].updateCount = UPDATECOUNT;
	players[cp].flags |= DDPF_CAMERA;
	players[cp].ingame = true; // !!!
	Sv_InitPoolForClient(cp);
	mo = Z_Malloc(sizeof(mobj_t), PU_LEVEL, 0);
	memset(mo, 0, sizeof(*mo));
	mo->x = conp->mo->x;
	mo->y = conp->mo->y;
	mo->z = conp->mo->z;
	mo->subsector = conp->mo->subsector;
	players[cp].mo = mo;
	displayplayer = cp;*/

	// Create a new local player.
	int cp;
//	ddplayer_t *conp = players + consoleplayer;

	if(argc < 2) return true;
	cp = atoi(argv[1]);
	clients[cp].connected = true;
	clients[cp].ready = true;
	clients[cp].updateCount = UPDATECOUNT;
	players[cp].flags |= DDPF_LOCAL;
	Sv_InitPoolForClient(cp);

	return true;
}

//===========================================================================
// CCmdSetConsole
//===========================================================================
int CCmdSetConsole(int argc, char **argv)
{
	int cp;

	if(argc < 2) return false;
	cp = atoi(argv[1]);
	if(players[cp].ingame)
		consoleplayer = displayplayer = cp;
	return true;
}

//===========================================================================
// CCmdConnect
//	Intelligently connect to a server. Just provide an IP address and the
//	rest is automatic.
//===========================================================================
int CCmdConnect(int argc, char **argv)
{
	int port = 0, returnValue = false;
	double startTime;
	char buf[100], *ptr;
	boolean needCloseStartup = false;
	serverinfo_t info;

	if(argc < 2 || argc > 3)
	{
		Con_Printf("Usage: %s (ip-address) [port]\n", argv[0]);
		Con_Printf("A TCP/IP connection is created to the given server.\n");
		Con_Printf("If a port is not specified port zero will be used.\n");
		return true;
	}
	if(isDedicated)
	{
		Con_Printf("Not allowed.\n");
		return false;
	}
	if(netgame)
	{
		Con_Printf("Already connected.\n");
		return false;
	}

	strcpy(buf, argv[1]);
	// If there is a port specified in the address, use it.
	if((ptr = strrchr(buf, ':')))
	{
		port = strtol(ptr + 1, 0, 0);
		*ptr = 0;
	}
	if(argc == 3) port = strtol(argv[2], 0, 0);
	Con_SetString("net-ip-address", buf);
	/*Con_SetInteger("net-ip-port", port);*/

	// If not already there, go to startup screen mode.
	if(!startupScreen)
	{
		needCloseStartup = true;
		Con_StartupInit();
	}
	// This won't print anything, but will draw the startup screen.
	Con_Message("");

	// Make sure TCP/IP is active.
	if(!N_InitService(NSP_TCPIP, false))
	{
		Con_Message("TCP/IP not available.\n");
		goto endConnect;
	}

	Con_Message("Connecting to %s...\n", buf);

	// Start searching at the specified location.
	N_LookForHosts();
	
	for(startTime = Sys_GetSeconds(); 
		Sys_GetSeconds() - startTime < net_connecttimeout; 
		Sys_Sleep(250))
	{
		if(!N_GetHostInfo(0, &info)) continue;

		// Found something! 
		N_PrintServerInfo(0, NULL);
		N_PrintServerInfo(0, &info);
		Con_Execute("net connect 0", false);

		returnValue = true;
		goto endConnect;
	}
	Con_Printf("No response from %s.\n", buf);

endConnect:
	if(needCloseStartup) Con_StartupDone();
	return returnValue;
}