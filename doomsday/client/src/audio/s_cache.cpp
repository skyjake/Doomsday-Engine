/** @file s_cache.cpp Sound Sample Cache
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cstring>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_audio.h"
#include "de_misc.h"

using namespace de;

#ifdef __SERVER__
#  define BEGIN_COP
#  define END_COP
#endif

// The cached samples are stored in a hash. When a sample is purged, its
// data will stay in the hash (sample lengths needed by the Logical Sound
// Manager).
#define CACHE_HASH_SIZE     (64)

#define PURGE_TIME          (10 * TICSPERSEC)

// Convert an unsigned byte to signed short (for resampling).
#define U8_S16(b)           (((byte)(b) - 0x80) << 8)

struct SfxCache
{
    SfxCache *next, *prev;
    int hits;
    int lastUsed; // Tic the sample was last hit.
    sfxsample_t sample;
};

struct CacheHash
{
    SfxCache *first, *last;
};

// 1 Mb = about 12 sec of 44KHz 16bit sound in the cache.
int sfxMaxCacheKB = 4096;

// Even one minute of silence is quite a long time during gameplay.
int sfxMaxCacheTics = TICSPERSEC * 60 * 4; // 4 minutes.

static CacheHash scHash[CACHE_HASH_SIZE];

static void Sfx_Uncache(SfxCache *node);

void Sfx_InitCache()
{
    // The cache is empty in the beginning.
    std::memset(scHash, 0, sizeof(scHash));
}

void Sfx_ShutdownCache()
{
    // Uncache all the samples in the cache.
    for(int i = 0; i < CACHE_HASH_SIZE; ++i)
    {
        while(scHash[i].first)
        {
            Sfx_Uncache(scHash[i].first);
        }
    }
}

static CacheHash *Sfx_CacheHash(int id)
{
    return &scHash[uint(id) % CACHE_HASH_SIZE];
}

/**
 * If the sound is cached, return a pointer to it.
 */
static SfxCache *Sfx_GetCached(int id)
{
    for(SfxCache *it = Sfx_CacheHash(id)->first; it; it = it->next)
    {
        if(it->sample.id == id)
            return it;
    }
    return 0;
}

/**
 * Simple linear resampling with possible conversion to 16 bits.
 * The destination sample must be initialized and it must have a large
 * enough buffer. We won't reduce rate or bits here.
 *
 * @note This is not a clean way to resample a sound. If you read about
 * DSP a bit, you'll find out that interpolation adds a lot of extra
 * frequencies in the sample. It should be low-pass filtered after the
 * interpolation.
 */
static void resample(void *dst, int dstBytesPer, int dstRate,
                     void const *src, int srcBytesPer, int srcRate,
                     int srcNumSamples, uint srcSize)
{
    DENG_ASSERT(src);
    DENG_ASSERT(dst);

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
            unsigned char const *sp = (unsigned char const *) src;
            short *dp = (short *) dst;

            for(int i = 0; i < srcNumSamples; ++i)
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
        {   // The source has a byte per sample as well.
            unsigned char const *sp = (unsigned char const *) src;
            unsigned char *dp = (unsigned char *) dst;

            for(int i = 0; i < srcNumSamples - 1; ++i, sp++)
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
            unsigned char const *sp = (unsigned char const *) src;
            short *dp = (short *) dst;
            short first;

            for(int i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                *dp++ = first = U8_S16(*sp);
                *dp++ = (first + U8_S16(sp[1])) >> 1;
            }

            // Fill in the last two as well.
            dp[0] = dp[1] = U8_S16(*sp);
        }
        else if(srcBytesPer == 2)
        {
            // Destination is signed 16bit. Source is 16bit.
            short const *sp = (short const *) src;
            short *dp = (short *) dst;

            for(int i = 0; i < srcNumSamples - 1; ++i, sp++)
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
            unsigned char const *sp = (unsigned char const *) src;
            unsigned char *dp = (unsigned char *) dst;
            unsigned char mid;

            for(int i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                mid = (*sp + sp[1]) >> 1;
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
            unsigned char const *sp = (unsigned char const *) src;
            short *dp = (short *) dst;
            short first, mid, last;

            for(int i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                first = U8_S16(*sp);
                last = U8_S16(sp[1]);
                mid = (first + last) >> 1;
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
            short const *sp = (short const *) src;
            short *dp = (short *) dst;
            short mid;

            for(int i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                mid = (*sp + sp[1]) >> 1;

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

#ifdef __CLIENT__
/**
 * Determines whether the audio SFX driver wants all samples to use the same
 * sampler rate.
 *
 * @return @c true, if resampling is required; otherwise @c false.
 */
static bool sfxMustUpsampleToSfxRate()
{
    int anySampleRateAccepted = 0;

    if(AudioDriver_SFX()->Getv)
    {
        AudioDriver_SFX()->Getv(SFXIP_ANY_SAMPLE_RATE_ACCEPTED, &anySampleRateAccepted);
    }
    return (anySampleRateAccepted? false : true);
}
#endif

/**
 * Caches a copy of the given sample. If it's already in the cache and has
 * the same format, nothing is done.
 *
 * @param id            Id number of the sound sample.
 * @param data          Actual sample data.
 * @param size          Size in bytes.
 * @param numSamples    Number of samples.
 * @param bytesPer      Bytes per sample (1 or 2).
 * @param rate          Samples per second.
 * @param group         Exclusion group (0, if none).
 *
 * @returns             Ptr to the cached sample. Always valid.
 */
static SfxCache *Sfx_CacheInsert(int id, void const *data, uint size, int numSamples,
    int bytesPer, int rate, int group)
{
    int rsfactor = 1;

    /**
     * First convert the sample to the minimum resolution and bits, set
     * by sfxRate and sfxBits.
     */

#ifdef __CLIENT__
    // The (up)resampling factor.
    if(sfxMustUpsampleToSfxRate())
    {
        rsfactor = de::max(1, sfxRate / rate);
    }
#endif

    /**
     * If the sample is already in the right format, just make a copy of it.
     * If necessary, resample the sound upwards, but not downwards.
     * (You can play higher resolution sounds than the current setting, but
     * not lower resolution ones.)
     */

    sfxsample_t cached;
    cached.size = numSamples * bytesPer * rsfactor;

    if(sfxBits == 16 && bytesPer == 1)
    {
        cached.bytesPer = 2;
        cached.size *= 2; // Will be resampled to 16bit.
    }
    else
    {
        cached.bytesPer = bytesPer;
    }

    cached.rate = rsfactor * rate;
    cached.numSamples = numSamples * rsfactor;
    cached.id = id;
    cached.group = group;

    // Check if this kind of a sample already exists.
    SfxCache *node = Sfx_GetCached(id);
    if(node)
    {
        // The sound is already in the cache. Is it in the right format?
        if(cached.bytesPer * 8 == sfxBits && cached.rate == sfxRate)
            return node; // This will do.

#ifdef __CLIENT__
        // Stop all sounds using this sample (we are going to destroy the
        // existing sample data).
        Sfx_UnloadSoundID(node->sample.id);
#endif

        // It's in the wrong format! We'll reuse this node.
        M_Free(node->sample.data);
    }
    else
    {
        // Get a new node and link it in.
        node = reinterpret_cast<SfxCache *>(M_Calloc(sizeof(SfxCache)));

        CacheHash *hash = Sfx_CacheHash(id);
        if(hash->last)
        {
            hash->last->next = node;
            node->prev = hash->last;
        }
        hash->last = node;

        if(!hash->first)
            hash->first = node;
    }

    void *buf = M_Malloc(cached.size);
    // Do the resampling, if necessary.
    resample(buf, cached.bytesPer, cached.rate, data, bytesPer, rate,
             numSamples, size);
    cached.data = buf;

    // Hits keep count of how many times the cached sound has been played.
    // The purger will remove samples with the lowest hitcount first.
    node->hits = 0;
    std::memcpy(&node->sample, &cached, sizeof(cached));
    return node;
}

static void Sfx_Uncache(SfxCache *node)
{
    DENG2_ASSERT(node);

    BEGIN_COP;

#ifdef __CLIENT__
    // Reset all channels loaded with this sample.
    Sfx_UnloadSoundID(node->sample.id);
#endif

    CacheHash *hash = Sfx_CacheHash(node->sample.id);

    // Unlink the node.
    if(hash->last == node)
        hash->last = node->prev;
    if(hash->first == node)
        hash->first = node->next;

    if(node->next)
        node->next->prev = node->prev;
    if(node->prev)
        node->prev->next = node->next;

    END_COP;

    // Free all memory allocated for the node.
    M_Free(node->sample.data);
    M_Free(node);
}

void Sfx_PurgeCache()
{
    static int lastPurge = 0;

#ifdef __CLIENT__
    if(!sfxAvail) return;
#endif

    // Is it time for a purge?
    int nowTime = Timer_Ticks();
    if(nowTime - lastPurge < PURGE_TIME) return; // No.

    lastPurge = nowTime;

    // Count the total size of the cache.
    // Also get rid of all sounds that have timed out.
    int totalSize = 0;
    SfxCache *next = 0;
    for(int i = 0; i < CACHE_HASH_SIZE; i++)
    {
        for(SfxCache *it = scHash[i].first; it; it = next)
        {
            next = it->next;
            if(nowTime - it->lastUsed > sfxMaxCacheTics)
            {
                // This sound hasn't been used in a looong time.
                Sfx_Uncache(it);
                continue;
            }

            totalSize += it->sample.size + sizeof(*it);
        }
    }

    int const maxSize = sfxMaxCacheKB * 1024;
    int lowHits = 0;
    SfxCache *lowest;
    while(totalSize > maxSize)
    {
        /**
         * The cache is too large! Find the stopped sample with the lowest
         * hitcount and get rid of it. Repeat until cache size is within
         * limits or there are no more stopped sounds.
         */
        lowest = 0;
        for(int i = 0; i < CACHE_HASH_SIZE; i++)
        {
            for(SfxCache *it = scHash[i].first; it; it = it->next)
            {
#ifdef __CLIENT__
                // If the sample is playing we won't remove it now.
                if(Sfx_CountPlaying(it->sample.id))
                    continue;
#endif

                // This sample could be removed, let's check the hits.
                if(!lowest || it->hits < lowHits)
                {
                    lowest = it;
                    lowHits = it->hits;
                }
            }
        }

        // No more samples to remove?
        if(!lowest) break;

        // Stop and uncache this cached sample.
        totalSize -= lowest->sample.size + sizeof(*lowest);
        Sfx_Uncache(lowest);
    }
}

void Sfx_GetCacheInfo(uint *cacheBytes, uint *sampleCount)
{
    uint size = 0, count = 0;

    for(int i = 0; i < CACHE_HASH_SIZE; ++i)
    {
        for(SfxCache *it = scHash[i].first; it; it = it->next, count++)
        {
            size += it->sample.size;
        }
    }

    if(cacheBytes)  *cacheBytes  = size;
    if(sampleCount) *sampleCount = count;
}

void Sfx_CacheHit(int id)
{
    SfxCache *node = Sfx_GetCached(id);
    if(node)
    {
        node->hits++;
        node->lastUsed = Timer_Ticks();
    }
}

static sfxsample_t *cacheSample(int id, sfxinfo_t const *info)
{
    LOG_AS("Sfx_Cache");
    LOG_INFO("Caching sample '%s' (#%i)...") << info->id << id;

    int bytesPer = 0, rate = 0, numSamples = 0;

    /**
     * Figure out where to get the sample data for this sound. It might be
     * from a data file such as a WAD or external sound resources.
     * The definition and the configuration settings will help us in making
     * the decision.
     */
    void *data = 0;

    /// Has an external sound file been defined?
    /// @note Path is relative to the base path.
    if(!Str_IsEmpty(&info->external))
    {
        AutoStr *searchPath = AutoStr_NewStd();
        F_PrependBasePath(searchPath, &info->external);

        // Try loading.
        data = WAV_Load(Str_Text(searchPath), &bytesPer, &rate, &numSamples);
        if(data)
        {
            bytesPer /= 8; // Was returned as bits.
        }
    }

    // If external didn't succeed, let's try the default resource dir.
    if(!data)
    {
        /**
         * If the sound has an invalid lumpname, search external anyway.
         * If the original sound is from a PWAD, we won't look for an
         * external resource (probably a custom sound).
         * @todo should be a cvar.
         */
        if(info->lumpNum < 0 || !F_LumpIsCustom(info->lumpNum))
        {
            try
            {
                String foundPath = App_FileSystem().findPath(de::Uri(info->lumpName, RC_SOUND),
                                                             RLF_DEFAULT, DD_ResourceClassById(RC_SOUND));
                foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.

                data = WAV_Load(foundPath.toUtf8().constData(), &bytesPer, &rate, &numSamples);
                if(data)
                {
                    // Loading was successful.
                    bytesPer /= 8; // Was returned as bits.
                }
            }
            catch(FS1::NotFoundError const&)
            {} // Ignore this error.
        }
    }

    // No sample loaded yet?
    if(!data)
    {
        // Try loading from the lump.
        if(info->lumpNum < 0)
        {
            LOG_WARNING("Failed to locate lump resource '%s' for sound '%s'.")
                << info->lumpName << info->id;
            return 0;
        }

        size_t lumpLength = F_LumpLength(info->lumpNum);
        if(lumpLength <= 8) return 0;

        int lumpIdx;
        struct file1_s *file = F_FindFileForLumpNum2(info->lumpNum, &lumpIdx);

        char hdr[12];
        F_ReadLumpSection(file, lumpIdx, (uint8_t *)hdr, 0, 12);

        // Is this perhaps a WAV sound?
        if(WAV_CheckFormat(hdr))
        {
            // Load as WAV, then.
            uint8_t const *sp = F_CacheLump(file, lumpIdx);

            data = WAV_MemoryLoad((byte const *) sp, lumpLength, &bytesPer, &rate, &numSamples);
            F_UnlockLump(file, lumpIdx);

            if(!data)
            {
                // Abort...
                LOG_WARNING("Unknown WAV format in lump '%s', aborting.") << info->lumpName;
                return 0;
            }

            bytesPer /= 8;
        }
    }

    if(data) // Loaded!
    {
        // Insert a copy of this into the cache.
        SfxCache *node = Sfx_CacheInsert(id, data, bytesPer * numSamples, numSamples,
                                           bytesPer, rate, info->group);
        Z_Free(data);
        return &node->sample;
    }

    // Probably an old-fashioned DOOM sample.
    size_t lumpLength = F_LumpLength(info->lumpNum);
    if(lumpLength > 8)
    {
        int lumpIdx;
        struct file1_s *file = F_FindFileForLumpNum2(info->lumpNum, &lumpIdx);

        uint8_t hdr[8];
        F_ReadLumpSection(file, lumpIdx, hdr, 0, 8);
        int head   = SHORT(*(short const *) (hdr));
        rate       = SHORT(*(short const *) (hdr + 2));
        numSamples = de::max(0, LONG(*(int const *) (hdr + 4)));

        bytesPer = 1; // 8-bit.

        if(head == 3 && numSamples > 0 && (unsigned) numSamples <= lumpLength - 8)
        {
            // The sample data can be used as-is - load directly from the lump cache.
            uint8_t const *data = F_CacheLump(file, lumpIdx) + 8; // Skip the header.

            // Insert a copy of this into the cache.
            SfxCache *node = Sfx_CacheInsert(id, data, bytesPer * numSamples, numSamples,
                                               bytesPer, rate, info->group);

            F_UnlockLump(file, lumpIdx);

            return &node->sample;
        }
    }

    LOG_WARNING("Unknown lump '%s' sound format, aborting.") << info->lumpName;
    return 0;
}

sfxsample_t *Sfx_Cache(int id)
{
    LOG_AS("Sfx_Cache");

#ifdef __CLIENT__
    if(!sfxAvail || !id) return 0;
#endif

    // Are we so lucky that the sound is already cached?
    if(SfxCache *node = Sfx_GetCached(id))
    {
        return &node->sample;
    }

    // Get the sound decription.
    if(sfxinfo_t *info = S_GetSoundInfo(id, 0, 0))
    {
        return cacheSample(id, info);
    }

    LOG_WARNING("Missing sfxinfo_t for id:%i, ignoring.") << id;
    return 0;
}

uint Sfx_GetSoundLength(int id)
{
    sfxsample_t *sample = Sfx_Cache(id & ~DDSF_FLAG_MASK);
    if(sample)
    {
        return (1000 * sample->numSamples) / sample->rate;
    }
    return 0;
}
