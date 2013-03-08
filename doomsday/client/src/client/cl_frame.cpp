/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * cl_frame.c: Frame Reception
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"

#include "client/cl_frame.h"
#include "client/cl_mobj.h"
#include "client/cl_player.h"
#include "client/cl_sound.h"
#include "client/cl_world.h"
#include "network/net_main.h"
#include "network/net_msg.h"

// MACROS ------------------------------------------------------------------

#if 0
#define SET_HISTORY_SIZE    50
#define RESEND_HISTORY_SIZE 50
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gotFrame;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Set to true when the PSV_FIRST_FRAME2 packet is received.
// Until then, all PSV_FRAME2 packets are ignored (they must be
// from the wrong map).
boolean gotFirstFrame;

//int     predicted_tics;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// gameTime of the current frame.
static float frameGameTime = 0;

#if 0
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
#endif

// CODE --------------------------------------------------------------------

/**
 * Clear the history of received set numbers.
 */
void Cl_InitFrame(void)
{
    Cl_ResetFrame();

#if 0
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
#endif
}

/**
 * Called when the map changes.
 */
void Cl_ResetFrame(void)
{
    gotFrame = false;

    // All frames received before the PSV_FIRST_FRAME2 are ignored.
    // They must be from the wrong map.
    gotFirstFrame = false;
}

float Cl_FrameGameTime(void)
{
    return frameGameTime;
}

#if 0
/**
 * Add a set number to the history.
 */
void Cl_HistoryAdd(byte set)
{
    setHistory[historyIdx++] = set;

    if(historyIdx >= SET_HISTORY_SIZE)
        historyIdx -= SET_HISTORY_SIZE;
}

/**
 * @return              @c true, if the set is found in the recent set
 *                      history.
 */
boolean Cl_HistoryCheck(byte set)
{
    uint        i;

    for(i = 0; i < SET_HISTORY_SIZE; ++i)
    {
        if(setHistory[i] == set)
            return true;
    }
    return false;
}

/**
 * Add a resend ID number to the resend history.
 */
void Cl_ResendHistoryAdd(byte id)
{
    resendHistory[resendHistoryIdx++] = id;

    if(resendHistoryIdx >= RESEND_HISTORY_SIZE)
        resendHistoryIdx -= RESEND_HISTORY_SIZE;
}

/**
 * @return              @c true, if the resend ID is found in the history.
 */
boolean Cl_ResendHistoryCheck(byte id)
{
    uint        i;

    for(i = 0; i < RESEND_HISTORY_SIZE; ++i)
    {
        if(resendHistory[i] == id)
            return true;
    }
    return false;
}

/**
 * Converts a set identifier, which ranges from 0...255, into a logical
 * ordinal. Checks for set identifier wraparounds and updates the set
 * ordinal base accordingly.
 */
uint Cl_ConvertSetToOrdinal(byte set)
{
    uint        ordinal = 0;

    if(latestSet > 185 && set < 70)
    {
        // We must conclude that wraparound has occured.
        setOrdinalBase += 256;
#if _NETDEBUG
VERBOSE2( Con_Printf("Cl_ConvertSetToOrdinal: Wraparound, now base is %i.\n",
                     setOrdinalBase) );
#endif
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
#endif

/**
 * Read a PSV_FRAME2/PSV_FIRST_FRAME2 packet.
 */
void Cl_Frame2Received(int packetType)
{
    byte        deltaType;
    boolean     skip = false;
#ifdef _NETDEBUG
    int         deltaCount = 0;
    int         startOffset;
    int         deltaLength;
#endif

    // The first thing in the frame is the gameTime.
    frameGameTime = Reader_ReadFloat(msgReader);

    // All frames that arrive before the first frame are ignored.
    // They are most likely from the wrong map.
    if(packetType == PSV_FIRST_FRAME2)
    {
        gotFirstFrame = true;
/*#ifdef _DEBUG
        VERBOSE( Con_Printf("*** GOT THE FIRST FRAME (%i) ***\n", set) );
#endif*/
    }
    else if(!gotFirstFrame)
    {
        // Just ignore. If this was a legitimate frame, the server will
        // send it again when it notices no ack is coming.
/*#ifdef _DEBUG
        VERBOSE( Con_Printf("==> Ignored set %i\n", set) );
#endif*/
        return;
    }

    /*
#ifdef _DEBUG
    VERBOSE2( Con_Printf("Cl_Frame2Received: Processing delta set %i.\n", set) );
#endif
    */

#if 0
    if(packetType != PSV_FIRST_FRAME2)
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
#endif

    {
        //VERBOSE2( Con_Printf("Starting to process deltas in set %i.\n", set) );

        // Read and process the message.
        while(!Reader_AtEnd(msgReader))
        {
            deltaType = Reader_ReadByte(msgReader);
            skip = false;
/*
#ifdef _DEBUG
            Con_Message("Received delta %i.", deltaType);
#endif
*/
            switch(deltaType)
            {
            case DT_CREATE_MOBJ:
                // The mobj will be created/shown.
                ClMobj_ReadDelta2(skip);
                break;

            case DT_MOBJ:
                // The mobj will be hidden if it's not yet Created.
                ClMobj_ReadDelta2(skip);
                break;

            case DT_NULL_MOBJ:
                // The mobj will be removed.
                ClMobj_ReadNullDelta2(skip);
                break;

            case DT_PLAYER:
                ClPlayer_ReadDelta2(skip);
                break;

            case DT_SECTOR:
                Cl_ReadSectorDelta2(deltaType, skip);
                break;

            //case DT_SIDE_R6: // Old format.
            case DT_SIDE:
                Cl_ReadSideDelta2(deltaType, skip);
                break;

            case DT_POLY:
                Cl_ReadPolyDelta2(skip);
                break;

            case DT_SOUND:
            case DT_MOBJ_SOUND:
            case DT_SECTOR_SOUND:
            case DT_SIDE_SOUND:
            case DT_POLY_SOUND:
                Cl_ReadSoundDelta2((deltatype_t) deltaType, skip);
                break;

            default:
                Con_Error("Cl_Frame2Received: Unknown delta type %i (numtypes=%i; message size %i).\n",
                          deltaType, NUM_DELTA_TYPES, netBuffer.length);
            }
        }

#ifdef _DEBUG
        if(!gotFrame)
        {
            Con_Message("Cl_Frame2Received: First frame received.");
        }
#endif

        // We have now received a frame.
        gotFrame = true;
    }
}
