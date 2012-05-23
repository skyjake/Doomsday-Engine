/**\file dswinmm.cpp
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2008-2012 Daniel Swanson <danij@dengine.net>
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
 * Music Driver for audio playback using Windows Multimedia (winmm).
 */

// HEADER FILES ------------------------------------------------------------

#define DENG2_C_API_ONLY
#include <de/c_wrapper.h>

#include <math.h>

#include "dswinmm.h"
#include "midistream.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct mixerdata_s {
    boolean available;
    MIXERLINE line;
    MIXERLINECONTROLS controls;
    MIXERCONTROL volume;
} mixerdata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initedOk = false;
static int verbose = 0;

static int midiAvail = false;
static WinMIDIStreamer* MIDIStreamer = NULL;

static int initMixerOk = 0;
static MMRESULT res;
static HMIXER mixer = NULL;
static mixerdata_t mixCD, mixMidi;

static int origVol; // The original MIDI volume.
static int origCDVol; // The original CD-DA volume.

// CODE --------------------------------------------------------------------

int mixer4i(int device, int action, int control, int parm)
{
    MIXERCONTROLDETAILS ctrlDetails;
    MIXERCONTROLDETAILS_UNSIGNED mcdUnsigned[2];
    MIXERCONTROL *mctrl;
    MIXERLINE  *mline;
    mixerdata_t *mix;
    int         i;

    if(!initMixerOk)
        return MIX_ERROR;

    // This is quite specific at the moment.
    // Only allow setting the CD volume.
    if(device != MIX_CDAUDIO && device != MIX_MIDI)
        return MIX_ERROR;
    if(control != MIX_VOLUME)
        return MIX_ERROR;

    // Choose the mixer line.
    mix = (device == MIX_CDAUDIO ? &mixCD : &mixMidi);

    // Is the mixer line for the requested device available?
    if(!mix->available)
        return MIX_ERROR;

    mline = &mix->line;
    mctrl = &mix->volume;

    // Init the data structure.
    memset(&ctrlDetails, 0, sizeof(ctrlDetails));
    ctrlDetails.cbStruct = sizeof(ctrlDetails);
    ctrlDetails.dwControlID = mctrl->dwControlID;
    ctrlDetails.cChannels = 1;  //mline->cChannels;
    ctrlDetails.cbDetails = sizeof(mcdUnsigned);
    ctrlDetails.paDetails = &mcdUnsigned;

    switch(action)
    {
    case MIX_GET:
        res =
            mixerGetControlDetails((HMIXEROBJ) mixer, &ctrlDetails,
                                   MIXER_GETCONTROLDETAILSF_VALUE);
        if(res != MMSYSERR_NOERROR)
            return MIX_ERROR;

        // The bigger one is the real volume.
        i = mcdUnsigned[mcdUnsigned[0].dwValue >
                        mcdUnsigned[1].dwValue ? 0 : 1].dwValue;

        // Return the value in range 0-255.
        return (255 * (i - mctrl->Bounds.dwMinimum)) /
            (mctrl->Bounds.dwMaximum - mctrl->Bounds.dwMinimum);

    case MIX_SET:
        // Clamp it.
        if(parm < 0)
            parm = 0;
        if(parm > 255)
            parm = 255;

        // Set both channels to the same volume (center balance).
        mcdUnsigned[0].dwValue = mcdUnsigned[1].dwValue =
            (parm * (mctrl->Bounds.dwMaximum - mctrl->Bounds.dwMinimum)) /
            255 + mctrl->Bounds.dwMinimum;

        res =
            mixerSetControlDetails((HMIXEROBJ) mixer, &ctrlDetails,
                                   MIXER_SETCONTROLDETAILSF_VALUE);
        if(res != MMSYSERR_NOERROR)
            return MIX_ERROR;
        break;

    default:
        return MIX_ERROR;
    }
    return MIX_OK;
}

static int mixer3i(int device, int action, int control)
{
    return mixer4i(device, action, control, 0);
}

static void initMixerLine(mixerdata_t* mix, DWORD type)
{
    memset(mix, 0, sizeof(*mix));
    mix->line.cbStruct = sizeof(mix->line);
    mix->line.dwComponentType = type;
    if((res =
        mixerGetLineInfo((HMIXEROBJ) mixer, &mix->line,
                         MIXER_GETLINEINFOF_COMPONENTTYPE)) !=
       MMSYSERR_NOERROR)
    {
        if(verbose)
            Con_Message("  Error getting line info: " "Error %i\n", res);
        return;
    }

    if(verbose)
    {
        Con_Message("  Destination line idx: %i\n", mix->line.dwDestination);
        Con_Message("  Line ID: 0x%x\n", mix->line.dwLineID);
        Con_Message("  Channels: %i\n", mix->line.cChannels);
        Con_Message("  Controls: %i\n", mix->line.cControls);
        Con_Message("  Name: %s (%s)\n", mix->line.szName,
                    mix->line.szShortName);
    }

    mix->controls.cbStruct = sizeof(mix->controls);
    mix->controls.dwLineID = mix->line.dwLineID;
    mix->controls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
    mix->controls.cControls = 1;
    mix->controls.cbmxctrl = sizeof(mix->volume);
    mix->controls.pamxctrl = &mix->volume;
    if((res =
        mixerGetLineControls((HMIXEROBJ) mixer, &mix->controls,
                             MIXER_GETLINECONTROLSF_ONEBYTYPE)) !=
       MMSYSERR_NOERROR)
    {
        if(verbose)
            Con_Message("  Error getting line controls " "(vol): error %i\n",
                        res);
        return;
    }

    if(verbose)
    {
        Con_Message("  Volume control ID: 0x%x\n", mix->volume.dwControlID);
        Con_Message("  Name: %s (%s)\n", mix->volume.szName,
                    mix->volume.szShortName);
        Con_Message("  Min/Max: %i/%i\n", mix->volume.Bounds.dwMinimum,
                    mix->volume.Bounds.dwMaximum);
    }

    // This mixer line is now available.
    mix->available = true;
}

/**
 * A ridiculous amount of code to do something this simple.
 * But mixers are pretty abstract a subject, I guess...
 * (No, the API just sucks.)
 */
static int initMixer(void)
{
    MIXERCAPS   mixerCaps;
    int         num = mixerGetNumDevs(); // Number of mixer devices.

    if(initMixerOk || CommandLine_Check("-nomixer"))
        return true;

    if(verbose)
    {
        // In verbose mode, print a lot of extra information.
        Con_Message("dsWinMM::initMixer: Number of mixer devices: %i\n", num);
    }

    // Open the mixer device.
    res = mixerOpen(&mixer, 0, 0, 0, MIXER_OBJECTF_MIXER);
    if(res != MMSYSERR_NOERROR)
    {
        if(verbose)
            Con_Message("  Error opening mixer: Error %i\n", res);
        return 0;
    }

    // Get the device caps.
    mixerGetDevCaps((UINT_PTR) mixer, &mixerCaps, sizeof(mixerCaps));

    Con_Message("dsWinMM::initMixer: %s\n", mixerCaps.szPname);
    if(verbose)
        Con_Message("  Audio line destinations: %i\n",
                    mixerCaps.cDestinations);

    // Init CD mixer.
    if(verbose)
        Con_Message("Init CD audio line:\n");
    initMixerLine(&mixCD, MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC);
    if(verbose)
        Con_Message("Init synthesizer line:\n");
    initMixerLine(&mixMidi, MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER);

    // We're successful.
    initMixerOk = true;

    // Get the original mixer volume settings (restored at shutdown).
    origVol = mixer3i(MIX_MIDI, MIX_GET, MIX_VOLUME);
    origCDVol = mixer3i(MIX_CDAUDIO, MIX_GET, MIX_VOLUME);

    return true;
}

static void shutdownMixer(void)
{
    if(!initMixerOk)
        return; // Can't uninitialize if not inited.

    // Restore the original mixer volumes, if possible.
    mixer4i(MIX_MIDI, MIX_SET, MIX_VOLUME, origVol);
    if(origCDVol != MIX_ERROR)
        mixer4i(MIX_CDAUDIO, MIX_SET, MIX_VOLUME, origCDVol);

    mixerClose(mixer);
    mixer = NULL;
    initMixerOk = false;
}

int DS_Init(void)
{
    // Are we in verbose mode?
    verbose = CommandLine_Exists("-verbose");

    initMixer();

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

    shutdownMixer();

    initedOk = false;
}

/**
 * The Event function is called to tell the driver about certain critical
 * events like the beginning and end of an update cycle.
 */
void DS_Event(int /*type*/)
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
    MIDIStreamer->volumeShift = CommandLine_Exists("-mdvol") ? 1 : 0;

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
    if(!midiAvail)
        return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        {
        int                 val = MINMAX_OF(0, (byte) (value * 255 + .5f), 255);

        // Straighten the volume curve.
        val <<= 8; // Make it a word.
        val = (int) (255.9980469 * sqrt(value));
        mixer4i(MIX_MIDI, MIX_SET, MIX_VOLUME, val);
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
            strcpy((char*) ptr, "WinMM::Mus");
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

void* DM_Music_SongBuffer(unsigned int length)
{
    if(midiAvail)
    {
        return MIDIStreamer->SongBuffer(length);
    }

    return NULL;
}
