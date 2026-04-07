/** @file s_cache.cpp  Sound sample cache.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "audio/s_cache.h"

#include "dd_main.h"  // App_AudioSystem()
#include "def_main.h"  // Def_Get*()
#include "audio/audiosystem.h"

#include <doomsday/filesys/fs_main.h>
#include <doomsday/wav.h>
#include <de/legacy/timer.h>
#include <cstring>

using namespace de;
using namespace res;

#ifdef __SERVER__
#  define BEGIN_COP
#  define END_COP
#endif

namespace audio {

// The cached samples are stored in a hash. When a sample is purged, its data will stay
// in the hash (sample lengths needed by the Logical Sound Manager).
static const dint CACHE_HASH_SIZE  = 64;

static const timespan_t PURGE_TIME = 10 * TICSPERSEC;

// 1 Mb = about 12 sec of 44KHz 16bit sound in the cache.
static const dint MAX_CACHE_KB     = 4096;

// Even one minute of silence is quite a long time during gameplay.
static const dint MAX_CACHE_TICS   = TICSPERSEC * 60 * 4;  // 4 minutes.

#if 0
/**
 * Determines the necessary upsample factor for the given sample @a rate.
 */
static dint upsampleFactor(dint rate)
{
    dint factor = 1;
#ifdef __CLIENT__
    // The (up)sampling factor.
    if (App_AudioSystem().mustUpsampleToSfxRate())
    {
        factor = de::max(1, ::sfxRate / rate);
    }
#else
    DE_UNUSED(rate);
#endif
    return factor;
}

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
static void resample(void *dst, dint dstBytesPer, dint dstRate, const void *src,
    dint srcBytesPer, dint srcRate, dint srcNumSamples, duint srcSize)
{
    DE_ASSERT(src && dst);

    // Let's first check for the easy cases.
    if (dstRate == srcRate)
    {
        if (srcBytesPer == dstBytesPer)
        {
            // A simple copy will suffice.
            std::memcpy(dst, src, srcSize);
        }
        else if (srcBytesPer == 1 && dstBytesPer == 2)
        {
            // Just changing the bytes won't do much good...
            const duchar *sp = (const duchar *) src;
            dshort *dp       = (dshort *) dst;

            for (dint i = 0; i < srcNumSamples; ++i)
            {
                *dp++ = (*sp++ - 0x80) << 8;
            }
        }
        return;
    }

    // 2x resampling.
    if (dstRate == 2 * srcRate)
    {
        if (dstBytesPer == 1)
        {
            // The source has a byte per sample as well.
            const duchar *sp = (const duchar *) src;
            duchar *dp       = (duchar *) dst;

            for (dint i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                *dp++ = *sp;
                *dp++ = (*sp + sp[1]) >> 1;
            }

            // Fill in the last two as well.
            dp[0] = dp[1] = *sp;
        }
        else if (srcBytesPer == 1)
        {
            // Destination is signed 16bit. Source is 8bit.
            const duchar *sp = (const duchar *) src;
            dshort *dp       = (dshort *) dst;

            for (dint i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                const dshort first = U8_S16(*sp);

                *dp++ = first;
                *dp++ = (first + U8_S16(sp[1])) >> 1;
            }

            // Fill in the last two as well.
            dp[0] = dp[1] = U8_S16(*sp);
        }
        else if (srcBytesPer == 2)
        {
            // Destination is signed 16bit. Source is 16bit.
            const dshort *sp = (const dshort *) src;
            dshort *dp       = (dshort *) dst;

            for (dint i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                *dp++ = *sp;
                *dp++ = (*sp + sp[1]) >> 1;
            }

            dp[0] = dp[1] = *sp;
        }
        return;
    }

    // 4x resampling (11Khz => 44KHz only).
    if (dstRate == 4 * srcRate)
    {
        if (dstBytesPer == 1)
        {
            // The source has a byte per sample as well.
            const duchar *sp = (const duchar *) src;
            duchar *dp       = (duchar *) dst;

            for (dint i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                const duchar mid = (*sp + sp[1]) >> 1;

                *dp++ = *sp;
                *dp++ = (*sp + mid) >> 1;
                *dp++ = mid;
                *dp++ = (mid + sp[1]) >> 1;
            }

            // Fill in the last four as well.
            dp[0] = dp[1] = dp[2] = dp[3] = *sp;
        }
        else if (srcBytesPer == 1)
        {
            // Destination is signed 16bit. Source is 8bit.
            const duchar *sp = (const duchar *) src;
            dshort *dp       = (dshort *) dst;

            for (int i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                const dshort first = U8_S16(*sp);
                const dshort last  = U8_S16(sp[1]);
                const dshort mid   = (first + last) >> 1;

                *dp++ = first;
                *dp++ = (first + mid) >> 1;
                *dp++ = mid;
                *dp++ = (mid + last) >> 1;
            }

            // Fill in the last four as well.
            dp[0] = dp[1] = dp[2] = dp[3] = U8_S16(*sp);
        }
        else if (srcBytesPer == 2)
        {
            // Destination is signed 16bit. Source is 16bit.
            const dshort *sp = (const dshort *) src;
            dshort *dp       = (dshort *) dst;

            for (dint i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                const dshort mid = (*sp + sp[1]) >> 1;

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
#endif

/**
 * Prepare the given sound sample @a smp for caching.
 *
 * If the sample @a data is already in the right format, just make a copy of it.
 *
 * If necessary, resample the sound @a data upwards to the minimum resolution and
 * bits (specified in the user Config). (You can play higher resolution sounds
 * than the current setting, but not lower resolution ones.)
 *
 * Converts the sample @a data and writes it to the (M_Malloc() allocated) buffer in @a smp
 * (ownership is given to the sfxsample_t).
 *
 * @param data        Actual sample data.
 * @param size        Size in bytes.
 * @param numSamples  Number of samples.
 * @param bytesPer    Bytes per sample (1 or 2).
 * @param rate        Samples per second.
 */
void configureSample(sfxsample_t &smp, const void *data, duint size,
                     dint numSamples, dint bytesPer, dint rate)
{
    DE_UNUSED(data);
    DE_UNUSED(size);

    zap(smp);
    smp.bytesPer   = bytesPer;
    smp.size       = numSamples * bytesPer;
    smp.rate       = rate;
    smp.numSamples = numSamples;

#if 0
    // Apply the upsample factor.
    const dint rsfactor = upsampleFactor(rate);
    smp.rate       *= rsfactor;
    smp.numSamples *= rsfactor;
    smp.size       *= rsfactor;

    // Resample to 16bit?
    if (::sfxBits == 16 && smp.bytesPer == 1)
    {
        smp.bytesPer = 2;
        smp.size     *= 2;
    }
#endif
}

SfxSampleCache::CacheItem::CacheItem()
    : next(nullptr)
    , prev(nullptr)
    , hits(0)
    , lastUsed(0)
{
    zap(sample);
}

SfxSampleCache::CacheItem::~CacheItem()
{
    // We have ownership of the sample data.
    M_Free(sample.data);
}

void SfxSampleCache::CacheItem::hit()
{
    hits += 1;
    lastUsed = Timer_Ticks();
}

void SfxSampleCache::CacheItem::replaceSample(sfxsample_t &newSample)
{
    hits = 0;

    // Release the existing sample data if any.
    M_Free(sample.data);
    // Replace the sample.
    std::memcpy(&sample, &newSample, sizeof(sample));
}

//---------------------------------------------------------------------------------------

DE_PIMPL(SfxSampleCache)
{
    /**
     * Cached samples are placed in a hash (key: sound id).
     */
    struct Hash {
        CacheItem *first = nullptr;
        CacheItem *last  = nullptr;
    } hash[CACHE_HASH_SIZE];

    dint lastPurge = 0;  ///< Time of the last purge (in game ticks).

    Impl(Public *i) : Base(i) {}
    ~Impl() { removeAll(); }

    /**
     * Find the appropriate hash for the given @a soundId.
     */
    Hash &hashFor(dint soundId)
    {
        return hash[duint( soundId ) % CACHE_HASH_SIZE];
    }

    const Hash &hashFor(dint soundId) const
    {
        return const_cast<Impl *>(this)->hashFor(soundId);
    }

    /**
     * Lookup a CacheItem with the given @a soundId.
     */
    CacheItem *tryFind(dint soundId) const
    {
        for (CacheItem *it = hashFor(soundId).first; it; it = it->next)
        {
            if (it->sample.id == soundId)
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
        if (hash.last)
        {
            hash.last->next = item;
            item->prev = hash.last;
        }
        hash.last = item;

        if (!hash.first)
            hash.first = item;

        return *item;
    }

    void removeCacheItem(CacheItem &item)
    {
#ifdef __CLIENT__
        App_AudioSystem().allowSfxRefresh(false);
#endif

        notifyRemove(item);

        Hash &hash = hashFor(item.sample.id);

        // Unlink the item.
        if (hash.last == &item)
            hash.last = item.prev;
        if (hash.first == &item)
            hash.first = item.next;

        if (item.next)
            item.next->prev = item.prev;
        if (item.prev)
            item.prev->next = item.next;

#ifdef __CLIENT__
        App_AudioSystem().allowSfxRefresh(true);
#endif

        // Free all memory allocated for the item.
        delete &item;
    }
    /**
     * Caches a copy of the given sample. If it's already in the cache and has the
     * same format, nothing is done.
     *
     * @param soundId       Id number of the sound sample.
     * @param data          Actual sample data.
     * @param size          Size in bytes.
     * @param numSamples    Number of samples.
     * @param bytesPer      Bytes per sample (1 or 2).
     * @param rate          Samples per second.
     * @param group         Exclusion group (0, if none).
     *
     * @returns             Ptr to the cached sample. Always valid.
     */
    CacheItem &insert(dint soundId, const void *data, duint size, dint numSamples,
        dint bytesPer, dint rate, dint group)
    {
        sfxsample_t cached;
        configureSample(cached, data, size, numSamples, bytesPer, rate);

        // Have we already cached a comparable sample?
        CacheItem *item = tryFind(soundId);
        if (item)
        {
            // A sample is already in the cache.
            // If the existing sample is in the same format - use it.
            if (cached.bytesPer * 8 == ::sfxBits && cached.rate == ::sfxRate)
                return *item;

            // Sample format differs - uncache it (we'll reuse this CacheItem).
            notifyRemove(*item);
        }
        else
        {
            // Add a new CacheItem for the sample.
            item = &insertCacheItem(soundId);
        }

        // Attribute the sample with tracking identifiers.
        cached.id    = soundId;
        cached.group = group;

#if 0
        // Perform resampling if necessary.
        resample(cached.data = M_Malloc(cached.size), cached.bytesPer, cached.rate,
                 data, bytesPer, rate, numSamples, size);
#endif
        cached.data = M_Malloc(cached.size);
        std::memcpy(cached.data, data, size);

        // Replace the cached sample.
        item->replaceSample(cached);

        return *item;
    }

    /**
     * Remove @em all CacheItems and their sample data.
     */
    void removeAll()
    {
        for (Hash &hl : hash)
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
        DE_NOTIFY_PUBLIC(SampleRemove, i)
        {
            i->sfxSampleCacheAboutToRemove(item.sample);
        }
    }

    DE_PIMPL_AUDIENCE(SampleRemove)
};

DE_AUDIENCE_METHOD(SfxSampleCache, SampleRemove)

SfxSampleCache::SfxSampleCache() : d(new Impl(this))
{}

void SfxSampleCache::clear()
{
    d->removeAll();
    d->lastPurge = 0;
}

void SfxSampleCache::maybeRunPurge()
{
#ifdef __CLIENT__
    // If no interface for SFX playback is available then we have nothing to do.
    // The assumption being that a manual clear is performed if/when SFX playback
    // availability changes.
    if (!App_AudioSystem().sfxIsAvailable()) return;
#endif

    // Is it time for a purge?
    const dint nowTime = Timer_Ticks();
    if (nowTime - d->lastPurge < PURGE_TIME) return;  // No.

    d->lastPurge = nowTime;

    // Count the total size of the cache.
    // Also get rid of all sounds that have timed out.
    dint totalSize  = 0;
    CacheItem *next = nullptr;
    for (Impl::Hash &hash : d->hash)
    for (CacheItem *it = hash.first; it; it = next)
    {
        next = it->next;
        if (nowTime - it->lastUsed > MAX_CACHE_TICS)
        {
            // This sound hasn't been used in a looong time.
            d->removeCacheItem(*it);
            continue;
        }

        totalSize += it->sample.size + sizeof(*it);
    }

    const dint maxSize = MAX_CACHE_KB * 1024;
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
        for (Impl::Hash &hash : d->hash)
        {
            for (CacheItem *it = hash.first; it; it = it->next)
            {
#ifdef __CLIENT__
                // If the sample is playing we won't remove it now.
                if (App_AudioSystem().sfxChannels().isPlaying(it->sample.id))
                    continue;
#endif

                // This sample could be removed, let's check the hits.
                if (!lowest || it->hits < lowHits)
                {
                    lowest  = it;
                    lowHits = it->hits;
                }
            }
        }

        // No more samples to remove?
        if (!lowest) break;

        // Stop and uncache this cached sample.
        totalSize -= lowest->sample.size + sizeof(*lowest);
        d->removeCacheItem(*lowest);
    }
}

void SfxSampleCache::info(duint *cacheBytes, duint *sampleCount)
{
    duint size  = 0;
    duint count = 0;
    for (Impl::Hash &hash : d->hash)
    for (CacheItem *it = hash.first; it; it = it->next)
    {
        size  += it->sample.size;
        count += 1;
    }

    if (cacheBytes)  *cacheBytes  = size;
    if (sampleCount) *sampleCount = count;
}

void SfxSampleCache::hit(dint soundId)
{
    if (CacheItem *found = d->tryFind(soundId))
    {
        found->hit();
    }
}

sfxsample_t *SfxSampleCache::cache(dint soundId)
{
    LOG_AS("SfxSampleCache");

#ifdef __CLIENT__
    // If no interface for SFX playback is available there is no benefit to caching
    // sound samples that won't be heard.
    /// @todo AudioSystem should handle this by restricting access. -ds
    if (!App_AudioSystem().sfxIsAvailable()) return nullptr;
#endif

    // Ignore invalid sound IDs.
    if (soundId <= 0) return nullptr;

    // Have we already cached this?
    if (CacheItem *existing = d->tryFind(soundId))
        return &existing->sample;

    // Lookup info for this sound.
    sfxinfo_t *info = Def_GetSoundInfo(soundId, 0, 0);
    if (!info)
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
    if (!Str_IsEmpty(&info->external))
    {
        String searchPath = App_BasePath() / String(Str_Text(&info->external));
        // Try loading.
        data = WAV_Load(searchPath, &bytesPer, &rate, &numSamples);
        if (data)
        {
            bytesPer /= 8; // Was returned as bits.
        }
    }

    // If external didn't succeed, let's try the default resource dir.
    if (!data)
    {
        /**
         * If the sound has an invalid lumpname, search external anyway. If the
         * original sound is from a PWAD, we won't look for an external resource
         * (probably a custom sound).
         *
         * @todo should be a cvar.
         */
        if (info->lumpNum < 0 || !App_FileSystem().lump(info->lumpNum).container().hasCustom())
        {
            try
            {
                String foundPath = App_FileSystem().findPath(res::Uri(info->lumpName, RC_SOUND),
                                                             RLF_DEFAULT, App_ResourceClass(RC_SOUND));
                foundPath = App_BasePath() / foundPath;  // Ensure the path is absolute.

                data = WAV_Load(foundPath, &bytesPer, &rate, &numSamples);
                if (data)
                {
                    // Loading was successful.
                    bytesPer /= 8;  // Was returned as bits.
                }
            }
            catch (const FS1::NotFoundError &)
            {}  // Ignore this error.
        }
    }

    // No sample loaded yet?
    if (!data)
    {
        // Try loading from the lump.
        if (info->lumpNum < 0)
        {
            LOG_AUDIO_WARNING("Failed to locate lump resource '%s' for sample '%s'")
                << info->lumpName << info->id;
            return nullptr;
        }

        File1 &lump = App_FileSystem().lump(info->lumpNum);
        if (lump.size() <= 8) return nullptr;

        char hdr[12];
        lump.read((duint8 *)hdr, 0, 12);

        // Is this perhaps a WAV sound?
        if (WAV_CheckFormat(hdr))
        {
            // Load as WAV, then.
            const duint8 *sp = lump.cache();
            data = WAV_MemoryLoad((const byte *) sp, lump.size(), &bytesPer, &rate, &numSamples);
            lump.unlock();

            if (!data)
            {
                // Abort...
                LOG_AUDIO_WARNING("Unknown WAV format in lump '%s'") << info->lumpName;
                return nullptr;
            }

            bytesPer /= 8;
        }
    }

    if (data)  // Loaded!
    {
        // Insert a copy of this into the cache.
        CacheItem &item = d->insert(soundId, data, bytesPer * numSamples, numSamples,
                                    bytesPer, rate, info->group);
        Z_Free(data);
        return &item.sample;
    }

    // Probably an old-fashioned DOOM sample.
    dsize lumpLength = 0;
    if (info->lumpNum >= 0)
    {
        File1 &lump = App_FileSystem().lump(info->lumpNum);

        if (lump.size() > 8)
        {
            duint8 hdr[8];
            lump.read(hdr, 0, 8);
            dint head  = DD_SHORT(*(const dshort *) (hdr));
            rate       = DD_SHORT(*(const dshort *) (hdr + 2));
            numSamples = de::max(0, DD_LONG(*(const dint *) (hdr + 4)));
            bytesPer   = 1; // 8-bit.

            if (head == 3 && numSamples > 0 && (unsigned) numSamples <= lumpLength - 8)
            {
                // The sample data can be used as-is - load directly from the lump cache.
                const duint8 *data = lump.cache() + 8;  // Skip the header.

                // Insert a copy of this into the cache.
                CacheItem &item = d->insert(soundId, data, bytesPer * numSamples, numSamples,
                                            bytesPer, rate, info->group);

                lump.unlock();

                return &item.sample;
            }
        }
    }

    LOG_AUDIO_WARNING("Unknown lump '%s' sound format") << info->lumpName;
    return nullptr;
}

}  // namespace audio
