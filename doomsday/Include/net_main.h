#ifndef __DOOMSDAY_NETWORK_H__
#define __DOOMSDAY_NETWORK_H__

#include <stdio.h>
#include <lzss.h>
#include "dd_share.h"
#include "sys_network.h"
#include "net_msg.h"
#include "p_data.h"
#include "con_decl.h"

#define SV_VERSION			6
#define BIT(x)				(1 << (x))

#define NSP_BROADCAST		-1			// For Net_SendBuffer.

// Flags for console text from the server.
// Change with server version?
#define SV_CONSOLE_FLAGS	(CBLF_WHITE|CBLF_LIGHT|CBLF_GREEN)

#define NETBUFFER_MAXDATA	32768 //16384 //9215

#define PING_TIMEOUT		1000	// Ping timeout (ms).
#define MAX_PINGS			10

#define MONITORTICS			7

#define LOCALTICS			10		// Built ticcmds are stored here.
#define	BACKUPTICS			70		// Two seconds worth of tics.

// The number of mobjs that can be stored in the input/visible buffer.
// The server won't send more mobjs than this.
#define	MAX_CLMOBJS		80	

// Packet types. 
// pkt = sent by anybody
// psv = sent by server
// pcl = sent by client
enum
{
	// Messages and responses.
	pcl_hello			= 0,
	pkt_ok,			
	pkt_cancel,
	pkt_player_info,
	pkt_chat,
	pkt_ticcmd,
	pkt_ping,
	psv_handshake,
	psv_server_close,
	psv_frame,
	psv_player_exit,
	psv_console_text,
	pcl_ack_shake,
	psv_sync,
	psv_filter,
	pkt_command,
	pkt_login,
	pcl_ack_sets,			
	pkt_coords,
	pkt_democam,
	pkt_democam_resume,

	// World events.
	//psv_sector_update	= DDPT_SECTOR_UPDATE,		// 32
	//psv_wall_update		= DDPT_WALL_UPDATE,			// 33
	psv_plane_sound		= DDPT_PLANE_SOUND,			// 32
	
	// Game specific events.
	pkt_game_marker		= DDPT_FIRST_GAME_EVENT		// 64
};

// Use the number defined in dd_share.h for sound packets. 
// This is for backwards compatibility.
#define psv_sound			DDPT_SOUND

#define RESENDCOUNT			10
#define HANDSHAKECOUNT		17
#define UPDATECOUNT			5

// These dd-flags are packed (i.e. included in mobj deltas).
#define DDMF_PACK_MASK		0x3ceff1ff 

// Size of the client coordinate buffer, used by server to calculate 
// coord errors.
#define NUM_CERR			4

// The consoleplayer's camera position is written to the demo file
// every 3rd tic.
#define	LOCALCAM_WRITE_TICS	3

//---------------------------------------------------------------------------
// Types
//---------------------------------------------------------------------------

typedef struct
{
	// High tics when ping was sent (0 if pinger not used).
	int		sent;			

	// A record of the pings (negative time: no response).
	float	times[MAX_PINGS];	

	// Total number of pings and the current one.
	int		total;
	int		current;
} 
pinger_t;

// Network information for a player. Corresponds the players array.
typedef struct
{
	// ID number. Each client has a unique ID number.
	id_t	id;

	// Ticcmd buffer. The server uses this when clients send it ticcmds.
	byte	*ticcmds;		

	// Number of tics in the buffer.
	int		numtics;	

	// Index of the first tic in the buffer.
	int		firsttic;

	// Last command executed, reused if a new one isn't found.
	ticcmd_t *lastcmd;	

	int		lastTransmit;

	// If >0, the server will send the next world frame to the client.
	// This is set when input is received from the client. 
	int		updateCount;

	// Gametic when the client entered the game.
	int		enterTime;

	// Client time. Updated as cmds are received from the client. 
	int		time;

	// Client-reported time of the last processed ticcmd.
	// Older or as old tics than this are discarded.
	byte	runTime;

	int		lag;
	int		lagStress;

	// Clients use this to determine how long ago they received the
	// last update of this client.
	int		age;

	// Is this client connected? (Might not be in the game yet.) Only
	// used by the server.
	int		connected;

	// Clients are pinged by the server when they join the game.
	// This is the ping in milliseconds for this client. For the server.
	unsigned int shakePing;

	// If true, the server will send the player a handshake. The client must
	// acknowledge it before this flag is turned off.
	int		handshake;

	// Server uses this to determine whether it's OK to send game packets
	// to the client.
	int		ready;

	// The name of the player.
	char	name[PLAYERNAMELEN];

	// Field of view. Used in determining visible mobjs (default: 90).
	float	fov;

	// jtNet node for this player.
	unsigned int nodeID;	// jtNet node ID.
	int		jtNetNode;		// The corresponding jtNet node number (-1=none).

	// Ping tracker for this client.
	pinger_t ping;

	// Demo recording file (being recorded if not NULL).
	LZFILE	*demo;
	boolean	recording;
	boolean recordpaused;

	// View console. Which player this client is viewing?
	int		viewConsole;

	int		errorPos;
	vertex_t error[NUM_CERR];
} 
client_t;

typedef struct
{
	byte	type;
	byte	data[NETBUFFER_MAXDATA];
} netdata_t;

typedef struct
{
	int		player;		// Recipient or sender.
	int		length;		// Number of bytes in the data buffer.
	int		headerLength; // 1 byte at the moment.

	byte	*cursor;	// Points into the data buffer.

	// The data buffer for sending and receiving packets.
	netdata_t msg;

} netbuffer_t;

// Packets.
typedef struct
{
	byte				version;
	unsigned short		playerMask;		// 16 players.
	byte				yourConsole;	// Which one's you?
	int					gameTime;
} handshake_packet_t;

typedef struct
{
	byte	console;
	char	name[PLAYERNAMELEN];
} playerinfo_packet_t;


//---------------------------------------------------------------------------
// Variables
//---------------------------------------------------------------------------
extern boolean firstNetUpdate;
extern netbuffer_t netbuffer;
extern int resend_start;			// set when server needs our tics
extern int resend_count;
extern int gametime, oldentertics;
extern int num_clmobjs;
extern boolean masterAware;
extern int netgame;
extern int consoleplayer;
extern int displayplayer;
extern int gametic, realtics, availabletics;
extern int isServer, isClient;
extern boolean allow_net_traffic;	// Should net traffic be allowed?
extern int net_dontsleep, net_ticsync;
extern ddplayer_t players[MAXPLAYERS];
extern client_t clients[MAXPLAYERS];


//---------------------------------------------------------------------------
// Functions
//---------------------------------------------------------------------------
void Net_Init(void);
void Net_Shutdown(void);
void Net_AllocArrays(void);
void Net_DestroyArrays(void);
void Net_SendPacket(int to_player, int type, void *data, int length);
boolean Net_GetPacket(void);
void Net_SendBuffer(int to_player, int sp_flags);
void Net_InitGame (void);
void Net_StartGame (void);
void Net_StopGame (void);
void Net_SendPing(int player, int count);
void Net_PingResponse(void);
void Net_ShowPingSummary(int player);
void Net_ShowChatMessage();
int Net_TimeDelta(byte now, byte then);
int Net_GetTicCmd(void *cmd, int player);
void Net_Update(void);
void Net_Ticker(void);
void Net_Drawer(void);

// Console commands.
D_CMD( Kick );
D_CMD( SetName );
D_CMD( SetTicks );
D_CMD( MakeCamera );
D_CMD( SetConsole );
D_CMD( Connect );

#endif