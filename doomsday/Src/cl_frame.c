
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

#define BIT(x)				(1 << (x))
#define MAX_ACKS			40
#define SET_HISTORY_SIZE	100

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gotframe;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Set to true when the psv_first_frame2 packet is received.
// Until then, all psv_frame2 packets are ignored (they must be
// from the wrong map).
boolean gotFirstFrame;

int predicted_tics;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int num_acks;
byte acks[MAX_ACKS];

// The set history keeps track of received sets and is used to detect
// duplicate frames.
short setHistory[SET_HISTORY_SIZE];
int historyIdx;

// CODE --------------------------------------------------------------------

/*
 * Clear the history of received set numbers.
 */
void Cl_InitFrame(void)
{
	gotframe = false;	// Nothing yet...

	// -1 denotes an invalid entry.
	memset(setHistory, -1, sizeof(setHistory));
	historyIdx = 0;
}

/*
 * Called when the map changes.
 */
void Cl_ResetFrame(void)
{
	gotframe = false;

	// All frames received before the psv_first_frame2 are ignored.
	// They must be from the wrong map.
	gotFirstFrame = false;
}

/*
 * Add a set number to the history.
 */
void Cl_HistoryAdd(byte set)
{
	setHistory[ historyIdx++ ] = set;
	
	if(historyIdx >= SET_HISTORY_SIZE)
		historyIdx -= SET_HISTORY_SIZE;
}

/*
 * Returns true if the set is found in the recent set history.
 */
boolean Cl_HistoryCheck(byte set)
{
	int i;

	for(i = 0; i < SET_HISTORY_SIZE; i++)
	{
		if(setHistory[i] == set) return true;
	}
	return false;
}

/*
 * Read a psv_frame2/psv_first_frame2 packet.
 */
void Cl_Frame2Received(int packetType)
{
	byte set = Msg_ReadByte(), deltaType;

	// All frames that arrive before the first frame are ignored.
	// They are most likely from the wrong map.
	if(packetType == psv_first_frame2)
	{
		gotFirstFrame = true;
/*#ifdef _DEBUG
		Con_Printf("*** GOT THE FIRST FRAME (%i) ***\n", set);
#endif*/
	}
	else if(!gotFirstFrame)
	{
		// Just ignore. If this was a legitimate frame, the server will
		// send it again when it notices no ack is coming.
/*#ifdef _DEBUG
		Con_Printf("==> Ignored set%i\n", set);
#endif*/
		return;
	}

	// Check for duplicates (might happen if ack doesn't get through
	// or ack arrives too late).
	if(!Cl_HistoryCheck(set))
	{
		// It isn't yet in the history, so add it there.
		Cl_HistoryAdd(set);

		// Read and process the message.
		while(!Msg_End())
		{
			switch(deltaType = Msg_ReadByte())
			{
			case DT_CREATE_MOBJ:
				// The mobj will be created/shown.
				Cl_ReadMobjDelta2(true);
				break;

			case DT_MOBJ:
				// The mobj will be hidden if it's not yet Created.
				Cl_ReadMobjDelta2(false);
				break;

			case DT_NULL_MOBJ:
				// The mobj will be removed.
				Cl_ReadNullMobjDelta2();
				break;

			case DT_PLAYER:
				Cl_ReadPlayerDelta2();
				break;

			case DT_SECTOR:
				Cl_ReadSectorDelta2();
				break;

			case DT_SIDE:
				Cl_ReadSideDelta2();
				break;

			case DT_POLY:
				Cl_ReadPolyDelta2();
				break;

			case DT_SOUND:
			case DT_MOBJ_SOUND:
			case DT_SECTOR_SOUND:
			case DT_POLY_SOUND:
				Cl_ReadSoundDelta2(deltaType);
				break;

			default:
				Con_Error("Cl_Frame2Received: Unknown delta type %i.\n", 
					deltaType);
			}
		}

		// We have now received a frame.
		gotframe = true;	

		// Reset the predict counter.
		predicted_tics = 0;
	}

	// Acknowledge the set. (Is a separate packet really necessary?)
	Msg_Begin(pcl_ack_sets);
	Msg_WriteByte(set);
	Net_SendBuffer(0, 0);
}

/*
 * Read and ack a psv_frame delta set.
 */
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

/*
 * Reads a psv_frame packet from the message buffer and applies the deltas
 * in it. Also acks the sets in the packet immediately.
 *
 * THIS FUNCTION IS OBSOLETE (psv_frame is no longer used)
 */
void Cl_FrameReceived(void)
{
	int i, frametime;

	gotframe = true;
	gotFirstFrame = true;

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
