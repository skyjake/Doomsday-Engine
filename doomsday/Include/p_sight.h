//===========================================================================
// P_SIGHT.H
//===========================================================================
#ifndef __DOOMSDAY_PLAY_SIGHT_H__
#define __DOOMSDAY_PLAY_SIGHT_H__

extern fixed_t topslope;
extern fixed_t bottomslope;		

boolean P_CheckReject(sector_t *sec1, sector_t *sec2);
boolean P_CheckFrustum(int plrNum, mobj_t *mo);
boolean P_CheckSight(mobj_t *t1, mobj_t *t2);

#endif 