/** @file samplecache.cpp  Sound sample cache.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "audio/samplecache.h"

#include "audio/idriver.h"
#include "audio/mixer.h"
#include "audio/system.h"

#include "clientapp.h"
#include "dd_main.h"    // App_AudioSystem()
#include "def_main.h"   // Def_Get*()

#include <doomsday/filesys/fs_main.h>
#include <doomsday/resource/wav.h>
#include <de/memory.h>
#include <de/memoryzone.h>
#include <de/timer.h>
#include <cstring>

using namespace de;

namespace audio {

// The cached samples are stored in a hash. When a sample is purged, its data will stay
// in the hash (sample lengths needed by the Logical Sound Manager).
static dint const CACHE_HASH_SIZE  = 64;

static timespan_t const PURGE_TIME = 10 * TICSPERSEC;

// 1 Mb = about 12 sec of 44KHz 16bit sound in the cache.
static dint const MAX_CACHE_KB     = 4096;

// Even one minute of silence is quite a long time during gameplay.
static dint const MAX_CACHE_TICS   = TICSPERSEC * 60 * 4;  // 4 minutes.

// Utility for converting an unsigned byte to signed short (for resampling).
static dshort inline U8_S16(duchar b)
{
    return (b - 0x80) << 8;
}

/**
 * Simple linear resampling with possible conversion to 16 bits. The destination sample
 * must be initialized and it must have a large enough buffer. We won't reduce rate or
 * bits here.
 *
 * @note This is not a clean way to resample a sound. If you read about DSP a bit, you'll
 * find out that interpolation adds a lot of extra frequencies in the sample. It should
 * be low-pass filtered after the interpolation.
 */
static void resample(void *dst, dint dstBytesPer, dint dstRate, void const *src,
    dint srcBytesPer, dint srcRate, dint srcNumSamples, duint srcSize)
{
    DENG2_ASSERT(src && dst);

    // Let's first check for the easy cases.
    if(dstRate == srcRate)
    {
        if(srcBytesPer == dstBytesPer)
        {
            // A simple copy will suffice.
            std::memcpy(dst, src, srcSize);
        }
        else if(srcBytesPer == 1 && dstBytesPer == 2)
        {
            // Just changing the bytes won't do much good...
            duchar const *sp = (duchar const *) src;
            dshort *dp       = (dshort *) dst;

            for(dint i = 0; i < srcNumSamples; ++i)
            {
                *dp++ = (*sp++ - 0x80) << 8;
            }
        }
        return;
    }

    // 2x resampling.
    if(dstRate == 2 * srcRate)
    {
        if(dstBytesPer == 1)
        {
            // The source has a byte per sample as well.
            duchar const *sp = (duchar const *) src;
            duchar *dp       = (duchar *) dst;

            for(dint i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                *dp++ = *sp;
                *dp++ = (*sp + sp[1]) >> 1;
            }

            // Fill in the last two as well.
            dp[0] = dp[1] = *sp;
        }
        else if(srcBytesPer == 1)
        {
            // Destination is signed 16bit. Source is 8bit.
            duchar const *sp = (duchar const *) src;
            dshort *dp       = (dshort *) dst;

            for(dint i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                dshort const first = U8_S16(*sp);

                *dp++ = first;
                *dp++ = (first + U8_S16(sp[1])) >> 1;
            }

            // Fill in the last two as well.
            dp[0] = dp[1] = U8_S16(*sp);
        }
        else if(srcBytesPer == 2)
        {
            // Destination is signed 16bit. Source is 16bit.
            dshort const *sp = (dshort const *) src;
            dshort *dp       = (dshort *) dst;

            for(dint i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                *dp++ = *sp;
                *dp++ = (*sp + sp[1]) >> 1;
            }

            dp[0] = dp[1] = *sp;
        }
        return;
    }

    // 4x resampling (11Khz => 44KHz only).
    if(dstRate == 4 * srcRate)
    {
        if(dstBytesPer == 1)
        {
            // The source has a byte per sample as well.
            duchar const *sp = (duchar const *) src;
            duchar *dp       = (duchar *) dst;

            for(dint i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                duchar const mid = (*sp + sp[1]) >> 1;

                *dp++ = *sp;
                *dp++ = (*sp + mid) >> 1;
                *dp++ = mid;
                *dp++ = (mid + sp[1]) >> 1;
            }

            // Fill in the last four as well.
            dp[0] = dp[1] = dp[2] = dp[3] = *sp;
        }
        else if(srcBytesPer == 1)
        {
            // Destination is signed 16bit. Source is 8bit.
            duchar const *sp = (duchar const *) src;
            dshort *dp       = (dshort *) dst;

            for(int i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                dshort const first = U8_S16(*sp);
                dshort const last  = U8_S16(sp[1]);
                dshort const mid   = (first + last) >> 1;

                *dp++ = first;
                *dp++ = (first + mid) >> 1;
                *dp++ = mid;
                *dp++ = (mid + last) >> 1;
            }

            // Fill in the last four as well.
            dp[0] = dp[1] = dp[2] = dp[3] = U8_S16(*sp);
        }
        else if(srcBytesPer == 2)
        {
            // Destination is signed 16bit. Source is 16bit.
            dshort const *sp = (dshort const *) src;
            dshort *dp       = (dshort *) dst;

            for(dint i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                dshort const mid = (*sp + sp[1]) >> 1;

                *dp++ = *sp;
                *dp++ = (*sp + mid) >> 1;
                *dp++ = mid;
                *dp++ = (mid + sp[1]) >> 1;
            }

            // Fill in the last four as well.
            dp[0] = dp[1] = dp[2] = dp[3] = *sp;
        }
    }
}

/**
 * Configure the given sound sample @a smp.
 *
 * @param smp         sfxsample_t info to configure.
 * @param data        Actual sample data.
 * @param size        Size in bytes.
 * @param numSamples  Number of samples.
 * @param bytesPer    Bytes per sample (1 or 2).
 * @param rate        Samples per second.
 */
static void configureSample(sfxsample_t &smp, void const * /*data*/, duint size,
    dint numSamples, dint bytesPer, dint rate)
{
    de::zap(smp);
    smp.bytesPer   = bytesPer;
    smp.size       = numSamples * bytesPer;
    smp.rate       = rate;
    smp.numSamples = numSamples;

    // Apply the upsample factor.
    dint const scale = System::get().upsampleFactor(rate);
    smp.rate       *= scale;
    smp.numSamples *= scale;
    smp.size       *= scale;

    // Resample to 16bit?
    if(::sfxBits == 16 && smp.bytesPer == 1)
    {
        smp.bytesPer  = 2;
        smp.size     *= 2;
    }
}

DENG2_PIMPL_NOREF(Sample)
{
    DENG2_PIMPL_AUDIENCE(Deletion)
};

DENG2_AUDIENCE_METHOD(Sample, Deletion)

Sample::Sample() : d(new Instance)
{
    de::zapPtr(static_cast<sfxsample_t *>(this));
}

Sample::~Sample()
{
    // Notify interested parties.
    DENG2_FOR_AUDIENCE2(Deletion, i)
    {
        i->sampleBeingDeleted(*this);
    }

    // We have ownership of the sound data.
    M_Free(data);
}

DENG2_PIMPL_NOREF(SampleCache::CacheItem)
{
    dint hits = 0;                   ///< Total number of cache hits.
    dint lastUsed = 0;               ///< Time in tics when a cache hit was last registered.
    std::unique_ptr<Sample> sample;  ///< Cached sample data (owned).
};

SampleCache::CacheItem::CacheItem() : d(new Instance)
{}

void SampleCache::CacheItem::hit()
{
    d->hits += 1;
    d->lastUsed = Timer_Ticks();
}

dint SampleCache::CacheItem::hitCount() const
{
    return d->hits;
}

dint SampleCache::CacheItem::lastUsed() const
{
    return d->lastUsed;
}

bool SampleCache::CacheItem::hasSample() const
{
    return bool( d->sample );
}

Sample &SampleCache::CacheItem::sample() const
{
    return *d->sample;
}

void SampleCache::CacheItem::replaceSample(Sample *newSample)
{
    d->hits = 0;
    d->sample.reset(newSample);
}

DENG2_PIMPL(SampleCache)
{
    /**
     * Cached samples are placed in a hash (key: sound id).
     */
    struct Hash
    {
        CacheItem *first = nullptr;
        CacheItem *last  = nullptr;
    } hash[CACHE_HASH_SIZE];

    dint lastPurge = 0;  ///< Time of the last purge (in game ticks).

    Instance(Public *i) : Base(i) {}
    ~Instance() { removeAll(); }

    /**
     * Find the appropriate hash for the given @a soundId.
     */
    Hash &hashFor(dint soundId)
    {
        return hash[duint( soundId ) % CACHE_HASH_SIZE];
    }

    Hash const &hashFor(dint soundId) const
    {
        return const_cast<Instance *>(this)->hashFor(soundId);
    }

    /**
     * Lookup a CacheItem with the given @a soundId.
     */
    CacheItem *tryFind(dint soundId) const
    {
        for(CacheItem *it = hashFor(soundId).first; it; it = it->next)
        {
            if(it->sample().soundId == soundId)
                return it;
        }
        return nullptr;  // Not found.
    }

    /**
     * Add a new CacheItem with the given @a soundId to the hash and return
     * it (ownership is retained).
     */
    CacheItem &insertCacheItem(dint soundId)
    {
        auto *item = new CacheItem;

        Hash &hash = hashFor(soundId);
        if(hash.last)
        {
            hash.last->next = item;
            item->prev = hash.last;
        }
        hash.last = item;

        if(!hash.first)
            hash.first = item;

        return *item;
    }

    void removeCacheItem(CacheItem &item)
    {
        ClientApp::audioSystem().allowChannelRefresh(false);

        notifyRemove(item);

        Hash &hash = hashFor(item.sample().soundId);

        // Unlink the item.
        if(hash.last == &item)
            hash.last = item.prev;
        if(hash.first == &item)
            hash.first = item.next;

        if(item.next)
            item.next->prev = item.prev;
        if(item.prev)
            item.prev->next = item.next;

        ClientApp::audioSystem().allowChannelRefresh();

        // Free all memory allocated for the item.
        delete &item;
    }

    /**
     * Caches a copy of the given sample. If it's already in the cache and has the
     * same format, nothing is done.
     *
     * If the sample @a data is already in the right format, just make a copy of it.
     *
     * If necessary, resample the sound @a data upwards to the minimum resolution
     * and bits (specified in the user Config). (You can play higher resolution sounds
     * than the current setting, but not lower resolution ones.)
     *
     * Converts the sample @a data and writes it to the (M_Malloc() allocated) buffer
     * (ownership is given to the sfxsample_t).
     *
     * @param soundId     Id number of the sound sample.
     * @param data        Actual sample data.
     * @param size        Size in bytes.
     * @param numSamples  Number of samples.
     * @param bytesPer    Bytes per sample (1 or 2).
     * @param rate        Samples per second.
     * @param group       Exclusion group (0, if none).
     *
     * @returns  The cached sample.
     */
    CacheItem &insert(dint soundId, void const *data, duint size, dint numSamples,
        dint bytesPer, dint rate, dint group)
    {
        std::unique_ptr<Sample> cached(new Sample);
        configureSample(*cached, data, size, numSamples, bytesPer, rate);

        // Have we already cached a comparable sample?
        CacheItem *item = tryFind(soundId);
        if(item)
        {
            // A sample is already in the cache.
            // If the existing sample is in the same format - use it.
            if(item->sample().bytesPer == cached->bytesPer &&
               item->sample().rate     == cached->rate)
                return *item;

            // Sample format differs - uncache it (we'll reuse this CacheItem).
            notifyRemove(*item);
        }
        else
        {
            item = &insertCacheItem(soundId);
        }

        // Attribute the sample with tracking identifiers.
        cached->soundId = soundId;
        cached->group   = group;

        // Perform resampling if necessary.
        resample(cached->data = M_Malloc(cached->size), cached->bytesPer, cached->rate,
                 data, bytesPer, rate, numSamples, size);

        // Replace the cached sample.
        item->replaceSample(cached.release());

        return *item;
    }

    /**
     * Remove @em all CacheItems and their sample data.
     */
    void removeAll()
    {
        for(Hash &hl : hash)
        {
            while(hl.first)
            {
                removeCacheItem(*hl.first);
            }
        }
    }

    /**
     * Let interested parties know that we're about to remove (uncache) the sample
     * associated with the given cache @a item.
     */
    void notifyRemove(CacheItem &item)
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(SampleRemove, i)
        {
            i->sampleCacheAboutToRemove(item.sample());
        }
    }

    DENG2_PIMPL_AUDIENCE(SampleRemove)
};

DENG2_AUDIENCE_METHOD(SampleCache, SampleRemove)

SampleCache::SampleCache() : d(new Instance(this))
{}

void SampleCache::clear()
{
    d->removeAll();
    d->lastPurge = 0;
}

void SampleCache::maybeRunPurge()
{
    // If no interface for SFX playback is available then we have nothing to do.
    // The assumption being that a manual clear is performed if/when SFX playback
    // availability changes.
    if(!ClientApp::audioSystem().soundPlaybackAvailable())
        return;

    // Is it time for a purge?
    dint const nowTime = Timer_Ticks();
    if(nowTime - d->lastPurge < PURGE_TIME) return;  // No.

    d->lastPurge = nowTime;

    // Count the total size of the cache.
    // Also get rid of all sounds that have timed out.
    dint totalSize  = 0;
    CacheItem *next = nullptr;
    for(Instance::Hash &hash : d->hash)
    for(CacheItem *it = hash.first; it; it = next)
    {
        next = it->next;
        if(nowTime - it->lastUsed() > MAX_CACHE_TICS)
        {
            // This sound hasn't been used in a looong time.
            d->removeCacheItem(*it);
            continue;
        }

        totalSize += it->sample().size + sizeof(*it);
    }

    dint const maxSize = MAX_CACHE_KB * 1024;
    dint lowHits = 0;
    CacheItem *lowest;
    while(totalSize > maxSize)
    {
        /*
         * The cache is too large! Find the stopped sample with the lowest hitcount
         * and get rid of it. Repeat until cache size is within limits or there are
         * no more stopped sounds.
         */
        lowest = 0;
        for(Instance::Hash &hash : d->hash)
        for(CacheItem *it = hash.first; it; it = it->next)
        {
            // If an audio driver is still playing the sample we can't remove it.
            auto const stillPlaying =
                ClientApp::audioSystem().forAllDrivers([&it] (IDriver const &driver)
            {
                return driver.forAllChannels(Channel::Sound, [&it] (Channel const &base)
                {
                    auto &ch = base.as<SoundChannel>();
                    return ch.isPlaying() && ch.samplePtr()->soundId == it->sample().soundId;
                });
            });
            if(stillPlaying) continue;

            // This sample could be removed, let's check the hits.
            if(!lowest || it->hitCount() < lowHits)
            {
                lowest  = it;
                lowHits = it->hitCount();
            }
        }

        // No more samples to remove?
        if(!lowest) break;

        // Stop and uncache this cached sample.
        totalSize -= lowest->sample().size + sizeof(*lowest);
        d->removeCacheItem(*lowest);
    }
}

void SampleCache::info(duint *cacheBytes, duint *sampleCount) const
{
    duint size  = 0;
    duint count = 0;
    for(Instance::Hash &hash : d->hash)
    for(CacheItem *it = hash.first; it; it = it->next)
    {
        size  += it->sample().size;
        count += 1;
    }

    if(cacheBytes)  *cacheBytes  = size;
    if(sampleCount) *sampleCount = count;
}

void SampleCache::hit(dint soundId)
{
    if(CacheItem *found = d->tryFind(soundId))
    {
        found->hit();
    }
}

Sample *SampleCache::cache(dint soundId)
{
    LOG_AS("SampleCache");

    // If no interface for SFX playback is available there is no benefit to caching
    // sound samples that won't be heard.
    /// @todo audio::System should handle this by restricting access. -ds
    if(!ClientApp::audioSystem().soundPlaybackAvailable())
        return nullptr;

    // Ignore invalid sound IDs.
    if(soundId <= 0) return nullptr;

    // Have we already cached this?
    if(CacheItem *existing = d->tryFind(soundId))
        return &existing->sample();

    // Lookup info for this sound.
    sfxinfo_t const *info = Def_GetSoundInfo(soundId);
    if(!info)
    {
        LOG_AUDIO_WARNING("Ignoring sound id:%i (missing sfxinfo_t)") << soundId;
        return nullptr;
    }

    // Attempt to cache this now.
    LOG_AUDIO_VERBOSE("Caching sample '%s' (id:%i)...") << info->id << soundId;

    dint bytesPer = 0;
    dint rate = 0;
    dint numSamples = 0;

    /**
     * Figure out where to get the sample data for this sound. It might be from a
     * data file such as a WAD or external sound resources. The definition and the
     * configuration settings will help us in making the decision.
     */
    void *data = nullptr;

    /// Has an external sound file been defined?
    /// @note Path is relative to the base path.
    if(!Str_IsEmpty(&info->external))
    {
        String searchPath = App_BasePath() / String(Str_Text(&info->external));
        // Try loading.
        data = WAV_Load(searchPath.toUtf8().constData(), &bytesPer, &rate, &numSamples);
        if(data)
        {
            bytesPer /= 8; // Was returned as bits.
        }
    }

    // If external didn't succeed, let's try the default resource dir.
    if(!data)
    {
        /**
         * If the sound has an invalid lumpname, search external anyway. If the
         * original sound is from a PWAD, we won't look for an external resource
         * (probably a custom sound).
         *
         * @todo should be a cvar.
         */
        if(info->lumpNum < 0 || !App_FileSystem().lump(info->lumpNum).container().hasCustom())
        {
            try
            {
                String foundPath = App_FileSystem().findPath(de::Uri(info->lumpName, RC_SOUND),
                                                             RLF_DEFAULT, App_ResourceClass(RC_SOUND));
                foundPath = App_BasePath() / foundPath;  // Ensure the path is absolute.

                data = WAV_Load(foundPath.toUtf8().constData(), &bytesPer, &rate, &numSamples);
                if(data)
                {
                    // Loading was successful.
                    bytesPer /= 8;  // Was returned as bits.
                }
            }
            catch(FS1::NotFoundError const &)
            {}  // Ignore this error.
        }
    }

    // No sample loaded yet?
    if(!data)
    {
        // Try loading from the lump.
        if(info->lumpNum < 0)
        {
            LOG_AUDIO_WARNING("Failed to locate lump resource '%s' for sample '%s'")
                << info->lumpName << info->id;
            return nullptr;
        }

        File1 &lump = App_FileSystem().lump(info->lumpNum);
        if(lump.size() <= 8) return nullptr;

        char hdr[12];
        lump.read((duint8 *)hdr, 0, 12);

        // Is this perhaps a WAV sound?
        if(WAV_CheckFormat(hdr))
        {
            // Load as WAV, then.
            duint8 const *sp = lump.cache();
            data = WAV_MemoryLoad((byte const *) sp, lump.size(), &bytesPer, &rate, &numSamples);
            lump.unlock();

            if(!data)
            {
                // Abort...
                LOG_AUDIO_WARNING("Unknown WAV format in lump '%s'") << info->lumpName;
                return nullptr;
            }

            bytesPer /= 8;
        }
    }

    if(data)  // Loaded!
    {
        // Insert a copy of this into the cache.
        CacheItem &item = d->insert(soundId, data, bytesPer * numSamples, numSamples,
                                    bytesPer, rate, info->group);
        Z_Free(data);
        return &item.sample();
    }

    // Probably an old-fashioned DOOM sample.
    if(info->lumpNum >= 0)
    {
        File1 /*const */&lump = App_FileSystem().lump(info->lumpNum);
        if(lump.size() > 8)
        {
            duint8 hdr[8];
            lump.read(hdr, 0, 8);
            dint head  = DD_SHORT(*(dshort const *) (hdr));
            rate       = DD_SHORT(*(dshort const *) (hdr + 2));
            numSamples = de::max(0, DD_LONG(*(dint const *) (hdr + 4)));
            bytesPer   = 1; // 8-bit.

            if(head == 3 && numSamples > 0 && (unsigned) numSamples <= lump.size() - 8)
            {
                // The sample data can be used as-is - load directly from the lump cache.
                duint8 const *data = lump.cache() + 8;  // Skip the header.

                // Insert a copy of this into the cache.
                CacheItem &item = d->insert(soundId, data, bytesPer * numSamples, numSamples,
                                           bytesPer, rate, info->group);

                lump.unlock();

                return &item.sample();
            }
        }
    }

    LOG_AUDIO_WARNING("Unknown lump '%s' sound format") << info->lumpName;
    return nullptr;
}

}  // namespace audio
