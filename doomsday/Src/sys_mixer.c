/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sys_mixer.c: Win32 Multimedia Mixer 
 *
 * Mainly used by the Win Mus driver for setting the CD volume.
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct mixerdata_s {
	boolean				available;
	MIXERLINE			line;
	MIXERLINECONTROLS	controls;
	MIXERCONTROL		volume;
} mixerdata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int					initOk = 0;
static MMRESULT				res;
static HMIXER				mixer = NULL;
static mixerdata_t			mixCD, mixMidi;

// CODE --------------------------------------------------------------------

//===========================================================================
// Sys_InitMixerLine
//===========================================================================
void Sys_InitMixerLine(mixerdata_t *mix, DWORD type)
{
	memset(mix, 0, sizeof(*mix));
	mix->line.cbStruct = sizeof(mix->line);
	mix->line.dwComponentType = type;
	if((res = mixerGetLineInfo( (HMIXEROBJ) mixer, &mix->line, 
		MIXER_GETLINEINFOF_COMPONENTTYPE)) != MMSYSERR_NOERROR)
	{
		if(verbose) Con_Message("  Error getting line info: "
			"Error %i\n", res);
		return;
	}

	if(verbose)
	{
		Con_Message("  Destination line idx: %i\n", mix->line.dwDestination);
		Con_Message("  Line ID: 0x%x\n", mix->line.dwLineID);
		Con_Message("  Channels: %i\n", mix->line.cChannels);
		Con_Message("  Controls: %i\n", mix->line.cControls);
		Con_Message("  Name: %s (%s)\n", mix->line.szName, mix->line.szShortName);
	}

	mix->controls.cbStruct = sizeof(mix->controls);
	mix->controls.dwLineID = mix->line.dwLineID;
	mix->controls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
	mix->controls.cControls = 1;
	mix->controls.cbmxctrl = sizeof(mix->volume);
	mix->controls.pamxctrl = &mix->volume;
	if((res = mixerGetLineControls( (HMIXEROBJ) mixer, &mix->controls, 
		MIXER_GETLINECONTROLSF_ONEBYTYPE)) != MMSYSERR_NOERROR)
	{
		if(verbose) Con_Message("  Error getting line controls "
			"(vol): error %i\n", res);
		return;
	}

	if(verbose)
	{
		Con_Message("  Volume control ID: 0x%x\n", mix->volume.dwControlID);
		Con_Message("  Name: %s (%s)\n", mix->volume.szName, mix->volume.szShortName);
		Con_Message("  Min/Max: %i/%i\n", mix->volume.Bounds.dwMinimum, 
			mix->volume.Bounds.dwMaximum);
	}

	// This mixer line is now available.
	mix->available = true;
}

//===========================================================================
// Sys_InitMixer
//	A ridiculous amount of code to do something this simple.
//	But mixers are pretty abstract a subject, I guess... 
//	(No, the API just sucks.)
//===========================================================================
int Sys_InitMixer(void)
{
	MIXERCAPS	mixerCaps;
	int			num = mixerGetNumDevs();	// Number of mixer devices.

	if(initOk 
		|| ArgCheck("-nomixer") 
		|| ArgCheck("-nomusic") 
		|| isDedicated) return true;

	if(verbose) 
	{
		// In verbose mode, print a lot of extra information.
		Con_Message("Sys_InitMixer: Number of mixer devices: %i\n", num);	
	}

	// Open the mixer device.
	res = mixerOpen(&mixer, 0, 0, 0, MIXER_OBJECTF_MIXER);
	if(res != MMSYSERR_NOERROR)
	{
		if(verbose) Con_Message("  Error opening mixer: Error %i\n", res);
		return 0;
	}
	
	// Get the device caps.
	mixerGetDevCaps( (UINT) mixer, &mixerCaps, sizeof(mixerCaps));

	Con_Message("Sys_InitMixer: %s\n", mixerCaps.szPname);
	if(verbose) Con_Message("  Audio line destinations: %i\n", 
		mixerCaps.cDestinations);

	// Init CD mixer.
	if(verbose) Con_Message("Init CD audio line:\n");
	Sys_InitMixerLine(&mixCD, MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC);
	if(verbose) Con_Message("Init synthesizer line:\n");
	Sys_InitMixerLine(&mixMidi, MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER);

	// We're successful.
	initOk = true;
	return true;
}

//===========================================================================
// Sys_ShutdownMixer
//===========================================================================
void Sys_ShutdownMixer(void)
{
	if(!initOk) return; // Can't uninitialize if not inited.

	mixerClose(mixer);
	mixer = NULL;
	initOk = false;
}

//===========================================================================
// Sys_Mixer4i
//===========================================================================
int Sys_Mixer4i(int device, int action, int control, int parm)
{
	MIXERCONTROLDETAILS				ctrlDetails;
	MIXERCONTROLDETAILS_UNSIGNED	mcdUnsigned[2];
	MIXERCONTROL					*mctrl;
	MIXERLINE						*mline;
	mixerdata_t						*mix;
	int								i;

	if(!initOk) return MIX_ERROR;

	// This is quite specific at the moment. 
	// Only allow setting the CD volume.
	if(device != MIX_CDAUDIO && device != MIX_MIDI) return MIX_ERROR;
	if(control != MIX_VOLUME) return MIX_ERROR;

	// Choose the mixer line.
	mix = (device == MIX_CDAUDIO? &mixCD : &mixMidi);

	// Is the mixer line for the requested device available?
	if(!mix->available) return MIX_ERROR;

	mline = &mix->line; 
	mctrl = &mix->volume;

	// Init the data structure.
	memset(&ctrlDetails, 0, sizeof(ctrlDetails));
	ctrlDetails.cbStruct = sizeof(ctrlDetails);
	ctrlDetails.dwControlID = mctrl->dwControlID;
	ctrlDetails.cChannels = 1; //mline->cChannels;
	ctrlDetails.cbDetails = sizeof(mcdUnsigned);
	ctrlDetails.paDetails = &mcdUnsigned;

	switch(action)
	{
	case MIX_GET:
		res = mixerGetControlDetails( (HMIXEROBJ) mixer, &ctrlDetails, 
			MIXER_GETCONTROLDETAILSF_VALUE);
		if(res != MMSYSERR_NOERROR) return MIX_ERROR;
		
		// The bigger one is the real volume.
		i = mcdUnsigned[mcdUnsigned[0].dwValue > mcdUnsigned[1].dwValue? 0 : 1].dwValue;
		
		// Return the value in range 0-255.
		return (255*(i - mctrl->Bounds.dwMinimum)) / (mctrl->Bounds.dwMaximum 
			- mctrl->Bounds.dwMinimum);

	case MIX_SET:
		// Clamp it.
		if(parm < 0) parm = 0;
		if(parm > 255) parm = 255;

		// Set both channels to the same volume (center balance).
		mcdUnsigned[0].dwValue = mcdUnsigned[1].dwValue = 
			(parm*(mctrl->Bounds.dwMaximum - mctrl->Bounds.dwMinimum))/255 
			+ mctrl->Bounds.dwMinimum;

		res = mixerSetControlDetails( (HMIXEROBJ) mixer, &ctrlDetails,
			MIXER_SETCONTROLDETAILSF_VALUE);
		if(res != MMSYSERR_NOERROR) return MIX_ERROR;		
		break;

	default:
		return MIX_ERROR;
	}
	return MIX_OK;
}

//===========================================================================
// Sys_Mixer3i
//===========================================================================
int Sys_Mixer3i(int device, int action, int control)
{
	return Sys_Mixer4i(device, action, control, 0);	
}

