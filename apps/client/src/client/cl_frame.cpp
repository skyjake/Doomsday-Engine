/** @file cl_frame.cpp  Frame Reception.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "client/cl_def.h"
#include "client/cl_frame.h"
#include "client/cl_mobj.h"
#include "client/cl_player.h"
#include "client/cl_sound.h"
#include "client/cl_world.h"
#include "network/net_main.h"
#include "network/net_buf.h"
#include "network/net_msg.h"

#if 0
#define SET_HISTORY_SIZE    50
#define RESEND_HISTORY_SIZE 50
#endif

// Set to true when the PSV_FIRST_FRAME2 packet is received.
// Until then, all PSV_FRAME2 packets are ignored (they must be
// from the wrong map).
dd_bool gotFirstFrame;

//int     predicted_tics;

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
void Cl_ResetFrame()
{
    netState.gotFrame = false;

    // All frames received before the PSV_FIRST_FRAME2 are ignored.
    // They must be from the wrong map.
    gotFirstFrame = false;
}

float Cl_FrameGameTime()
{
    return frameGameTime;
}

void Cl_Frame2Received(int packetType)
{
    // The first thing in the frame is the gameTime.
    frameGameTime = Reader_ReadFloat(msgReader);

    // All frames that arrive before the first frame are ignored.
    // They are most likely from the wrong map.
    if (packetType == PSV_FIRST_FRAME2)
    {
        gotFirstFrame = true;
    }
    else if (!gotFirstFrame)
    {
        // Just ignore. If this was a legitimate frame, the server will
        // send it again when it notices no ack is coming.
        return;
    }

    // Read and process the message.
    while (!Reader_AtEnd(msgReader))
    {
        const byte deltaType = Reader_ReadByte(msgReader);

        switch (deltaType)
        {
        case DT_CREATE_MOBJ:
            // The mobj will be created/shown.
            ClMobj_ReadDelta();
            break;

        case DT_MOBJ:
            // The mobj will be hidden if it's not yet Created.
            ClMobj_ReadDelta();
            break;

        case DT_NULL_MOBJ:
            // The mobj will be removed.
            ClMobj_ReadNullDelta();
            break;

        case DT_PLAYER:
            ClPlayer_ReadDelta();
            break;

        case DT_SECTOR:
            Cl_ReadSectorDelta(deltaType);
            break;

        //case DT_SIDE_R6: // Old format.
        case DT_SIDE:
            Cl_ReadSideDelta(deltaType);
            break;

        case DT_POLY:
            Cl_ReadPolyDelta();
            break;

        case DT_SOUND:
        case DT_MOBJ_SOUND:
        case DT_SECTOR_SOUND:
        case DT_SIDE_SOUND:
        case DT_POLY_SOUND:
            Cl_ReadSoundDelta((deltatype_t) deltaType);
            break;

        default:
            LOG_NET_ERROR("Received unknown delta type %i (message size: %i bytes)")
                    << deltaType << netBuffer.length;
            return;
        }
    }

    if (!netState.gotFrame)
    {
        LOGDEV_NET_NOTE("First frame received");
    }

    // We have now received a frame.
    netState.gotFrame = true;
}
