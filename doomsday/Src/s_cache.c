/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
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
#include "de_system.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// The cached samples are stored in a hash. When a sample is purged, its
// data will stay in the hash (sample lengths needed by the Logical Sound 
// Manager).
#define CACHE_HASH_SIZE		64

#define PURGE_TIME			(10 * TICSPERSEC)

// Convert an unsigned byte to signed short (for resampling).
#define U8_S16(b)			(((byte)(b) - 0x80) << 8)

// TYPES -------------------------------------------------------------------

typedef struct sfxcache_s {
	struct sfxcache_s *next, *prev;
	int hits;
	int lastused;			// Tic the sample was last hit.
	sfxsample_t	sample;
} sfxcache_t;

typedef struct cachehash_s {
	sfxcache_t *first, *last;
} cachehash_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Sfx_Uncache(sfxcache_t *node);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// 1 Mb = about 12 sec of 44KHz 16bit sound in the cache.
int	sfx_max_cache_kb = 4096; 

// Even one minute of silence is quite a long time during gameplay.
int	sfx_max_cache_tics = TICSPERSEC * 60 * 4; // 4 minutes.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static cachehash_t scHash[CACHE_HASH_SIZE];

// CODE --------------------------------------------------------------------

//===========================================================================
// Sfx_InitCache
//===========================================================================
void Sfx_InitCache(void)
{
	// The cache is empty in the beginning.
	memset(scHash, 0, sizeof(scHash));
}

//===========================================================================
// Sfx_ShutdownCache
//===========================================================================
void Sfx_ShutdownCache(void)
{
	int i;

	// Uncache all the samples in the cache.
	for(i = 0; i < CACHE_HASH_SIZE; i++)
	{
		while(scHash[i].first) Sfx_Uncache(scHash[i].first);
	}
}

//===========================================================================
// Sfx_CacheHash
//===========================================================================
cachehash_t *Sfx_CacheHash(int id)
{
	return &scHash[ (unsigned int) id % CACHE_HASH_SIZE ];
}

//===========================================================================
// Sfx_GetCached
//	If the sound is cached, return a pointer to it.
//===========================================================================
sfxcache_t *Sfx_GetCached(int id)
{
	sfxcache_t *it;

	for(it = Sfx_CacheHash(id)->first; it; it = it->next)
	{
		if(it->sample.id == id) return it;
	}
	return NULL;
}

//===========================================================================
// Sfx_Resample
//	Simple linear resampling with possible conversion to 16 bits.
//	The destination sample must be initialized and it must have a large
//	enough buffer. We won't reduce rate or bits here. 
//	
//	NOTE: This is not a clean way to resample a sound. If you read about
//	DSP a bit, you'll find out that interpolation adds a lot of extra
//	frequencies in the sample. It should be low-pass filtered after the
//	interpolation.
//===========================================================================
void Sfx_Resample(sfxsample_t *src, sfxsample_t *dest)
{
	int num = src->numsamples; // == dest->numsamples
	int i;

	// Let's first check for the easy cases.
	if(dest->rate == src->rate)
	{
		if(src->bytesper == dest->bytesper)
		{
			// A simple copy will suffice.
			memcpy(dest->data, src->data, src->size);
		}
		else if(src->bytesper == 1
			&& dest->bytesper == 2)
		{
			// Just changing the bytes won't do much good...
			unsigned char *sp;
			short *dp;
			for(i = 0, sp = src->data, dp = dest->data; i < num; i++)
				*dp++ = (*sp++ - 0x80) << 8;
		}
		return;
	}
	// 2x resampling.
	if(dest->rate == 2 * src->rate)
	{
		if(dest->bytesper == 1)
		{
			// The source has a byte per sample as well.
			unsigned char *sp, *dp;
			for(i = 0, sp = src->data, dp = dest->data; i < num - 1; i++, sp++)
			{
				*dp++ = *sp;
				*dp++ = (*sp + sp[1]) >> 1;
			}
			// Fill in the last two as well.
			dp[0] = dp[1] = *sp;
		}
		else if(src->bytesper == 1) // Destination is signed 16bit.
		{
			// Source is 8bit.
			unsigned char *sp;
			short *dp, first;
			for(i = 0, sp = src->data, dp = dest->data; i < num - 1; i++, sp++)
			{
				*dp++ = first = U8_S16(*sp);
				*dp++ = (first + U8_S16(sp[1])) >> 1;
			}
			// Fill in the last two as well.
			dp[0] = dp[1] = U8_S16(*sp);
		}
		else if(src->bytesper == 2) // Destination is signed 16bit.
		{
			// Source is 16bit.
			short *sp, *dp;
			for(i = 0, sp = src->data, dp = dest->data; i < num - 1; i++, sp++)
			{
				*dp++ = *sp;
				*dp++ = (*sp + sp[1]) >> 1;
			}
			dp[0] = dp[1] = *sp;
		}
		return;
	}
	// 4x resampling (11Khz => 44KHz only).
	if(dest->rate == 4 * src->rate)
	{
		if(dest->bytesper == 1)
		{
			// The source has a byte per sample as well.
			unsigned char *sp, *dp, mid;
			for(i = 0, sp = src->data, dp = dest->data; i < num - 1; i++, sp++)
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
		else if(src->bytesper == 1) // Destination is signed 16bit.
		{
			// Source is 8bit.
			unsigned char *sp;
			short *dp, first, mid, last;
			for(i = 0, sp = src->data, dp = dest->data; i < num - 1; i++, sp++)
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
		else if(src->bytesper == 2) // Destination is signed 16bit.
		{
			// Source is 16bit.
			short *sp, *dp, mid;
			for(i = 0, sp = src->data, dp = dest->data; i < num - 1; i++, sp++)
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

//===========================================================================
// Sfx_CacheInsert
//	Caches a copy of the given sample. If it's already in the cache and has 
//	the same format, nothing is done. Returns a pointers to the cached 
//	sample. Always returns a valid cached sample.
//===========================================================================
sfxcache_t *Sfx_CacheInsert(sfxsample_t *sample)
{
	sfxcache_t *node;
	sfxsample_t cached;
	cachehash_t *hash;
	int rsfactor;

	// First convert the sample to the minimum resolution and bits, set
	// by sfx_rate and sfx_bits.

	// The resampling factor.
	rsfactor = sfx_rate / sample->rate;
	if(!rsfactor) rsfactor = 1;

	// If the sample is already in the right format, just make a copy of it. 
	// If necessary, resample the sound upwards, but not downwards. 
	// (You can play higher resolution sounds than the current setting, but 
	// not lower resolution ones.)

	cached.size = sample->numsamples * sample->bytesper * rsfactor;
	if(sfx_bits == 16 && sample->bytesper == 1)
	{
		cached.bytesper = 2;
		cached.size *= 2; // Will be resampled to 16bit.
	}
	else
	{
		cached.bytesper = sample->bytesper;
	}
	cached.rate = rsfactor * sample->rate;
	cached.numsamples = sample->numsamples * rsfactor;
	cached.id = sample->id;
	cached.group = sample->group;

	// Now check if this kind of a sample already exists.
	node = Sfx_GetCached(sample->id);
	if(node)
	{
		// The sound is already in the cache. Let's see if it's in 
		// the right format.
		if(cached.bytesper*8 == sfx_bits && cached.rate == sfx_rate)
			return node; // This'll do.

		// All sounds using this sample stop playing, because we're
		// going to destroy the existing sample data.
		Sfx_UnloadSoundID(node->sample.id);

		// It's in the wrong format! We'll reuse this node.
		M_Free(node->sample.data);
	}
	else
	{
		// Get a new node and link it in.
		node = M_Calloc(sizeof(sfxcache_t));
		hash = Sfx_CacheHash(sample->id);
		if(hash->last)
		{
			hash->last->next = node;
			node->prev = hash->last;
		}
		hash->last = node;
		if(!hash->first) hash->first = node;
	}

	// Do the resampling, if necessary.
	cached.data = M_Malloc(cached.size);
	Sfx_Resample(sample, &cached);

	// Hits keep count of how many times the cached sound has been played.
	// The purger will remove samples with the lowest hitcount first.
	node->hits = 0;
	memcpy(&node->sample, &cached, sizeof(cached));
	return node;
}

//===========================================================================
// Sfx_Uncache
//===========================================================================
void Sfx_Uncache(sfxcache_t *node)
{
	cachehash_t *hash;

	BEGIN_COP;

	// Reset all channels loaded with this sample.
	Sfx_UnloadSoundID(node->sample.id);

	// Unlink the node.
	hash = Sfx_CacheHash(node->sample.id);
	if(hash->last == node) hash->last = node->prev;
	if(hash->first == node) hash->first = node->next;
	if(node->next) node->next->prev = node->prev;
	if(node->prev) node->prev->next = node->next;

	END_COP;

	// Free all memory allocated for the node.
	M_Free(node->sample.data);
	M_Free(node);
}

//===========================================================================
// Sfx_UncacheID
//	Removes the sound with the matching ID from the sound cache.
//===========================================================================
void Sfx_UncacheID(int id)
{
	sfxcache_t *node = Sfx_GetCached(id);

	if(!node) return; // No such sound is cached.
	Sfx_Uncache(node);	
}

//===========================================================================
// Sfx_PurgeCache
//	Called periodically by S_Ticker(). If the cache is too large, stopped 
//	samples with the lowest hitcount will be uncached.
//===========================================================================
void Sfx_PurgeCache(void)
{
	int	totalsize = 0, maxsize = sfx_max_cache_kb * 1024;
	sfxcache_t *it, *next;
	sfxcache_t *lowest;
	int i, lowhits, nowtime = Sys_GetTime();
	static int lastpurge = 0;

	if(!sfx_avail) return;

	// Is it time for a purge?
	if(nowtime - lastpurge < PURGE_TIME)
	{
		// Don't purge yet.
		return;
	}
	lastpurge = nowtime;

	// Count the total size of the cache.
	// Also get rid of all sounds that have timed out.
	for(i = 0; i < CACHE_HASH_SIZE; i++)
	{
		for(it = scHash[i].first; it; it = next)
		{
			next = it->next;
			if(nowtime - it->lastused > sfx_max_cache_tics)
			{
				// This sound hasn't been used in a looong time.
				Sfx_Uncache(it);
				continue;
			}
			totalsize += it->sample.size + sizeof(*it);
		}
	}

	while(totalsize > maxsize) 
	{
		// The cache is too large! Find the stopped sample with the
		// lowest hitcount and get rid of it. Repeat until cache size
		// is within limits or there are no more stopped sounds.
		lowest = NULL;
		for(i = 0; i < CACHE_HASH_SIZE; i++)
		{
			for(it = scHash[i].first; it; it = it->next)
			{
				// If the sample is playing we won't remove it now.
				if(Sfx_CountPlaying(it->sample.id)) continue;
				
				// This sample could be removed, let's check the hits.
				if(!lowest || it->hits < lowhits)
				{
					lowest = it;
					lowhits = it->hits;
				}
			}
		}
		if(!lowest) break; // No more samples to remove.

		// Stop and uncache this cached sample.
		totalsize -= lowest->sample.size + sizeof(*lowest);
		Sfx_Uncache(lowest);
	}	
}

//===========================================================================
// Sfx_GetCacheInfo
//	Return the number of bytes and samples cached.
//===========================================================================
void Sfx_GetCacheInfo(uint *cacheBytes, uint *sampleCount)
{
	sfxcache_t *it;
	uint size = 0, count = 0;
	int i;

	for(i = 0; i < CACHE_HASH_SIZE; i++)
	{
		for(it = scHash[i].first; it; it = it->next, count++)
			size += it->sample.size;
	}

	if(cacheBytes) *cacheBytes = size;
	if(sampleCount) *sampleCount = count;
}

//===========================================================================
// Sfx_CacheHit
//===========================================================================
void Sfx_CacheHit(int id)
{
	sfxcache_t *node = Sfx_GetCached(id);

	if(node)
	{
		node->hits++;
		node->lastused = Sys_GetTime();
	}
}

//===========================================================================
// Sfx_Cache
//	Returns a pointer to the cached copy of the sample. Give this pointer
//	to Sfx_StartSound(). Returns NULL if the sound ID is invalid.
//===========================================================================
sfxsample_t *Sfx_Cache(int id)
{
	sfxcache_t *node;
	sfxinfo_t *info;
	sfxsample_t samp;
	unsigned short *sp = NULL;
	boolean needFree = false;
	char buf[300];

	if(!sfx_avail) return NULL;

	// Are we so lucky that the sound is already cached?
	if((node = Sfx_GetCached(id)) != NULL)
	{
		// Oh, what a glorious day!
		return &node->sample;
	}

	// Get the sound decription.
	info = S_GetSoundInfo(id, NULL, NULL);

	VERBOSE2( Con_Message("Sfx_Cache: Caching sound %i (%s).\n", 
		id, info->id) );
	
	// Init the sample data. A copy of 'samp' will be placed in the cache.
	memset(&samp, 0, sizeof(samp));
	samp.id = id;	
	samp.group = info->group;
	samp.data = NULL;

	// Figure out where to get the sample data for this sound.
	// It might be from a data file such as a WAD or external sound 
	// resources. The definition and the configuration settings will
	// help us in making the decision.

	// Has an external sound file been defined?
	if(info->external[0])
	{
		// Yes, try loading. Like music, the file name is relative to the
		// base path.
		M_PrependBasePath(info->external, buf);
		if((samp.data = WAV_Load(buf, &samp.bytesper, &samp.rate, 
			&samp.numsamples)))
		{
			// Loading was successful!
			needFree = true;
			samp.bytesper /= 8; // Was returned as bits.
		}
	}
	
	// If external didn't succeed, let's try the default resource dir.
	if(!samp.data)
	{
		// If the sound has an invalid lumpname, search external anyway.
		// If the original sound is from a PWAD, we won't look for an
		// external resource (probably a custom sound).
		if((info->lumpnum < 0 || W_IsFromIWAD(info->lumpnum))
			&& R_FindResource(RC_SFX, info->lumpname, NULL, buf)
			&& (samp.data = WAV_Load(buf, &samp.bytesper, &samp.rate,
				&samp.numsamples)))
		{
			// Loading was successful!
			needFree = true;
			samp.bytesper /= 8; // Was returned as bits.
		}
	}

	// No sample loaded yet?
	if(!samp.data)
	{
		// Try loading from the lump.
		if(info->lumpnum < 0)
		{
			Con_Message("Sfx_Cache: Sound %s has a missing lump: '%s'.\n",
				info->id, info->lumpname);
			Con_Message("  Verifying... The lump number is %i.\n", 
				W_CheckNumForName(info->lumpname));
			return NULL;
		}

		sp = W_CacheLumpNum(info->lumpnum, PU_STATIC);
		
		// Is this perhaps a WAV sound?
		if(WAV_CheckFormat((char*)sp))
		{
			// Load as WAV, then.
			if(!(samp.data = WAV_MemoryLoad((char*)sp, 
				W_LumpLength(info->lumpnum),
				&samp.bytesper, &samp.rate, &samp.numsamples)))
			{
				// Abort...
				Con_Message("Sfx_Cache: WAV data in lump %s is bad.\n",
					info->lumpname);
				W_CacheLumpNum(info->lumpnum, PU_CACHE);
				return NULL;
			}			
			needFree = true;
			samp.bytesper /= 8;
		}
		else
		{
			// Must be an old-fashioned DOOM sample.
			samp.data = sp + 4;		// Eight byte header.
			samp.bytesper = 1;		// 8-bit.
			samp.rate = sp[1];		// Sample rate.
			samp.numsamples = *(int*)(sp + 2);
		}
	}

	samp.size = samp.bytesper * samp.numsamples;

	// Insert a copy of this into the cache.
	node = Sfx_CacheInsert(&samp);

	// We don't need the temporary sample any more, clean up.
	if(sp) W_ChangeCacheTag(info->lumpnum, PU_CACHE);
	if(needFree) Z_Free(samp.data);
	
	return &node->sample;
}

//===========================================================================
// Sfx_GetSoundLength
//	Returns the length of the sound (in milliseconds).
//===========================================================================
uint Sfx_GetSoundLength(int id)
{
	sfxsample_t *sample = Sfx_Cache(id & ~DDSF_FLAG_MASK);

	if(!sample)
	{
		// No idea.
		return 0;
	}
	return (1000 * sample->numsamples) / sample->rate;
}
