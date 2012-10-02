/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * s_cache.c: Sound Sample Cache
 *
 * The data is stored using M_Malloc().
 *
 * To play a sound:
 *  1) Figure out the ID of the sound.
 *  2) Call Sfx_Cache() to get a sfxsample_t.
 *  3) Pass the sfxsample_t to Sfx_StartSound().
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_refresh.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// The cached samples are stored in a hash. When a sample is purged, its
// data will stay in the hash (sample lengths needed by the Logical Sound
// Manager).
#define CACHE_HASH_SIZE     (64)

#define PURGE_TIME          (10 * TICSPERSEC)

// Convert an unsigned byte to signed short (for resampling).
#define U8_S16(b)           (((byte)(b) - 0x80) << 8)

// TYPES -------------------------------------------------------------------

typedef struct sfxcache_s {
    struct sfxcache_s* next, *prev;
    int             hits;
    int             lastUsed; // Tic the sample was last hit.
    sfxsample_t     sample;
} sfxcache_t;

typedef struct cachehash_s {
    sfxcache_t*     first, *last;
} cachehash_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void            Sfx_Uncache(sfxcache_t* node);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// 1 Mb = about 12 sec of 44KHz 16bit sound in the cache.
int sfxMaxCacheKB = 4096;

// Even one minute of silence is quite a long time during gameplay.
int sfxMaxCacheTics = TICSPERSEC * 60 * 4; // 4 minutes.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static cachehash_t scHash[CACHE_HASH_SIZE];

// CODE --------------------------------------------------------------------

void Sfx_InitCache(void)
{
    // The cache is empty in the beginning.
    memset(scHash, 0, sizeof(scHash));
}

void Sfx_ShutdownCache(void)
{
    int                 i;

    // Uncache all the samples in the cache.
    for(i = 0; i < CACHE_HASH_SIZE; ++i)
    {
        while(scHash[i].first)
            Sfx_Uncache(scHash[i].first);
    }
}

cachehash_t* Sfx_CacheHash(int id)
{
    return &scHash[(unsigned int) id % CACHE_HASH_SIZE];
}

/**
 * If the sound is cached, return a pointer to it.
 */
sfxcache_t* Sfx_GetCached(int id)
{
    sfxcache_t*         it;

    for(it = Sfx_CacheHash(id)->first; it; it = it->next)
    {
        if(it->sample.id == id)
            return it;
    }

    return NULL;
}

/**
 * Simple linear resampling with possible conversion to 16 bits.
 * The destination sample must be initialized and it must have a large
 * enough buffer. We won't reduce rate or bits here.
 *
 * \note This is not a clean way to resample a sound. If you read about
 * DSP a bit, you'll find out that interpolation adds a lot of extra
 * frequencies in the sample. It should be low-pass filtered after the
 * interpolation.
 */
static void resample(void* dst, int dstBytesPer, int dstRate,
                     const void* src, int srcBytesPer, int srcRate,
                     int srcNumSamples, unsigned int srcSize)
{
    int                 i;

    assert(src);
    assert(dst);

    // Let's first check for the easy cases.
    if(dstRate == srcRate)
    {
        if(srcBytesPer == dstBytesPer)
        {
            // A simple copy will suffice.
            memcpy(dst, src, srcSize);
        }
        else if(srcBytesPer == 1 && dstBytesPer == 2)
        {
            // Just changing the bytes won't do much good...
            const unsigned char* sp = src;
            short*              dp = dst;

            for(i = 0; i < srcNumSamples; ++i)
                *dp++ = (*sp++ - 0x80) << 8;
        }

        return;
    }

    // 2x resampling.
    if(dstRate == 2 * srcRate)
    {
        if(dstBytesPer == 1)
        {   // The source has a byte per sample as well.
            const unsigned char* sp = src;
            unsigned char*      dp = dst;

            for(i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                *dp++ = *sp;
                *dp++ = (*sp + sp[1]) >> 1;
            }

            // Fill in the last two as well.
            dp[0] = dp[1] = *sp;
        }
        else if(srcBytesPer == 1)
        {   // Destination is signed 16bit. Source is 8bit.
            const unsigned char* sp = src;
            short*              dp = dst;
            short               first;

            for(i = 0; i < srcNumSamples - 1; ++i, sp++)
            {
                *dp++ = first = U8_S16(*sp);
                *dp++ = (first + U8_S16(sp[1])) >> 1;
            }

            // Fill in the last two as well.
            dp[0] = dp[1] = U8_S16(*sp);
        }
        else if(srcBytesPer == 2)
        {   // Destination is signed 16bit. Source is 16bit.
            const short*        sp = src;
            short*              dp = dst;

            for(i = 0; i < srcNumSamples - 1; ++i, sp++)
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
        {   // The source has a byte per sample as well.
            const unsigned char* sp = src;
            unsigned char*      dp = dst;
            unsigned char       mid;

            for(i = 0; i < srcNumSamples - 1; ++i, sp++)
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
        {   // Destination is signed 16bit. Source is 8bit.
            const unsigned char* sp = src;
            short*              dp = dst;
            short               first, mid, last;

            for(i = 0; i < srcNumSamples - 1; ++i, sp++)
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
        {   // Destination is signed 16bit. Source is 16bit.
            const short*        sp = src;
            short*              dp = dst;
            short               mid;

            for(i = 0; i < srcNumSamples - 1; ++i, sp++)
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
sfxcache_t* Sfx_CacheInsert(int id, const void* data, unsigned int size,
                            int numSamples, int bytesPer, int rate,
                            int group)
{
    sfxcache_t*         node;
    sfxsample_t         cached;
    cachehash_t*        hash;
    int                 rsfactor;
    void*               buf;

    /**
     * First convert the sample to the minimum resolution and bits, set
     * by sfxRate and sfxBits.
     */

    // The resampling factor.
    rsfactor = sfxRate / rate;
    if(!rsfactor)
        rsfactor = 1;

    /**
     * If the sample is already in the right format, just make a copy of it.
     * If necessary, resample the sound upwards, but not downwards.
     * (You can play higher resolution sounds than the current setting, but
     * not lower resolution ones.)
     */

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
    node = Sfx_GetCached(id);
    if(node)
    {
        // The sound is already in the cache. Is it in the right format?
        if(cached.bytesPer * 8 == sfxBits && cached.rate == sfxRate)
            return node; // This will do.

        // Stop all sounds using this sample (we are going to destroy the
        // existing sample data).
        Sfx_UnloadSoundID(node->sample.id);

        // It's in the wrong format! We'll reuse this node.
        M_Free(node->sample.data);
    }
    else
    {
        // Get a new node and link it in.
        node = M_Calloc(sizeof(sfxcache_t));
        hash = Sfx_CacheHash(id);

        if(hash->last)
        {
            hash->last->next = node;
            node->prev = hash->last;
        }
        hash->last = node;

        if(!hash->first)
            hash->first = node;
    }

    buf = M_Malloc(cached.size);
    // Do the resampling, if necessary.
    resample(buf, cached.bytesPer, cached.rate, data, bytesPer, rate,
             numSamples, size);
    cached.data = buf;

    // Hits keep count of how many times the cached sound has been played.
    // The purger will remove samples with the lowest hitcount first.
    node->hits = 0;
    memcpy(&node->sample, &cached, sizeof(cached));
    return node;
}

void Sfx_Uncache(sfxcache_t* node)
{
    cachehash_t*        hash;

    if(!node)
        return;

    BEGIN_COP;

    // Reset all channels loaded with this sample.
    Sfx_UnloadSoundID(node->sample.id);

    hash = Sfx_CacheHash(node->sample.id);

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

/**
 * Removes the sound with the matching ID from the sound cache.
 */
void Sfx_UncacheID(int id)
{
    sfxcache_t*         node = Sfx_GetCached(id);

    if(!node)
        return; // No such sound is cached.

    Sfx_Uncache(node);
}

/**
 * Called periodically by S_Ticker(). If the cache is too large, stopped
 * samples with the lowest hitcount will be uncached.
 */
void Sfx_PurgeCache(void)
{
    static int          lastPurge = 0;

    int                 totalSize = 0, maxSize = sfxMaxCacheKB * 1024;
    sfxcache_t*         it, *next, *lowest;
    int                 i, lowHits = 0, nowTime = Sys_GetTime();

    if(!sfxAvail)
        return;

    // Is it time for a purge?
    if(nowTime - lastPurge < PURGE_TIME)
        return; // Don't purge yet.

    lastPurge = nowTime;

    // Count the total size of the cache.
    // Also get rid of all sounds that have timed out.
    for(i = 0; i < CACHE_HASH_SIZE; i++)
    {
        for(it = scHash[i].first; it; it = next)
        {
            next = it->next;
            if(nowTime - it->lastUsed > sfxMaxCacheTics)
            {   // This sound hasn't been used in a looong time.
                Sfx_Uncache(it);
                continue;
            }

            totalSize += it->sample.size + sizeof(*it);
        }
    }

    while(totalSize > maxSize)
    {
        /**
         * The cache is too large! Find the stopped sample with the lowest
         * hitcount and get rid of it. Repeat until cache size is within
         * limits or there are no more stopped sounds.
         */
        lowest = NULL;
        for(i = 0; i < CACHE_HASH_SIZE; i++)
        {
            for(it = scHash[i].first; it; it = it->next)
            {
                // If the sample is playing we won't remove it now.
                if(Sfx_CountPlaying(it->sample.id))
                    continue;

                // This sample could be removed, let's check the hits.
                if(!lowest || it->hits < lowHits)
                {
                    lowest = it;
                    lowHits = it->hits;
                }
            }
        }

        if(!lowest)
            break; // No more samples to remove.

        // Stop and uncache this cached sample.
        totalSize -= lowest->sample.size + sizeof(*lowest);
        Sfx_Uncache(lowest);
    }
}

/**
 * @return              Number of bytes and samples cached.
 */
void Sfx_GetCacheInfo(uint* cacheBytes, uint* sampleCount)
{
    sfxcache_t*         it;
    uint                size = 0, count = 0;
    int                 i;

    for(i = 0; i < CACHE_HASH_SIZE; ++i)
    {
        for(it = scHash[i].first; it; it = it->next, count++)
            size += it->sample.size;
    }

    if(cacheBytes)
        *cacheBytes = size;
    if(sampleCount)
        *sampleCount = count;
}

void Sfx_CacheHit(int id)
{
    sfxcache_t*         node = Sfx_GetCached(id);

    if(node)
    {
        node->hits++;
        node->lastUsed = Sys_GetTime();
    }
}

static sfxsample_t* cacheSample(int id, const sfxinfo_t* info)
{
    void* data = NULL;
    int numSamples = 0, bytesPer = 0, rate = 0;
    size_t lumpLength = 0;

    VERBOSE2( Con_Message("Caching sound '%s' (#%i)...\n", info->id, id) )

    /**
     * Figure out where to get the sample data for this sound. It might be
     * from a data file such as a WAD or external sound resources.
     * The definition and the configuration settings will help us in making
     * the decision.
     */

    /// Has an external sound file been defined?
    /// @note Path is relative to the base path.
    if(!Str_IsEmpty(&info->external))
    {
        ddstring_t buf;
        Str_Init(&buf);
        F_PrependBasePath(&buf, &info->external);

        // Try loading.
        data = WAV_Load(Str_Text(&buf), &bytesPer, &rate, &numSamples);
        if(NULL != data)
        {
            bytesPer /= 8; // Was returned as bits.
        }
        Str_Free(&buf);
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
            ddstring_t foundPath; Str_Init(&foundPath);

            if(F_FindResource2(RC_SOUND, info->lumpName, &foundPath) != 0 &&
               (data = WAV_Load(Str_Text(&foundPath), &bytesPer, &rate, &numSamples)))
            {   // Loading was successful!
                bytesPer /= 8; // Was returned as bits.
            }

            Str_Free(&foundPath);
        }
    }

    // No sample loaded yet?
    if(!data)
    {
        abstractfile_t* fsObject;
        int lumpIdx;
        char hdr[12];

        // Try loading from the lump.
        if(info->lumpNum < 0)
        {
            Con_Message("Warning:Sfx_Cache: Failed to locate lump resource '%s' for sound '%s'.\n",
                info->lumpName, info->id);
            return NULL;
        }

        lumpLength = F_LumpLength(info->lumpNum);
        if(lumpLength <= 8) return 0;

        fsObject = F_FindFileForLumpNum2(info->lumpNum, &lumpIdx);
        F_ReadLumpSection(fsObject, lumpIdx, (uint8_t*)hdr, 0, 12);

        // Is this perhaps a WAV sound?
        if(WAV_CheckFormat(hdr))
        {
            // Load as WAV, then.
            const uint8_t* sp = F_CacheLump(fsObject, lumpIdx, PU_APPSTATIC);
            data = WAV_MemoryLoad((const byte*) sp, lumpLength, &bytesPer, &rate, &numSamples);
            F_CacheChangeTag(fsObject, lumpIdx, PU_CACHE);

            if(!data)
            {
                // Abort...
                Con_Message("Warning: Sfx_Cache: Unknown WAV format in lump '%s', aborting.\n", info->lumpName);
                return 0;
            }

            bytesPer /= 8;
        }
    }

    if(data) // Loaded!
    {
        // Insert a copy of this into the cache.
        sfxcache_t* node = Sfx_CacheInsert(id, data, bytesPer * numSamples, numSamples,
                                           bytesPer, rate, info->group);
        Z_Free(data);
        return &node->sample;
    }

    // Probably an old-fashioned DOOM sample.
    lumpLength = F_LumpLength(info->lumpNum);
    if(lumpLength > 8)
    {
        int lumpIdx;
        abstractfile_t* fsObject = F_FindFileForLumpNum2(info->lumpNum, &lumpIdx);
        uint8_t hdr[8];
        int head;

        F_ReadLumpSection(fsObject, lumpIdx, hdr, 0, 8);
        head = SHORT(*(const short*) (hdr));
        rate = SHORT(*(const short*) (hdr + 2));
        numSamples = LONG(*(const int*) (hdr + 4));
        if(numSamples < 0) numSamples = 0;
        bytesPer = 1; // 8-bit.

        if(head == 3 && numSamples > 0 && (unsigned)numSamples <= lumpLength - 8)
        {
            // The sample data can be used as-is - load directly from the lump cache.
            const uint8_t* data = F_CacheLump(fsObject, lumpIdx, PU_APPSTATIC) + 8; // Skip the header.

            // Insert a copy of this into the cache.
            sfxcache_t* node = Sfx_CacheInsert(id, data, bytesPer * numSamples, numSamples,
                                               bytesPer, rate, info->group);

            F_CacheChangeTag(fsObject, lumpIdx, PU_CACHE);

            return &node->sample;
        }
    }

    Con_Message("Warning: Sfx_Cache: Unknown lump '%s' sound format, aborting.\n", info->lumpName);
    return NULL;
}

/**
 * @return              Ptr to the cached copy of the sample (give this ptr
 *                      to Sfx_StartSound), ELSE @c NULL if the soundID is
 *                      invalid.
 */
sfxsample_t* Sfx_Cache(int id)
{
    sfxcache_t*         node;
    sfxinfo_t*          info;

    if(!id || !sfxAvail)
        return NULL;

    // Are we so lucky that the sound is already cached?
    if((node = Sfx_GetCached(id)) != NULL)
    {
        return &node->sample;
    }

    // Get the sound decription.
    info = S_GetSoundInfo(id, NULL, NULL);
    if(!info)
    {
        Con_Message("Warning: Missing SoundInfo for Id %i, ignoring.\n", id);
        return NULL;
    }

    return cacheSample(id, info);
}

/**
 * @return              The length of the sound (in milliseconds).
 */
uint Sfx_GetSoundLength(int id)
{
    sfxsample_t*        sample = Sfx_Cache(id & ~DDSF_FLAG_MASK);

    if(!sample)
    {   // No idea.
        return 0;
    }

    return (1000 * sample->numSamples) / sample->rate;
}
