/** @file s_cache.h Sound Sample Cache
 *
 * The data is stored using M_Malloc().
 *
 * To play a sound:
 *  1) Figure out the ID of the sound.
 *  2) Call Sfx_Cache() to get a sfxsample_t.
 *  3) Pass the sfxsample_t to Sfx_StartSound().

 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG_SOUND_CACHE_H
#define LIBDENG_SOUND_CACHE_H

#include "api_audiod_sfx.h"

#ifdef __cplusplus
extern "C" {
#endif

void Sfx_InitCache(void);

void Sfx_ShutdownCache(void);


/**
 * @return  Ptr to the cached copy of the sample (give this ptr to
 *          Sfx_StartSound(); otherwise @c 0 if invalid.
 */
sfxsample_t *Sfx_Cache(int id);

void Sfx_CacheHit(int id);

/**
 * @return  The length of the sound (in milliseconds).
 */
uint Sfx_GetSoundLength(int id);

/**
 * @return  Number of bytes and samples cached.
 */
void Sfx_GetCacheInfo(uint *cacheBytes, uint *sampleCount);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_SOUND_CACHE_H */
