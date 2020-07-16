/** @file s_cache.h  Sound sample cache.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef AUDIO_SFXSAMPLECACHE_H
#define AUDIO_SFXSAMPLECACHE_H

#include "api_audiod_sfx.h"  // sfxsample_t
#include <de/observers.h>

namespace audio {

/**
 * Data cache for sfxsample_t.
 *
 * To play a sound:
 *  1) Figure out the ID of the sound.
 *  2) Call @ref cache() to get a sfxsample_t.
 *  3) Pass the sfxsample_t to Sfx_StartSound().
 *
 * @todo Use de::WaveformBank instead. -ds
 */
class SfxSampleCache
{
public:
    /// Notified when a sound sample is about to be removed from the cache.
    DE_AUDIENCE(SampleRemove, void sfxSampleCacheAboutToRemove(const sfxsample_t &sample))

    struct CacheItem
    {
        CacheItem *next, *prev;

        int hits;            ///< Number of cache hits.
        int lastUsed;        ///< Tic the sample was last hit.
        sfxsample_t sample;  ///< The cached sample data.

        CacheItem();
        ~CacheItem();

        /**
         * Register a cache hit and remember the current tic.
         */
        void hit();

        /**
         * Replace the cached sample data with a copy of @a newSample.
         */
        void replaceSample(sfxsample_t &newSample);
    };

public:
    /**
     * Construct a new (empty) sound sample cache.
     */
    SfxSampleCache();

    /**
     * Call this to clear all sound samples from the cache.
     */
    void clear();

    /**
     * Call this periodically to perform a cache purge. If the cache is too large,
     * stopped samples with the lowest hitcount will be uncached.
     */
    void maybeRunPurge();

    /**
     * Lookup a cached copy of the sound sample associated with @a id. (Give this
     * ptr to @ref Sfx_StartSound()).
     *
     * @param soundId  Sound sample identifier.
     * @return  Associated sfxsample_t if found; otherwise @c nullptr.
     */
    sfxsample_t *cache(int soundId);

    /**
     * Register a cache hit on the sound sample associated with @a id.
     *
     * Hits keep count of how many times the cached sound has been played. The purger
     * will remove samples with the lowest hitcount first.
     *
     * @param soundId  Sound sample identifier.
     */
    void hit(int soundId);

    /**
     * Returns cache usage info (for debug).
     *
     * @param cacheBytes   Total number of bytes used is written here.
     * @param sampleCount  Total number of cached samples is written here.
     */
    void info(uint *cacheBytes, uint *sampleCount);

private:
    DE_PRIVATE(d)
};

}  // namespace audio

#endif  // AUDIO_SFXSAMPLECACHE_H
