/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * sys_cdaudio.c: Compact Disc-Digital Audio (CD-DA) / "Redbook".
 *
 * WIN32-specific, uses the MCI interface.
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define DEVICEID                "mycd"

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int DM_CDAudioInit(void);
void DM_CDAudioShutdown(void);
void DM_CDAudioUpdate(void);
void DM_CDAudioSet(int prop, float value);
int DM_CDAudioGet(int prop, void* ptr);
void DM_CDAudioPause(int pause);
void DM_CDAudioStop(void);
int DM_CDAudioPlay(int track, int looped);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

musinterface_cd_t musd_win_icd = {
    DM_CDAudioInit,
    DM_CDAudioUpdate,
    DM_CDAudioSet,
    DM_CDAudioGet,
    DM_CDAudioPause,
    DM_CDAudioStop,
    DM_CDAudioPlay
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int cdInited = false;

// The original volume of the CD-DA mixer channel so we can restore on exit.
static int cdOrigVolume;

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
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if((error = mciSendString(buf, returnInfo, returnLength, NULL)))
    {
        mciGetErrorString(error, buf, 300);
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

/**
 * Assign the value of a CDAudio-interface property.
 */
void DM_CDAudioSet(int prop, float value)
{
    if(!cdInited)
        return;

    switch(prop)
    {
    /**
     * Providing volume control here is a pretty old-fashioned approach.
     * Under most modern OSes, audio APIs don't even have access to the
     * control of mixer's output audio stream, they instead control the
     * in-mix volume of the waveform/synth pattern.
     *
     * Under WIN32 this has always been a bit of an issue but with the
     * newer APIs (like DirectSound and DirectMusic) and certainly with the
     * new audio paths under Vista this kind of malarky is almost a thing
     * of the past. However presently, MCI remains the only means to offer
     * direct CD-DA/Rebook playback so we adjust the mixer volume.
     */
    case MUSIP_VOLUME:
        Sys_Mixer4i(MIX_CDAUDIO, MIX_SET, MIX_VOLUME, value * 255 + 0.5);
        break;

    default:
        break;
    }
}

/**
 * Retrieve the value of a CDAudio-interface property.
 */
int DM_CDAudioGet(int prop, void* ptr)
{
    if(!cdInited)
        return false;

    switch(prop)
    {
    case MUSIP_ID:
        if(ptr)
        {
            strcpy(ptr, "Win/CD");
            return true;
        }
        break;

    default:
        break;
    }

    return false;
}

/**
 * Initialize the CDAudio-interface.
 */
int DM_CDAudioInit(void)
{
    if(cdInited)
        return true;

    if(!sendMCICmd(0, 0, "open cdaudio alias " DEVICEID))
        return false;

    if(!sendMCICmd(0, 0, "set " DEVICEID " time format tmsf"))
        return false;

    // Get the original CD volume, we'll try to restore it on exit in case
    // we change it at some point (likely).
    cdOrigVolume = Sys_Mixer3i(MIX_CDAUDIO, MIX_GET, MIX_VOLUME);

    cdCurrentTrack = 0;
    cdLooping = false;
    cdStartTime = cdPauseTime = cdTrackLength = 0;

    // Successful initialization.
    return cdInited = true;
}

/**
 * Shutdown the CDAudio-interface, we do nothing whilst offline.
 */
void DM_CDAudioShutdown(void)
{
    if(!cdInited)
        return;

    DM_CDAudioStop();
    sendMCICmd(0, 0, "close " DEVICEID);

    // Restore original CD volume, if possible.
    if(cdOrigVolume != MIX_ERROR)
        Sys_Mixer4i(MIX_CDAUDIO, MIX_SET, MIX_VOLUME, cdOrigVolume);

    cdInited = false;
}

/**
 * Do any necessary update tasks. Called every frame by the engine.
 */
void DM_CDAudioUpdate(void)
{
    if(!cdInited)
        return;

    // Check for looping.
    if(cdCurrentTrack && cdLooping &&
       Sys_GetSeconds() - cdStartTime > cdTrackLength)
    {
        // Restart the track.
        DM_CDAudioPlay(cdCurrentTrack, true);
    }
}

/**
 * Begin playback of a specifc audio track, possibly looped.
 */
int DM_CDAudioPlay(int track, int looped)
{
    int                 len;

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
    cdLooping = looped;
    cdStartTime = Sys_GetSeconds();
    return cdCurrentTrack = track;
}

/**
 * Pauses playback of the currently playing audio track.
 */
void DM_CDAudioPause(int pause)
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
void DM_CDAudioStop(void)
{
    if(!cdInited || !cdCurrentTrack)
        return;

    cdCurrentTrack = 0;
    sendMCICmd(0, 0, "stop " DEVICEID);
}
