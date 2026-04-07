/** @file cdaudio.cpp  Compact Disc-Digital Audio (CD-DA) / "Redbook" playback.
 *
 * Uses the Windows API MCI interface.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "dswinmm.h"
#include <de/c_wrapper.h>
#include <de/legacy/timer.h>
#include <cmath>
#include <cstdio>

#define DEVICEID "mycd"

static int cdInited = false;

// Currently playing track info:
static int cdCurrentTrack = 0;
static dd_bool cdLooping;
static double cdStartTime, cdPauseTime, cdTrackLength;

/**
 * Execute an MCI command string.
 *
 * Returns @c true iff successful.
 */
static int sendMCICmd(char *returnInfo, int returnLength, char const *format, ...)
{
    char buf[300];
    va_list args;

    va_start(args, format);
    dd_vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (MCIERROR error = mciSendStringA(buf, returnInfo, returnLength, NULL))    
    {
        if (error == MCIERR_HARDWARE)
        {
            // Hardware is not cooperating.
            App_Log(DE2_DEV_AUDIO_VERBOSE, "[WinMM] CD playback hardware is not ready");
            return false;
        }
        mciGetErrorStringA(error, buf, 300);
        App_Log(DE2_AUDIO_ERROR, "[WinMM] CD playback error: %s", buf);
        return false;
    }

    return true;
}

/**
 * @return  Length of the track in seconds.
 */
static int getTrackLength(int track)
{
    char lenString[80];

    if(!sendMCICmd(lenString, 80, "status " DEVICEID " length track %i",
                   track))
        return 0;

    int minutes, seconds;
    sscanf(lenString, "%i:%i", &minutes, &seconds);
    return minutes * 60 + seconds;
}

static int isPlaying()
{
    char lenString[80];

    if(!sendMCICmd(lenString, 80, "status " DEVICEID " mode wait"))
        return false;

    if(strcmp(lenString, "playing") == 0)
        return true;

    return false;
}

void DM_CDAudio_Set(int prop, float value)
{
    if(!cdInited) return;

    switch(prop)
    {
    case MUSIP_VOLUME: {
        // Straighten the volume curve.
        int val = MINMAX_OF(0, int(value * 255 + .5f), 255);
        val <<= 8; // Make it a word.
        val = (int) (255.9980469 * sqrt(value));
        mixer4i(MIX_CDAUDIO, MIX_SET, MIX_VOLUME, val);
        break; }

    default: break;
    }
}

int DM_CDAudio_Get(int prop, void *ptr)
{
    if(!cdInited) return false;

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

    default: break;
    }

    return false;
}

int DM_CDAudio_Init()
{
    if(cdInited) return true;

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

void DM_CDAudio_Shutdown()
{
    if(!cdInited) return;

    DM_CDAudio_Stop();
    sendMCICmd(0, 0, "close " DEVICEID);

    cdInited = false;
}

void DM_CDAudio_Update()
{
    if(!cdInited) return;

    // Check for looping.
    if(cdCurrentTrack && cdLooping &&
       Timer_Seconds() - cdStartTime > cdTrackLength)
    {
        // Restart the track.
        DM_CDAudio_Play(cdCurrentTrack, true);
    }
}

int DM_CDAudio_Play(int track, int looped)
{
    if(!cdInited) return false;

    // Get the length of the track.
    int len = getTrackLength(track);
    cdTrackLength = len;
    if(!len) return false; // Hmm?!

    // Play it!
    if(!sendMCICmd(0, 0, "play " DEVICEID " from %i to %i", track,
                   MCI_MAKE_TMSF(track, 0, len, 0)))
        return false;

    // Success!
    cdLooping = (looped? true:false);
    cdStartTime = Timer_Seconds();
    return cdCurrentTrack = track;
}

void DM_CDAudio_Pause(int pause)
{
    if(!cdInited) return;

    sendMCICmd(0, 0, "%s " DEVICEID, pause ? "pause" : "play");
    if(pause)
    {
        cdPauseTime = Timer_Seconds();
    }
    else
    {
        cdStartTime += Timer_Seconds() - cdPauseTime;
    }
}

void DM_CDAudio_Stop()
{
    if(!cdInited || !cdCurrentTrack) return;

    cdCurrentTrack = 0;
    sendMCICmd(0, 0, "stop " DEVICEID);
}
