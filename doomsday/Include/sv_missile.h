//===========================================================================
// SV_MISSILE.H
//===========================================================================
#ifndef __DOOMSDAY_SERVER_POOL_MISSILE_H__
#define __DOOMSDAY_SERVER_POOL_MISSILE_H__

#include "sv_def.h"
#include "sv_pool.h"

misrecord_t *Sv_MRFind(pool_t *pool, thid_t id);
void Sv_MRAdd(pool_t *pool, const mobjdelta_t *delta);
int	Sv_MRCheck(pool_t *pool, const mobjdelta_t *mobj);
void Sv_MRRemove(pool_t *pool, thid_t id);

#endif 