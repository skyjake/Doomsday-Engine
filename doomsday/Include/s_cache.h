//===========================================================================
// S_CACHE.H
//===========================================================================
#ifndef __DOOMSDAY_SOUND_CACHE_H__
#define __DOOMSDAY_SOUND_CACHE_H__

void			Sfx_InitCache(void);
void			Sfx_ShutdownCache(void);
sfxsample_t*	Sfx_Cache(int id);
void			Sfx_CacheHit(int id);
uint			Sfx_GetSoundLength(int id);
void			Sfx_GetCacheInfo(uint *cacheBytes, uint *sampleCount);

#endif 