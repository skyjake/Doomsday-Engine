//===========================================================================
// S_MUS.H 
//===========================================================================
#ifndef __DOOMSDAY_SOUND_MUSIC_H__
#define __DOOMSDAY_SOUND_MUSIC_H__

#include "con_decl.h"
#include "sys_musd.h"

// Music preference. If multiple resources are available, this setting
// is used to determine which one to use (mus < ext < cd).
enum
{
	MUSP_MUS,
	MUSP_EXT,
	MUSP_CD
};

extern int	mus_preference;

boolean		Mus_Init(void);
void		Mus_Shutdown(void);
void		Mus_SetVolume(float vol);
void		Mus_Pause(boolean do_pause);
void		Mus_StartFrame(void);
int			Mus_Start(ded_music_t *def, boolean looped);
void		Mus_Stop(void);

// Console commands.
D_CMD( PlayMusic );
D_CMD( PlayExt );
D_CMD( StopMusic );

#endif 