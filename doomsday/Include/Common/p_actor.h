#ifndef __DD_COMMON_ACTOR_H__
#define __DD_COMMON_ACTOR_H__

void P_SetThingSRVO(mobj_t *mo, int stepx, int stepy);
void P_SetThingSRVOZ(mobj_t *mo, int stepz);
void P_SRVOAngleTicker(mobj_t *mo);
void P_ClearThingSRVO(mobj_t *mo);
void P_UpdateHealthBits(mobj_t *mobj);
void P_UpdateMobjFlags(mobj_t *mobj);

#endif
