/**\file cdaudio.cpp
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Compact Disc-Digital Audio (CD-DA) / "Redbook".
 *
 * Uses the Windows API MCI interface.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <stdio.h>

#include "dswinmm.h"

// MACROS ------------------------------------------------------------------

#define DEVICEID                "mycd"

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int cdInited = false;

// Currently playing track info:
static int cdCurrentTrack = 0;
static boolean cdLooping;
static double cdStartTime, cdPauseTime, cdTrackLength;

// CODE --------------------------------------------------------------------

/**
 * Execute an MCI command string.
 *
 * @return              @c true, if successful.
 */
static int sendMCICmd(char* returnInfo, int returnLength,
                      const char *format, ...)
{
    char                buf[300];
    va_list             args;
    MCIERROR            error;

    va_start(args, format);
    dd_vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if((error = mciSendStringA(buf, returnInfo, returnLength, NULL)))
    {
        mciGetErrorStringA(error, buf, 300);
        Con_Message("DM_WinCD: %s\n", buf);

        return false;
    }

    return true;
}

/**
 * @return              Length of the track in seconds.
 */
static int getTrackLength(int track)
{
    char                lenString[80];
    int                 min, sec;

    if(!sendMCICmd(lenString, 80, "status " DEVICEID " length track %i",
                   track))
        return 0;

    sscanf(lenString, "%i:%i", &min, &sec);
    return min * 60 + sec;
}

static int isPlaying(void)
{
    char                lenString[80];

    if(!sendMCICmd(lenString, 80, "status " DEVICEID " mode wait"))
        return false;

    if(strcmp(lenString, "playing") == 0)
        return true;

    return false;
}

/**
 * Assign the value of a CDAudio-interface property.
 */
void DM_CDAudio_Set(int prop, float value)
{
    if(!cdInited)
        return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        {
        int                 val = MINMAX_OF(0, (byte) (value * 255 + .5f), 255);

        // Straighten the volume curve.
        val <<= 8; // Make it a word.
        val = (int) (255.9980469 * sqrt(value));
        mixer4i(MIX_CDAUDIO, MIX_SET, MIX_VOLUME, val);
        break;
        }

    default:
        break;
    }
}

/**
 * Retrieve the value of a CDAudio-interface property.
 */
int DM_CDAudio_Get(int prop, void* ptr)
{
    if(!cdInited)
        return false;

    switch(prop)
    {
    case MUSIP_ID:
        if(ptr)
        {
            strcpy((char*) ptr, "WinMM::CD");
            return true;
        }
        break;

    case MUSIP_PLAYING:
        return (cdInited && isPlaying()? true : false);

    default:
        break;
    }

    return false;
}

/**
 * Initialize the CDAudio-interface.
 */
int DM_CDAudio_Init(void)
{
    if(cdInited)
        return true;

    if(!sendMCICmd(0, 0, "open cdaudio alias " DEVICEID))
        return false;

    if(!sendMCICmd(0, 0, "set " DEVICEID " time format tmsf"))
        return false;

    cdCurrentTrack = 0;
    cdLooping = false;
    cdStartTime = cdPauseTime = cdTrackLength = 0;

    // Successful initialization.
    return cdInited = true;
}

/**
 * Shutdown the CDAudio-interface, we do nothing whilst offline.
 */
void DM_CDAudio_Shutdown(void)
{
    if(!cdInited)
        return;

    DM_CDAudio_Stop();
    sendMCICmd(0, 0, "close " DEVICEID);

    cdInited = false;
}

/**
 * Do any necessary update tasks. Called every frame by the engine.
 */
void DM_CDAudio_Update(void)
{
    if(!cdInited)
        return;

    // Check for looping.
    if(cdCurrentTrack && cdLooping &&
       Sys_GetSeconds() - cdStartTime > cdTrackLength)
    {
        // Restart the track.
        DM_CDAudio_Play(cdCurrentTrack, true);
    }
}

/**
 * Begin playback of a specifc audio track, possibly looped.
 */
int DM_CDAudio_Play(int track, int looped)
{
    int len;

    if(!cdInited)
        return false;

    // Get the length of the track.
    cdTrackLength = len = getTrackLength(track);
    if(!len)
        return false; // Hmm?!

    // Play it!
    if(!sendMCICmd(0, 0, "play " DEVICEID " from %i to %i", track,
                   MCI_MAKE_TMSF(track, 0, len, 0)))
        return false;

    // Success!
    cdLooping = (looped? true:false);
    cdStartTime = Sys_GetSeconds();
    return cdCurrentTrack = track;
}

/**
 * Pauses playback of the currently playing audio track.
 */
void DM_CDAudio_Pause(int pause)
{
    if(!cdInited)
        return;

    sendMCICmd(0, 0, "%s " DEVICEID, pause ? "pause" : "play");
    if(pause)
        cdPauseTime = Sys_GetSeconds();
    else
        cdStartTime += Sys_GetSeconds() - cdPauseTime;
}

/**
 * Stops playback of the currently playing audio track.
 */
void DM_CDAudio_Stop(void)
{
    if(!cdInited || !cdCurrentTrack)
        return;

    cdCurrentTrack = 0;
    sendMCICmd(0, 0, "stop " DEVICEID);
}
