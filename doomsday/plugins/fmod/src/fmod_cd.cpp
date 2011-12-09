/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "driver_fmod.h"

static int numDrives;

int DM_CDAudio_Init(void)
{
    numDrives = 0;
    FMOD_RESULT result;
    result = fmodSystem->getNumCDROMDrives(&numDrives);
    DSFMOD_ERRCHECK(result);
    DSFMOD_TRACE("CDAudio_Init: " << numDrives << " CD drives available.");

    return fmodSystem != 0;
}

void DM_CDAudio_Shutdown(void)
{
    // Will be shut down with the rest of FMOD.
    DSFMOD_TRACE("CDAudio_Shutdown.");
}

void DM_CDAudio_Update(void)
{
    // No need to update anything.
}

void DM_CDAudio_Set(int prop, float value)
{
    if(!fmodSystem) return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        DM_Music_Set(MUSIP_VOLUME, value);
        break;

    default:
        break;
    }
}

int DM_CDAudio_Get(int prop, void* ptr)
{
    if(!fmodSystem) return false;

    switch(prop)
    {
    case MUSIP_ID:
        if(ptr)
        {
            strcpy((char*) ptr, "FMOD/CD");
            return true;
        }
        break;

    case MUSIP_PLAYING:
        return DM_Music_Get(MUSIP_PLAYING, ptr);

    default:
        return false;
    }

    return true;
}

int DM_CDAudio_Play(int track, int looped)
{
    if(!fmodSystem) return false;
    if(!numDrives)
    {
        DSFMOD_TRACE("CDAudio_Play: No CD drives available.");
        return false;
    }

    // Use a bigger stream buffer for CD audio.
    fmodSystem->setStreamBufferSize(64*1024, FMOD_TIMEUNIT_RAWBYTES);



    return true;
}

void DM_CDAudio_Pause(int pause)
{
    DM_Music_Pause(pause);
}

void DM_CDAudio_Stop(void)
{
    DM_Music_Stop();
}
