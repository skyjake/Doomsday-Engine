/** @file samplecache.h  Sound sample cache.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef AUDIO_SAMPLECACHE_H
#define AUDIO_SAMPLECACHE_H

#include "api_audiod_sfx.h"  // sfxsample_t
#include <de/Observers>

namespace audio {

/**
 * @todo Use libgui's de::Waveform instead. -ds
 */
class Sample : public sfxsample_t
{
public:
    /// Audience notified when the sample is about to be deleted.
    DENG2_DEFINE_AUDIENCE2(Deletion, void sampleBeingDeleted(Sample const &sample))

public:
    Sample();
    virtual ~Sample();

private:
    DENG2_PRIVATE(d)
};

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
class SampleCache
{
public:
    /// Audience notified when a sound sample is about to be removed from the cache.
    DENG2_DEFINE_AUDIENCE2(SampleRemove, void sampleCacheAboutToRemove(Sample const &sample))

    struct CacheItem
    {
        CacheItem *next = nullptr;
        CacheItem *prev = nullptr;

        CacheItem();

        /**
         * Returns @c true if a Sample data payload is assigned.
         * @ref sample(), replaceSample()
         */
        bool hasSample() const;

        /**
         * Returns the associated Sample data payload.
         * @ref hasSample(), replaceSample()
         */
        Sample &sample() const;

        /**
         * Replace the cached sample data with a @a newSample.
         * @ref hasSample(), sample()
         */
        void replaceSample(Sample *newSample);

        /**
         * Register a cache hit and remember the current time.
         */
        void hit();

        /**
         * Returns the total number of registered cache hits for the data payload.
         */
        de::dint hitCount() const;

        /**
         * Returns the time in tics when the last cache hit was registered for the
         * data payload.
         */
        de::dint lastUsed() const;

    private:
        DENG2_PRIVATE(d)
    };

public:
    /**
     * Construct a new (empty) sound sample cache.
     */
    SampleCache();

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
    Sample *cache(de::dint soundId);

    /**
     * Register a cache hit on the sound sample associated with @a id.
     *
     * Hits keep count of how many times the cached sound has been played. The purger
     * will remove samples with the lowest hitcount first.
     *
     * @param soundId  Sound sample identifier.
     */
    void hit(de::dint soundId);

    /**
     * Returns cache usage info (for debug).
     *
     * @param cacheBytes   Total number of bytes used is written here.
     * @param sampleCount  Total number of cached samples is written here.
     */
    void info(de::duint *cacheBytes, de::duint *sampleCount) const;

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // AUDIO_SAMPLECACHE_H
