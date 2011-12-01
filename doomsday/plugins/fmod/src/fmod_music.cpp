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

int DM_Music_Init(void)
{
    return fmodSystem != 0;
}

void DM_Music_Shutdown(void)
{

}

void DM_Music_Set(int prop, float value)
{
    if(!midiAvail)
        return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        {
        /*
        int                 val = MINMAX_OF(0, (byte) (value * 255 + .5f), 255);

        // Straighten the volume curve.
        val <<= 8; // Make it a word.
        val = (int) (255.9980469 * sqrt(value));
        mixer4i(MIX_MIDI, MIX_SET, MIX_VOLUME, val);
        */
        break;
        }

    default:
        break;
    }
}

int DM_Music_Get(int prop, void* ptr)
{
    switch(prop)
    {
    case MUSIP_ID:
        if(ptr)
        {
            strcpy((char*) ptr, "FMOD/Mus");
            return true;
        }
        break;

    case MUSIP_PLAYING:
        /*
        if(midiAvail && MIDIStreamer)
            return (MIDIStreamer->IsPlaying()? true : false);*/
        return false;

    default:
        break;
    }

    return false;
}

void DM_Music_Update(void)
{
    // No need to do anything. The callback handles restarting.
}

void DM_Music_Stop(void)
{
    if(midiAvail)
    {
        //MIDIStreamer->Stop();
    }
}

int DM_Music_Play(int looped)
{
    if(midiAvail)
    {
        return true;
    }
    return false;
}

void DM_Music_Pause(int setPause)
{
    if(midiAvail)
    {
    }
}

void* DM_Music_SongBuffer(unsigned int length)
{
    if(midiAvail)
    {
//        return MIDIStreamer->SongBuffer(length);
    }

    return NULL;
}

int DM_Music_PlayFile(const char *filename, int looped)
{

}
