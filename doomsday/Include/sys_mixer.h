//===========================================================================
// SYS_MIXER.H
//===========================================================================
#ifndef __DOOMSDAY_SYSTEM_MIXER_H__
#define __DOOMSDAY_SYSTEM_MIXER_H__

// Mixer return values.
enum
{
	MIX_ERROR = -1,
	MIX_OK
};

// Mixer devices.
enum 
{
	MIX_CDAUDIO,
	MIX_MIDI
};

// Mixer actions.
enum
{
	MIX_GET,
	MIX_SET
};

// Mixer controls.
enum
{
	MIX_VOLUME			// 0-255
};

int		Sys_InitMixer(void);
void	Sys_ShutdownMixer(void);
int		Sys_Mixer4i(int device, int action, int control, int parm);
int		Sys_Mixer3i(int device, int action, int control);

#endif 
