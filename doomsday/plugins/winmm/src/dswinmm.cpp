/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008 Daniel Swanson <danij@dengine.net>
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
 * dswinmm.cpp: Music Driver for Win32 Multimedia (winmm).
 */

// HEADER FILES ------------------------------------------------------------

#include "dswinmm.h"
#include "midistream.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initedOk = false;
static int midiAvail = false;
static WinMIDIStreamer* MIDIStreamer = NULL;

// CODE --------------------------------------------------------------------

int DS_Init(void)
{
    initedOk = true;
    return true;
}

void DS_Shutdown(void)
{
    if(!initedOk)
        return; // Wha?

    // In case the engine hasn't already done so, close open interfaces.
    DM_CDAudio_Shutdown();
    DM_Music_Shutdown();

    initedOk = false;
}

/**
 * The Event function is called to tell the driver about certain critical
 * events like the beginning and end of an update cycle.
 */
void DS_Event(int type)
{
    // Do nothing...
}

/**
 * @return              @c true, if successful.
 */
int DM_Music_Init(void)
{
    if(midiAvail)
        return true; // Already initialized.

    Con_Message("DM_WinMusInit: %i MIDI-Out devices present.\n",
                midiOutGetNumDevs());

    MIDIStreamer = new WinMIDIStreamer;

    // Open the midi stream.
    if(!MIDIStreamer || !MIDIStreamer->OpenStream())
        return false;

    // Double output volume?
    MIDIStreamer->volumeShift = ArgExists("-mdvol") ? 1 : 0;

    // Now the MIDI is available.
    Con_Message("DM_WinMusInit: MIDI initialized.\n");

    return midiAvail = true;
}

void DM_Music_Shutdown(void)
{
    if(midiAvail)
    {
        delete MIDIStreamer;
        MIDIStreamer = NULL;
        midiAvail = false;
    }
}

void DM_Music_Set(int prop, float value)
{
    // No unique properties.
}

int DM_Music_Get(int prop, void* ptr)
{
    switch(prop)
    {
    case MUSIP_ID:
        if(ptr)
        {
            strcpy((char*) ptr, "Win/Mus");
            return true;
        }
        break;

    case MUSIP_PLAYING:
        if(midiAvail && MIDIStreamer)
            return (MIDIStreamer->IsPlaying()? true : false);
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
        MIDIStreamer->Stop();
    }
}

int DM_Music_Play(int looped)
{
    if(midiAvail)
    {
        MIDIStreamer->Play(looped);
        return true;
    }

    return false;
}

void DM_Music_Pause(int setPause)
{
    if(midiAvail)
    {
        MIDIStreamer->Pause(setPause);
    }
}

void* DM_Music_SongBuffer(size_t length)
{
    if(midiAvail)
    {
        return MIDIStreamer->SongBuffer(length);
    }

    return NULL;
}
