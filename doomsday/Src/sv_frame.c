
//**************************************************************************
//**
//** SV_FRAME.C
//**
//**************************************************************************

#define DD_PROFILE

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_system.h"
#include "de_refresh.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
	PROF_GEN_DELTAS,
	PROF_WRITE_DELTAS,
	PROF_PACKET_SIZE
END_PROF_TIMERS()

#define FIXED8_8(x)				(((x) & 0xffff00) >> 8)
#define MAX_MOBJ_LEN			23
#define MAX_PLAYER_LEN			20

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Sv_RefreshClient(int playernum);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean net_timerefresh;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int allow_frames = false;
int send_all_players = false;
int frame_interval = 1; // Skip every second frame by default (17.5fps)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

// Sends all the relevant information to each client.
void Sv_TransmitFrame(void)
{
	int		i, ctime, num_ingame, pcnt;

	// Obviously clients don't transmit anything.
	if(!allow_frames || isClient) return;			

	// If the server is fast, we can get here several times 
	// per tic. We don't want that.
	/*if(gametic <= last_transmit + frame_interval) return;
	last_transmit = gametic;*/

	for(i = 0, num_ingame = 0; i < MAXPLAYERS; i++)
		if(players[i].ingame) num_ingame++;

	for(i = 0, pcnt = 0; i < MAXPLAYERS; i++)
	{
		// Local players are skipped if not recording a demo.
		if(!players[i].ingame
			|| players[i].flags & DDPF_LOCAL && !clients[i].recording) 
			continue;
		
		// Time to send an update? ("interlaced" updates)
		pcnt++;
		ctime = gametic;
		if(frame_interval > 0 && num_ingame > 1)
			ctime += (pcnt * frame_interval) / num_ingame;
		if(ctime <= clients[i].lastTransmit + frame_interval) continue;
		clients[i].lastTransmit = ctime;
/*#if _DEBUG
		ST_Message("gt:%i (%i) -> cl%i\n", gametic, ctime, i);
#endif*/

		if(clients[i].ready && clients[i].updateCount > 0)
		{
			// Don't allow packets to pile up.
			if(N_CheckSendQueue(i)) 
			{
				Sv_RefreshClient(i);
				if(players[i].flags & DDPF_LOCAL)
				{
					// All the necessary data is always sent to local 
					// players.
					players[i].flags &= ~DDPF_FIXPOS;
				}
			}
		}
	}
}

//==========================================================================
// Sv_RefreshClient
//	Send all necessary data to the client (a frame packet).
//==========================================================================
void Sv_RefreshClient(int plrNum)
{
	ddplayer_t	*player = &players[plrNum];
	client_t	*cl = &clients[plrNum];
	int			valid = ++validcount;

	int			refresh_started_at = Sys_GetRealTime();

	if(!player->mo) 
	{
		// Interesting... we don't know where the client is.
		return;		
	}

	BEGIN_PROF( PROF_GEN_DELTAS );

	// The first thing we must do is generate a Delta Set for the client. 
	Sv_DoFrameDelta(plrNum);

	END_PROF( PROF_GEN_DELTAS );

	// There, now we know what has changed. Let's create the Frame packet.
	Msg_Begin(psv_frame);

	// Frame time, lowest byte of gametic.
	Msg_WriteByte(gametic);	

	BEGIN_PROF( PROF_WRITE_DELTAS );

	// Delta Sets.
	Sv_WriteFrameDelta(plrNum);

	END_PROF( PROF_WRITE_DELTAS );

	profiler_[PROF_PACKET_SIZE].startCount++;
	profiler_[PROF_PACKET_SIZE].totalTime += Msg_Offset();
	
	// Send the frame packet as high priority.
	Net_SendBuffer(plrNum, /*SPF_FRAME | */0xe000);

	// Server acks local deltas right away.
	if(players[plrNum].flags & DDPF_LOCAL) 
		Sv_AckDeltaSetLocal(plrNum);

	if(net_timerefresh)
	{
		Con_Printf("refresh %i: %i ms (len=%i b)\n", plrNum, 
			Sys_GetRealTime()-refresh_started_at,
			netbuffer.length);
	}
}

/*
 * Shutdown routine for the server.
 */
void Sv_Shutdown(void)
{
	PRINT_PROF( PROF_GEN_DELTAS );
	PRINT_PROF( PROF_WRITE_DELTAS );
	PRINT_PROF( PROF_PACKET_SIZE );

	Sv_ShutdownPools();
}