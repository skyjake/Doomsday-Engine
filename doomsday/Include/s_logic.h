//===========================================================================
// S_LOGIC.H
//===========================================================================
#ifndef __DOOMSDAY_SOUND_LOGICAL_H__
#define __DOOMSDAY_SOUND_LOGICAL_H__

void		Sfx_InitLogical(void);
void		Sfx_PurgeLogical(void);
void		Sfx_StartLogical(int id, mobj_t *origin, boolean isRepeating);
int			Sfx_StopLogical(int id, mobj_t *origin);
boolean		Sfx_IsPlaying(int id, mobj_t *origin);

#endif 