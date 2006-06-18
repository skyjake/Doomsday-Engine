/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * cl_frame.c: Frame Reception
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

#define SET_HISTORY_SIZE	50 
#define RESEND_HISTORY_SIZE	50 

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

int     predicted_tics;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Ordinal of the latest set received by the client. Used for detecting deltas
// that arrive out of order. The ordinal is the logical equivalent of the set
// identifier (which is 0...255).
static uint     latestSetOrdinal;
static byte     latestSet;

// The ordinal base is added to the set number to convert it from a set 
// identifier to an ordinal. Every time the set numbers wrap around from 255 
// to zero, the ordinal is incremented by 256.
static uint     setOrdinalBase;

// The set history keeps track of received sets and is used to detect
// duplicate frames.
static short    setHistory[SET_HISTORY_SIZE];
static int      historyIdx;

// The resend ID history keeps track of received resend deltas. Used
// to detect duplicate resends.
static byte     resendHistory[RESEND_HISTORY_SIZE];
static int      resendHistoryIdx;

// CODE --------------------------------------------------------------------

/*
 * Clear the history of received set numbers.
 */
void Cl_InitFrame(void)
{
	gotframe = false;			// Nothing yet...

	// -1 denotes an invalid entry.
	memset(setHistory, -1, sizeof(setHistory));
	historyIdx = 0;

	// Clear the resend ID history.
	memset(resendHistory, 0, sizeof(resendHistory));
	resendHistoryIdx = 0;
    
    // Reset ordinal counters.
    latestSet = 0;
    latestSetOrdinal = 0;
    setOrdinalBase = 256;
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
	setHistory[historyIdx++] = set;

	if(historyIdx >= SET_HISTORY_SIZE)
		historyIdx -= SET_HISTORY_SIZE;
}

/*
 * Returns true if the set is found in the recent set history.
 */
boolean Cl_HistoryCheck(byte set)
{
	int     i;

	for(i = 0; i < SET_HISTORY_SIZE; i++)
	{
		if(setHistory[i] == set)
			return true;
	}
	return false;
}

/*
 * Add a resend ID number to the resend history.
 */
void Cl_ResendHistoryAdd(byte id)
{
	resendHistory[resendHistoryIdx++] = id;

	if(resendHistoryIdx >= RESEND_HISTORY_SIZE)
		resendHistoryIdx -= RESEND_HISTORY_SIZE;
}

/*
 * Returns true if the resend ID is found in the history.
 */
boolean Cl_ResendHistoryCheck(byte id)
{
	int     i;

	for(i = 0; i < RESEND_HISTORY_SIZE; i++)
	{
		if(resendHistory[i] == id)
			return true;
	}
	return false;
}

/*
 * Converts a set identifier, which ranges from 0...255, into a logical ordinal.
 * Checks for set identifier wraparounds and updates the set ordinal base
 * accordingly.
 */
uint Cl_ConvertSetToOrdinal(byte set)
{
    uint ordinal = 0;
    
    if(latestSet > 185 && set < 70)
    {
        // We must conclude that wraparound has occured.
        setOrdinalBase += 256;
        
        VERBOSE2( Con_Printf("Cl_ConvertSetToOrdinal: Wraparound, now base is %i.\n",
                             setOrdinalBase) );
    }
    ordinal = set + setOrdinalBase;

    if(latestSet < 35 && set > 220)
    {
        // This is most likely a set that came in before wraparound.
        ordinal -= 256;
    }
    else
    {
        latestSet = set;
    }
    return ordinal;
}

/*
 * Read a psv_frame2/psv_first_frame2 packet.
 */
void Cl_Frame2Received(int packetType)
{
	byte    set = Msg_ReadByte(), oldSet, resend, deltaType;
	byte    resendAcks[300];
	int     i, numResendAcks = 0;
	boolean skip = false;

	// All frames that arrive before the first frame are ignored.
	// They are most likely from the wrong map.
	if(packetType == psv_first_frame2)
	{
		gotFirstFrame = true;
#ifdef _DEBUG
        VERBOSE( Con_Printf("*** GOT THE FIRST FRAME (%i) ***\n", set) );
#endif 
	}
	else if(!gotFirstFrame)
	{
		// Just ignore. If this was a legitimate frame, the server will
		// send it again when it notices no ack is coming.
#ifdef _DEBUG
		VERBOSE( Con_Printf("==> Ignored set %i\n", set) );
#endif 
		return;
	}

#ifdef _DEBUG    
    VERBOSE2( Con_Printf("Cl_Frame2Received: Processing delta set %i.\n", set) );
#endif

    if(packetType != psv_first_frame2)
    {
        // If this is not the first frame, it will be ignored if it arrives 
        // out of order.
        uint ordinal = Cl_ConvertSetToOrdinal(set);
        
        if(ordinal < latestSetOrdinal)
        {
            VERBOSE2( Con_Printf("==> Ignored set %i because it arrived out of order.\n",
                                set) );
            return;
        }
        latestSetOrdinal = ordinal;

        VERBOSE2( Con_Printf("Latest set ordinal is %i.\n", latestSetOrdinal) );
    }
    
	// Check for duplicates (might happen if ack doesn't get through
	// or ack arrives too late).
	if(!Cl_HistoryCheck(set))
	{
		// It isn't yet in the history, so add it there.
		Cl_HistoryAdd(set);

        VERBOSE2( Con_Printf("Starting to process deltas in set %i.\n", set) );

		// Read and process the message.
		while(!Msg_End())
		{
			deltaType = Msg_ReadByte();
			skip = false;
            
            VERBOSE2( Con_Printf("Received delta %i.\n", deltaType & ~DT_RESENT) );

			// Is this a resent delta?
			if(deltaType & DT_RESENT)
			{
                VERBOSE2( Con_Printf("  This is a resent delta.\n") );
                
				deltaType &= ~DT_RESENT;

				// Read the set number this was originally in.
				oldSet = Msg_ReadByte();

				// Read the resend ID.
				resend = Msg_ReadByte();

				// Did we already receive this delta?
                {
                    boolean historyChecked = Cl_HistoryCheck(oldSet);
                    boolean resendChecked = Cl_ResendHistoryCheck(resend);
                    
                    if(historyChecked || resendChecked)
                    {
                        // Yes, we've already got this. Must skip.
                        skip = true;

                        VERBOSE2( Con_Printf("  Skipping delta %i (oldset=%i, rsid=%i), because history=%i resend=%i\n",
                                             deltaType, oldSet, resend, 
                                             historyChecked, resendChecked) );
                    }
                }

				// We must acknowledge that we've received this.
				resendAcks[numResendAcks++] = resend;
				Cl_ResendHistoryAdd(resend);
			}
            else
            {
                VERBOSE2( Con_Printf("  Not resent.\n") );
            }

			switch (deltaType)
			{
			case DT_CREATE_MOBJ:
				// The mobj will be created/shown.
				Cl_ReadMobjDelta2(true, skip);
				break;

			case DT_MOBJ:
				// The mobj will be hidden if it's not yet Created.
				Cl_ReadMobjDelta2(false, skip);
				break;

			case DT_NULL_MOBJ:
				// The mobj will be removed.
				Cl_ReadNullMobjDelta2(skip);
				break;

			case DT_PLAYER:
				Cl_ReadPlayerDelta2(skip);
				break;

			case DT_SECTOR_SHORT_FLAGS: // Old format.
            case DT_SECTOR:
				Cl_ReadSectorDelta2(deltaType, skip);
				break;

			case DT_SIDE:
				Cl_ReadSideDelta2(skip);
				break;

			case DT_POLY:
				Cl_ReadPolyDelta2(skip);
				break;

			case DT_SOUND:
			case DT_MOBJ_SOUND:
			case DT_SECTOR_SOUND:
			case DT_POLY_SOUND:
				Cl_ReadSoundDelta2(deltaType, skip);
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

	if(numResendAcks == 0)
	{
		// Acknowledge the set.
		Msg_Begin(pcl_ack_sets);
		Msg_WriteByte(set);
        
#ifdef _DEBUG
        VERBOSE2( Con_Printf("Cl_Frame2Received: Ack set %i. "
                             "Nothing was resent.\n", set) );
#endif
	}
	else
	{
		// Acknowledge the set and the resent deltas.
		Msg_Begin(pcl_acks);
		Msg_WriteByte(set);
#ifdef _DEBUG
        VERBOSE2( Con_Printf("Cl_Frame2Received: Ack set %i. "
                             "Contained %i resent deltas: \n", 
                             set, numResendAcks) );
#endif
		for(i = 0; i < numResendAcks; i++)
		{
			Msg_WriteByte(resendAcks[i]);

#ifdef _DEBUG
            VERBOSE2( Con_Printf("%i ", resendAcks[i]) );
#endif
		}
#ifdef _DEBUG
        VERBOSE2( Con_Printf("\n") );
#endif
	}
	Net_SendBuffer(0, 0);
}
