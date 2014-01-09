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
    }
    else if(!gotFirstFrame)
    {
        // Just ignore. If this was a legitimate frame, the server will
        // send it again when it notices no ack is coming.
        return;
    }

    {
        // Read and process the message.
        while(!Reader_AtEnd(msgReader))
        {
            deltaType = Reader_ReadByte(msgReader);
            skip = false;

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
                LOG_NET_ERROR("Received unknown delta type %i (message size: %i bytes)")
                        << deltaType << netBuffer.length;
                return;
            }
        }

        if(!gotFrame)
        {
            LOGDEV_NET_NOTE("First frame received");
        }

        // We have now received a frame.
        gotFrame = true;
    }
}
