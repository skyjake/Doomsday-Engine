
//**************************************************************************
//**
//** CL_FRAME.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

#define BIT(x)		(1 << (x))
#define MAX_ACKS	32

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gotframe;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int predicted_tics;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int num_acks;
byte acks[MAX_ACKS];

// CODE --------------------------------------------------------------------

//==========================================================================
// Cl_ReadDeltaSet
//	Read and ack a delta set.
//==========================================================================
void Cl_ReadDeltaSet(void)
{
	byte present = Msg_ReadByte();
	byte set = Msg_ReadByte();
		
	// Add the set number to list of acks.
	if(num_acks < MAX_ACKS) acks[num_acks++] = set;

	if(present & BIT( DT_MOBJ )) while(Cl_ReadMobjDelta());
	if(present & BIT( DT_PLAYER )) while(Cl_ReadPlayerDelta());
	if(present & BIT( DT_LUMP )) while(Cl_ReadLumpDelta());
	if(present & BIT( DT_SECTOR )) while(Cl_ReadSectorDelta());
	if(present & BIT( DT_SIDE )) while(Cl_ReadSideDelta());
	if(present & BIT( DT_POLY )) while(Cl_ReadPolyDelta());
}

//==========================================================================
// Cl_FrameReceived
//	Reads a frame packet from the message buffer and applies the deltas
//	in it. Also acks the sets in the packet immediately.
//==========================================================================
void Cl_FrameReceived(void)
{
#if 0
	byte count = Msg_ReadByte();

	//Cl_HandleFrame();
	/*memcpy(&latest_frame_packet, &netbuffer.msg, 
		latest_frame_size = netbuffer.length + 
		netbuffer.headerLength);*/
	//CON_Printf("---Frame on gt=%i, Rt=%i, len=%i\n", gametic, I_GetTime(), latest_frame_size);
					
	if(count) 
	{
		// Spawn the missiles.
		gx.NetWorldEvent(DDWE_PROJECTILE, count, &netbuffer.cursor);
	}

	// Store rest of the message.
	latest_frame_size = netbuffer.length - Msg_Offset() 
		+ netbuffer.headerLength;		
	latest_frame_packet.type = netbuffer.msg.type;
	memcpy(latest_frame_packet.data, netbuffer.cursor, 
		latest_frame_size - netbuffer.headerLength);
#endif

	int i, frametime;

	gotframe = true;

#if _DEBUG
	if(!game_ready) Con_Message("Got frame but GAME NOT READY!\n");
#endif

	// Frame time, lowest byte of gametic.
	frametime = Msg_ReadByte();	

	num_acks = 0;
	while(!Msg_End()) Cl_ReadDeltaSet();

	// Acknowledge all sets.
	Msg_Begin(pcl_ack_sets);
	for(i = 0; i < num_acks; i++) Msg_WriteByte(acks[i]);
	Net_SendBuffer(0, 0);

	// Reset the predict counter.
	predicted_tics = 0;
}
