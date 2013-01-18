/** @file s_cache.h
 *
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

/**
 * Sound Sample Cache.
 */

#ifndef LIBDENG_SOUND_CACHE_H
#define LIBDENG_SOUND_CACHE_H

#include "api_audiod_sfx.h"

#ifdef __cplusplus
extern "C" {
#endif

void Sfx_InitCache(void);

void Sfx_ShutdownCache(void);

sfxsample_t* Sfx_Cache(int id);

void Sfx_CacheHit(int id);

uint Sfx_GetSoundLength(int id);

void Sfx_GetCacheInfo(uint* cacheBytes, uint* sampleCount);

#ifdef __cplusplus
} // __cplusplus
#endif

#endif /* LIBDENG_SOUND_CACHE_H */
