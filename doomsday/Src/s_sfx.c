
//**************************************************************************
//**
//** S_SFX.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"
#include "sys_audio.h"

// MACROS ------------------------------------------------------------------

#define SFX_MAX_CHANNELS		64
#define SFX_LOWEST_PRIORITY		-1000
#define PURGE_TIME				10					// Seconds.
#define UPDATE_TIME				(2.0/TICSPERSEC)	// Seconds.

// Begin and end macros for Critical OPerations. They are operations 
// that can't be done while a refresh is being made. No refreshing 
// will be done between BEGIN_COP and END_COP.
#define BEGIN_COP		Sfx_AllowRefresh(false)
#define END_COP			Sfx_AllowRefresh(true)

// Convert an unsigned byte to signed short (for resampling).
#define U8_S16(b)		(((byte)(b) - 0x80) << 8)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void		Sfx_3DMode(boolean activate);
void		Sfx_SampleFormat(int new_bits, int new_rate);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean			sfx_avail = false;
sfxdriver_t		*driver = NULL;

int				sfx_max_channels = 16;
int				sfx_dedicated_2d = 4;
int				sfx_3d = false;
float			sfx_reverb_strength = 1;
int				sfx_bits = 8;
int				sfx_rate = 11025;

// 1 Mb = about 12 sec of 44KHz 16bit sound in the cache.
int				sfx_max_cache_kb = 1024; 

// Even one minute of silence is quite a long time during gameplay.
int				sfx_max_cache_tics = TICSPERSEC * 60 * 1; // 1 minute.

// Console variables:
int				sound_3dmode = false;
int				sound_16bit = false;
int				sound_rate = 11025;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int num_channels = 0;
static sfxchannel_t	*channels;
static sfxcache_t scroot;
static mobj_t *listener;
static sector_t *listener_sector = 0;

static int refresh_handle;
static volatile boolean allowrefresh, refreshing;

static byte refmonitor = 0;

// CODE --------------------------------------------------------------------

//===========================================================================
// Sfx_ChannelRefreshThread
//	This is a high-priority thread that periodically checks if the 
//	channels need to be updated with more data. The thread terminates
//	when it notices that the channels have been destroyed.
//	The Sfx driver maintains a 250ms buffer for each channel, which 
//	means the refresh must be done often enough to keep them filled.
//===========================================================================
int Sfx_ChannelRefreshThread(void *parm)
{
	sfxchannel_t *ch;
	int i;

	// We'll continue looping until the Sfx module is shut down.
	while(sfx_avail && channels)
	{
		// The bit is swapped on each refresh (debug info).
		refmonitor ^= 1;

		if(allowrefresh)
		{
			// Do the refresh.
			refreshing = true;
			for(i = 0, ch = channels; i < num_channels; i++, ch++)
			{
				if(!ch->buffer
					|| !(ch->buffer->flags & SFXBF_PLAYING)) continue;
				driver->Refresh(ch->buffer);
			}
			refreshing = false;
			// Let's take a nap.
			Sys_Sleep(100);
		}
		else
		{
			// Refreshing is not allowed, so take a shorter nap while
			// waiting for allowrefresh.
			Sys_Sleep(50);
		}
	}
	// Time to end this thread.
	return 0;
}

//===========================================================================
// Sfx_AllowRefresh
//	Enabling refresh is simple: the refresh thread is resumed.
//	When disabling refresh, first make sure a new refresh doesn't
//	begin (using allowrefresh). We still have to see if a refresh
//	is being made and wait for it to stop. Then we can suspend the 
//	refresh thread.
//===========================================================================
void Sfx_AllowRefresh(boolean allow)
{
	if(!sfx_avail) return;
	if(allowrefresh == allow) return; // No change.
	allowrefresh = allow;
	// If we're denying refresh, let's make sure that if it's currently
	// running, we don't continue until it has stopped.
	if(!allow) while(refreshing) Sys_Sleep(0);
	Sys_SuspendThread(refresh_handle, !allow);
}

//===========================================================================
// Sfx_StopSoundGroup
//	Stop all sounds of the group. If an emitter is specified, only its
//	sounds are checked.
//===========================================================================
void Sfx_StopSoundGroup(int group, mobj_t *emitter)
{
	sfxchannel_t *ch;
	int i;

	if(!sfx_avail) return;
	for(i = 0, ch = channels; i < num_channels; i++, ch++)
	{
		if(!ch->buffer
			|| !(ch->buffer->flags & SFXBF_PLAYING)
			|| ch->buffer->sample->group != group
			|| emitter && ch->emitter != emitter)
			continue;
		// This channel must stop.
		driver->Stop(ch->buffer);
	}
}

//===========================================================================
// Sfx_StopSound
//	Stops all channels that are playing the specified sound. If ID is
//	zero, all sounds are stopped. If emitter is not NULL, then the channel's 
//	emitter mobj must match it.
//===========================================================================
void Sfx_StopSound(int id, mobj_t *emitter)
{
	sfxchannel_t *ch;
	int i;

	if(!sfx_avail) return;
	for(i = 0, ch = channels; i < num_channels; i++, ch++)
	{
		if(!ch->buffer 
			|| !(ch->buffer->flags & SFXBF_PLAYING)
			|| id && ch->buffer->sample->id != id
			|| emitter && ch->emitter != emitter) 
			continue; 
		// This channel must be stopped!
		driver->Stop(ch->buffer);
	}
}

//===========================================================================
// Sfx_IsPlaying
//===========================================================================
int Sfx_IsPlaying(int id, mobj_t *emitter)
{
	sfxchannel_t *ch;
	int i;

	if(!sfx_avail) return false;
	for(i = 0, ch = channels; i < num_channels; i++, ch++)
	{
		if(!ch->buffer 
			|| !(ch->buffer->flags & SFXBF_PLAYING)
			|| ch->emitter != emitter
			|| id && ch->buffer->sample->id != id) continue;

		// Once playing, repeating sounds don't stop.
		if(ch->buffer->flags & SFXBF_REPEAT) return true;

		// Check time. The flag is updated after a slight delay
		// (only at refresh).
		if(Sys_GetTime() - ch->starttime < ch->buffer->sample->numsamples
			/(float)ch->buffer->freq*TICSPERSEC) return true;
	}
	return false;
}

//===========================================================================
// Sfx_UnloadSoundID
//	The specified sample will soon no longer exist. All channel buffers
//	loaded with the sample will be reset.
//===========================================================================
void Sfx_UnloadSoundID(int id)
{
	sfxchannel_t *ch;
	int i;
	
	if(!sfx_avail) return;
	BEGIN_COP;
	for(i = 0, ch = channels; i < num_channels; i++, ch++)
	{
		if(!ch->buffer 
			|| !ch->buffer->sample
			|| ch->buffer->sample->id != id) 
			continue; 
		// Stop and unload.
		driver->Reset(ch->buffer);
	}
	END_COP;
}

//===========================================================================
// Sfx_GetCached
//	If the sound is cached, return a pointer to it.
//===========================================================================
sfxcache_t *Sfx_GetCached(int id)
{
	sfxcache_t *it;

	for(it = scroot.next; it != &scroot; it = it->next)
		if(it->sample.id == id)
			return it;
	return NULL;
}

//===========================================================================
// Sfx_Resample
//	Simple linear resampling with possible conversion to 16 bits.
//	The destination sample must be initialized and it must have a large
//	enough buffer. We won't reduce rate or bits here. 
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
// Sfx_Cache
//	Caches the given sample. If it's already in the cache and has the same
//	format, nothing is done. Returns a pointers to the cached sample.
//	Always returns a valid cached sample.
//===========================================================================
sfxcache_t *Sfx_Cache(sfxsample_t *sample)
{
	sfxcache_t *node;
	sfxsample_t cached;
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
		Z_Free(node->sample.data);
	}
	else
	{
		// Get a new node.
		node = Z_Malloc(sizeof(sfxcache_t), PU_STATIC, 0);
		// Link it in. Make it the first in the cache.
		node->prev = &scroot;
		node->next = scroot.next;
		scroot.next = node->next->prev = node;
	}

	// Do the resampling, if necessary.
	cached.data = Z_Malloc(cached.size, PU_STATIC, 0);
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
	BEGIN_COP;

	// Reset all channels loaded with this sample.
	Sfx_UnloadSoundID(node->sample.id);

	// Unlink the node.
	node->next->prev = node->prev;
	node->prev->next = node->next;

	// Free all memory allocated for the node.
	Z_Free(node->sample.data);
	Z_Free(node);

	END_COP;
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
// Sfx_CountPlaying
//	Returns the number of channels the sound is playing on.
//===========================================================================
int Sfx_CountPlaying(int id)
{
	sfxchannel_t *ch = channels;
	int i = 0, count = 0;
	
	if(!sfx_avail) return 0;
	for(; i < num_channels; i++, ch++)
	{
		if(!ch->buffer						// No buffer.
			|| !ch->buffer->sample			// No loaded sample.
			|| ch->buffer->sample->id != id	// Not the requested sample.
			|| !(ch->buffer->flags & SFXBF_PLAYING)) continue;
		count++;
	}
	return count;
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
	int lowhits, nowtime = Sys_GetTime();

	if(!sfx_avail) return;

	// Count the total size of the cache.
	// Also get rid of all sounds that have timed out.
	for(it = scroot.next; it != &scroot; it = next)
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

	while(totalsize > maxsize) 
	{
		// The cache is too large! Find the stopped sample with the
		// lowest hitcount and get rid of it. Repeat until cache is
		// legally sized or there are no more stopped sounds.
		lowest = NULL;
		for(it = scroot.next; it != &scroot; it = it->next)
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
		if(!lowest) break; // No more samples to remove.

		// Stop and uncache this cached sample.
		totalsize -= lowest->sample.size + sizeof(*lowest);
		Sfx_Uncache(lowest);
	}	
}

//===========================================================================
// Sfx_Priority
//	The priority of a sound is affected by distance, volume and age.
//===========================================================================
float Sfx_Priority(mobj_t *emitter, float *fixpos, float volume, int starttic)
{
	// In five seconds all priority of a sound is gone.
	float timeoff = 1000 * (Sys_GetTime() - starttic) / (5.0f*TICSPERSEC);
	float orig[3];

	if(!listener || !emitter && !fixpos)
	{
		// The sound does not have an origin.
		return 1000*volume - timeoff;
	}
	// The sound has an origin, base the points on distance.
	if(emitter)
	{
		orig[VX] = FIX2FLT( emitter->x );
		orig[VY] = FIX2FLT( emitter->y );
		orig[VZ] = FIX2FLT( emitter->z );
	}
	else
	{
		// No emitter mobj, use the fixed source position.
		memcpy(orig, fixpos, sizeof(orig));
	}
	return 1000*volume 
		- P_MobjPointDistancef(listener, 0, orig)/2
		- timeoff;
}

//===========================================================================
// Sfx_ChannelPriority
//	Calculate priority points for a sound playing on a channel.
//	They are used to determine which sounds can be cancelled by new sounds.
//	Zero is the lowest priority.
//===========================================================================
float Sfx_ChannelPriority(sfxchannel_t *ch)
{
	if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING)) 
		return SFX_LOWEST_PRIORITY;

	if(ch->flags & SFXCF_NO_ORIGIN)
		return Sfx_Priority(0, 0, ch->volume, ch->starttime);

	// ch->pos is set to emitter->xyz during updates.
	return Sfx_Priority(0, ch->pos, ch->volume, ch->starttime);
}

//===========================================================================
// Sfx_GetListenerXYZ
//	Returns the actual 3D coordinates of the listener.
//===========================================================================
void Sfx_GetListenerXYZ(float *pos)
{
	if(!listener) return;

	// FIXME: Make it exactly eye-level! (viewheight)
	pos[VX] = FIX2FLT( listener->x );
	pos[VY] = FIX2FLT( listener->y );
	pos[VZ] = FIX2FLT( listener->z + listener->height - (5<<FRACBITS) );
}

//===========================================================================
// Sfx_ChannelUpdate
//	Updates the channel buffer's properties based on 2D/3D position 
//	calculations. Listener might be NULL. Sounds emitted from the listener
//	object are considered to be inside the listener's head.
//===========================================================================
void Sfx_ChannelUpdate(sfxchannel_t *ch)
{
	sfxbuffer_t *buf = ch->buffer;
	float normdist, dist, pan, angle, vec[3];

	if(!buf) return;

	// Copy the emitter's position (if any), to the pos coord array.
	if(ch->emitter)
	{
		ch->pos[VX] = FIX2FLT( ch->emitter->x );
		ch->pos[VY] = FIX2FLT( ch->emitter->y );
		ch->pos[VZ] = FIX2FLT( ch->emitter->z );
		// If this is a mobj, center the Z pos.
		if(P_IsMobjThinker(ch->emitter->thinker.function))
		{
			// Sounds originate from the center.
			ch->pos[VZ] += FIX2FLT( ch->emitter->height )/2;
		}
	}

	// Frequency is common to both 2D and 3D sounds.
	driver->Set(buf, SFXBP_FREQUENCY, ch->frequency);

	if(buf->flags & SFXBF_3D)
	{
		// Volume is affected only by maxvol.
		driver->Set(buf, SFXBP_VOLUME, ch->volume * sfx_volume/255.0f);
		if(ch->emitter && ch->emitter == listener)
		{
			// Emitted by the listener object. Go to relative position mode
			// and set the position to (0,0,0).
			vec[VX] = vec[VY] = vec[VZ] = 0;
			driver->Set(buf, SFXBP_RELATIVE_MODE, true);
			driver->Setv(buf, SFXBP_POSITION, vec);
		}
		else
		{
			// Use the channel's real position.
			driver->Set(buf, SFXBP_RELATIVE_MODE, false);
			driver->Setv(buf, SFXBP_POSITION, ch->pos);
		}
		// If the sound is emitted by the listener, speed is zero.
		if(ch->emitter
			&& ch->emitter != listener
			&& P_IsMobjThinker(ch->emitter->thinker.function))
		{
			vec[VX] = FIX2FLT( ch->emitter->momx ) * TICSPERSEC;
			vec[VY] = FIX2FLT( ch->emitter->momy ) * TICSPERSEC;
			vec[VZ] = FIX2FLT( ch->emitter->momz ) * TICSPERSEC;
			driver->Setv(buf, SFXBP_VELOCITY, vec); 
		}
		else
		{
			// Not moving.
			vec[VX] = vec[VY] = vec[VZ] = 0;
			driver->Setv(buf, SFXBP_VELOCITY, vec);
		}
	}
	else // This is a 2D buffer.
	{
		if(ch->flags & SFXCF_NO_ORIGIN
			|| ch->emitter && ch->emitter == listener)
		{
			dist = 1;
			pan = 0;
		}
		else
		{
			// Calculate roll-off attenuation. [.125/(.125+x), x=0..1]
			dist = P_MobjPointDistancef(listener, 0, ch->pos);
			if(dist < sound_min_distance
				|| ch->flags & SFXCF_NO_ATTENUATION)
			{
				// No distance attenuation.
				dist = 1;
			}
			else if(dist > sound_max_distance)
			{
				// Can't be heard.
				dist = 0;
			}
			else
			{
				normdist = (dist - sound_min_distance)
					/ (sound_max_distance - sound_min_distance);
				// Apply the linear factor so that at max distance there 
				// really is silence.
				dist = .125f/(.125f + normdist) * (1 - normdist);
			}			
			// And pan, too. Calculate angle from listener to emitter.
			if(listener)
			{
				angle = (R_PointToAngle2(listener->x, listener->y,
					ch->pos[VX] * FRACUNIT,
					ch->pos[VY] * FRACUNIT) 
					- listener->angle) / (float) ANGLE_MAX * 360;
				// We want a signed angle.
				if(angle > 180) angle -= 360;
				// Front half.
				if(angle <= 90 && angle >= -90) 
					pan = -angle/90;
				else 
				{
					// Back half.
					pan = (angle + (angle > 0? -180 : 180)) / 90;
					// Dampen sounds coming from behind.
					dist *= (1 + (pan > 0? pan : -pan))/2;
				}
			}
			else
			{
				// No listener mobj? Can't pan, then.
				pan = 0;
			}
		}		
		driver->Set(buf, SFXBP_VOLUME, ch->volume * dist * sfx_volume/255.0f);
		driver->Set(buf, SFXBP_PAN, pan);
	}
}

//===========================================================================
// Sfx_ListenerUpdate
//===========================================================================
void Sfx_ListenerUpdate(void)
{
	float vec[4];
	int i;
	
	// No volume means no sound.
	if(!sfx_avail || !sfx_3d || !sfx_volume) return;

	// Update the listener mobj.
	listener = S_GetListenerMobj();

	if(listener)
	{
		// Position. At eye-level.
		Sfx_GetListenerXYZ(vec);
		driver->Listenerv(SFXLP_POSITION, vec);

		// Orientation. (0,0) will produce front=(1,0,0) and up=(0,0,1).
		vec[VX] = listener->angle / (float) ANGLE_MAX * 360;
		vec[VY] = listener->dplayer? 
			LOOKDIR2DEG(listener->dplayer->lookdir) : 0;
		driver->Listenerv(SFXLP_ORIENTATION, vec);

		// Velocity. The unit is world distance units per second.
		vec[VX] = FIX2FLT( listener->momx ) * TICSPERSEC;
		vec[VY] = FIX2FLT( listener->momy ) * TICSPERSEC;
		vec[VZ] = FIX2FLT( listener->momz ) * TICSPERSEC;
		driver->Listenerv(SFXLP_VELOCITY, vec);
	
		// Reverb effects. Has the current sector changed?
		if(listener_sector != listener->subsector->sector)
		{
			listener_sector = listener->subsector->sector;
			for(i = 0; i < NUM_REVERB_DATA; i++)
			{
				vec[i] = listener_sector->reverb[i];
				if(i == SRD_VOLUME) vec[i] *= sfx_reverb_strength;
			}
			driver->Listenerv(SFXLP_REVERB, vec);
		}
	}

	// Update all listener properties.
	driver->Listener(SFXLP_UPDATE, 0);
}

//===========================================================================
// Sfx_ListenerNoReverb
//===========================================================================
void Sfx_ListenerNoReverb(void)
{
	float rev[4] = { 0, 0, 0, 0 };

	if(!sfx_avail) return;
	listener_sector = NULL;
	driver->Listenerv(SFXLP_REVERB, rev);
	driver->Listener(SFXLP_UPDATE, 0);
}

//===========================================================================
// Sfx_ChannelStop
//	Stops the sound playing on the channel.
//	Just stopping a buffer doesn't affect refresh.
//===========================================================================
void Sfx_ChannelStop(sfxchannel_t *ch)
{
	if(!ch->buffer) return;
	driver->Stop(ch->buffer);
}

//===========================================================================
// Sfx_GetChannelPriorities
//===========================================================================
void Sfx_GetChannelPriorities(float *prios)
{
	int i;

	for(i = 0; i < num_channels; i++)
		prios[i] = Sfx_ChannelPriority(channels + i);
}

//===========================================================================
// Sfx_ChannelFindVacant
//===========================================================================
sfxchannel_t *Sfx_ChannelFindVacant
	(boolean use3d, int bytes, int rate, int sampleid)
{
	sfxchannel_t *ch;
	int i;

	for(i = 0, ch = channels; i < num_channels; i++, ch++)
	{
		if(!ch->buffer 
			|| ch->buffer->flags & SFXBF_PLAYING
			|| use3d != ((ch->buffer->flags & SFXBF_3D) != 0)
			|| ch->buffer->bytes != bytes
			|| ch->buffer->rate != rate) continue;
		// What about the sample?
		if(sampleid > 0)
		{
			if(!ch->buffer->sample
				|| ch->buffer->sample->id != sampleid) continue;
		}
		else if(!sampleid)
		{
			// We're trying to find a channel with no sample 
			// already loaded.
			if(ch->buffer->sample) continue;			
		}
		// This is perfect, take this!
		return ch;
	}
	return NULL;
}

//===========================================================================
// Sfx_StartSound
//	Used by the high-level sound interface to play sounds on this system.
//	If emitter is NULL, fixedpos is checked for a position. If both emitter
//	and fixedpos are NULL, then the sound is played as centered 2D. Freq is
//	relative and modifies the sample's rate. 
//===========================================================================
void Sfx_StartSound
	(sfxsample_t *sample, float volume, float freq, mobj_t *emitter, 
	 float *fixedpos, int flags)
{
	sfxchannel_t	*ch, *selch, *prioch;
	sfxcache_t		*chsamp;
	ded_sound_t		*def;
	int				i, count, nowtime;
	float			myprio, lowprio;
	boolean			have_channel_prios = false;
	float			channel_prios[SFX_MAX_CHANNELS];
	boolean			play3d = sfx_3d && (emitter || fixedpos);
	
	if(!sfx_avail
		|| sample->id < 1 
		|| sample->id >= defs.count.sounds.num
		|| volume <= 0) return;

	// Cache the given sample into memory. If necessary, the sound is
	// resampled when caching.
	chsamp = Sfx_Cache(sample);

	// Calculate the new sound's priority.
	nowtime = Sys_GetTime();
	myprio = Sfx_Priority(emitter, fixedpos, volume, nowtime);

	// Check that there aren't already too many channels playing this sample.
	def = &defs.sounds[sample->id];
	if(def->channels > 0)
	{
		// The decision to stop channels is based on priorities.
		Sfx_GetChannelPriorities(channel_prios);
		have_channel_prios = true;

		count = Sfx_CountPlaying(sample->id);
		while(count >= def->channels)
		{
			// Stop the lowest priority sound of the playing instances,
			// again noting sounds that are more important than us.
			for(selch = NULL, i = 0, ch = channels; 
				i < num_channels; i++, ch++)
			{
				if(ch->buffer
					&& ch->buffer->flags & SFXBF_PLAYING
					&& ch->buffer->sample->id == sample->id
					&& myprio >= channel_prios[i]
					&& (!selch || channel_prios[i] <= lowprio))
				{
					selch = ch;
					lowprio = channel_prios[i];
				}
			}
			if(!selch) 
			{
				// The new sound can't be played because we were unable
				// to stop enough channels to accommodate the limitation.
				return;
			}
			// Stop this one.
			count--;
			Sfx_ChannelStop(selch);
		}
	}

	// Hitcount tells how many times the cached sound has been used.
	chsamp->hits++;
	chsamp->lastused = nowtime;

	// Pick a channel for the sound. We will do our best to play the sound,
	// cancelling existing ones if need be. The best choice would be a 
	// free channel already loaded with the sample, in the correct format
	// and mode.

	BEGIN_COP;

	// First look through the stopped channels. At this stage we're very
	// picky: only the perfect choice will be good enough.
	selch = Sfx_ChannelFindVacant(play3d, chsamp->sample.bytesper,
		chsamp->sample.rate, chsamp->sample.id);

	// The second step is to look for a vacant channel with any sample,
	// but preferably one with no sample already loaded.
	if(!selch) selch = Sfx_ChannelFindVacant(play3d, chsamp->sample.bytesper,
		chsamp->sample.rate, 0);

	// Then try any non-playing channel in the correct format.
	if(!selch) selch = Sfx_ChannelFindVacant(play3d, chsamp->sample.bytesper,
		chsamp->sample.rate, -1);

	if(!selch)
	{
		// OK, there wasn't a perfect channel. Now we must use a channel
		// with the wrong format or decide which one of the playing ones 
		// gets stopped.

		if(!have_channel_prios)
			Sfx_GetChannelPriorities(channel_prios);

		// All channels with a priority less than or equal to ours
		// can be stopped.
		for(prioch = NULL, i = 0, ch = channels; i < num_channels; i++, ch++)
		{
			if(!ch->buffer
				|| play3d != ((ch->buffer->flags & SFXBF_3D) != 0)) 
				continue; // No buffer or in the wrong mode.
			if(!(ch->buffer->flags & SFXBF_PLAYING))
			{
				// This channel is not playing, just take it!
				selch = ch;
				break;
			}
			// Are we more important than this sound?
			// We want to choose the lowest priority sound.
			if(myprio >= channel_prios[i]
				&& (!prioch || channel_prios[i] <= lowprio))
			{
				prioch = ch;
				lowprio = channel_prios[i];
			}
		}
		// If a good low-priority channel was found, use it.
		if(prioch) 
		{
			selch = prioch;
			Sfx_ChannelStop(selch);
		}
	}
	if(!selch) 
	{
		END_COP;
		return; // We couldn't find a suitable channel. Shame.
	}
	
	// Does our channel need to be reformatted?
	if(selch->buffer->rate != chsamp->sample.rate
		|| selch->buffer->bytes != chsamp->sample.bytesper)
	{
		driver->Destroy(selch->buffer);
		// Create a new buffer with the correct format.
		selch->buffer = driver->Create(play3d? SFXBF_3D : 0,
			chsamp->sample.bytesper * 8, chsamp->sample.rate);
	}

	// Check for the repeat flag.
	if(flags & SF_REPEAT)
		selch->buffer->flags |= SFXBF_REPEAT;
	else
		selch->buffer->flags &= ~SFXBF_REPEAT;

	// Init the channel information.
	selch->flags &= ~(SFXCF_NO_ORIGIN | SFXCF_NO_ATTENUATION);
	selch->volume = volume;
	selch->frequency = freq;
	if(!emitter && !fixedpos)
	{
		selch->flags |= SFXCF_NO_ORIGIN;
		selch->emitter = NULL;
	}
	else
	{
		selch->emitter = emitter;
		if(fixedpos) memcpy(selch->pos, fixedpos, sizeof(selch->pos));
	}
	if(flags & SF_NO_ATTENUATION)
	{
		// The sound can be heard from any distance.
		selch->flags |= SFXCF_NO_ATTENUATION;
	}

	// Load in the sample. Must load prior to setting properties, because
	// the driver might actually create the real buffer only upon loading.
	driver->Load(selch->buffer, &chsamp->sample);	
	
	// Update channel properties.
	Sfx_ChannelUpdate(selch);
	
	// 3D sounds need a few extra properties set up.
	if(play3d)
	{
		// Init the buffer's min/max distances.
		// This is only done once, when the sound is started (i.e. here).
		driver->Set(selch->buffer, SFXBP_MIN_DISTANCE,
			selch->flags & SFXCF_NO_ATTENUATION? 10000 : sound_min_distance);

		driver->Set(selch->buffer, SFXBP_MAX_DISTANCE,
			selch->flags & SFXCF_NO_ATTENUATION? 20000 : sound_max_distance);
	}
	
	// This'll commit all the deferred properties.
	driver->Listener(SFXLP_UPDATE, 0);

	// Start playing.
	driver->Play(selch->buffer);

	END_COP;

	// Take note of the start time.
	selch->starttime = nowtime;
}

//===========================================================================
// Sfx_Update
//	Update channel and listener properties.
//===========================================================================
void Sfx_Update(void)
{
	int i;
	sfxchannel_t *ch;

	// Who is listening?
	// If the display player doesn't have a mobj, no positioning is done.
	listener = S_GetListenerMobj();

	// Update channels.
	for(i = 0, ch = channels; i < num_channels; i++, ch++)
	{
		if(!ch->buffer
			|| !(ch->buffer->flags & SFXBF_PLAYING)) 
			continue; // Not playing sounds on this...
		Sfx_ChannelUpdate(ch);
	}

	// Update listener.
	Sfx_ListenerUpdate();
}

//===========================================================================
// Sfx_StartFrame
//	Periodical routines: channel updates, cache purge, cvar checks.
//===========================================================================
void Sfx_StartFrame(void)
{
	static int old_3dmode = false;
	static int old_16bit = false;
	static int old_rate = 11025;
	static double lastpurge = 0, lastupdate = 0;
	double nowtime = Sys_GetSeconds();

	if(!sfx_avail) return;

	// Tell the driver that the sound frame begins.
	driver->Event(SFXEV_BEGIN);

	// Have there been changes to the cvar settings?
	if(old_3dmode != sound_3dmode) 
	{
		Sfx_3DMode(sound_3dmode != 0);
		old_3dmode = sound_3dmode;
	}

	// Check that the rate is valid.
	if(sound_rate != 11025 && sound_rate != 22050 && sound_rate != 44100)
	{
		Con_Message("sound-rate corrected to 11025.\n");
		sound_rate = 11025;
	}

	// Do we need to change the sample format?
	if(old_16bit != sound_16bit 
		|| old_rate != sound_rate) 
	{
		Sfx_SampleFormat(sound_16bit? 16 : 8, sound_rate);
		old_16bit = sound_16bit;
		old_rate = sound_rate;
	}

	// Should we purge the cache (to conserve memory)?
	if(nowtime - lastpurge >= PURGE_TIME)
	{
		lastpurge = nowtime;
		Sfx_PurgeCache();		
	}

	// Is it time to do a channel update?
	if(nowtime - lastupdate >= UPDATE_TIME)
	{
		lastupdate = nowtime;
		Sfx_Update();	
	}
}

//===========================================================================
// Sfx_EndFrame
//===========================================================================
void Sfx_EndFrame(void)
{
	if(!sfx_avail) return;

	// The sound frame ends.
	driver->Event(SFXEV_END);
}

//===========================================================================
// Sfx_InitDriver
//	Initializes the sfx driver interface.
//	Returns true if successful.
//===========================================================================
boolean Sfx_InitDriver(sfxdriver_e drvid)
{
	switch(drvid)
	{
	case SFXD_DSOUND:
		driver = &sfxd_dsound;
		break;

	case SFXD_A3D:
		if(!(driver = DS_Load("A3D"))) return false;
		break;

	case SFXD_OPENAL:
		if(!(driver = DS_Load("OpenAL"))) return false;
		break;

	case SFXD_COMPATIBLE:
		if(!(driver = DS_Load("Compat"))) return false;
		break;

	default:
		Con_Error("Sfx_Driver: Unknown driver type %i.\n", drvid);
	}

	// Initialize the driver.
	return driver->Init();
}

//===========================================================================
// Sfx_CreateChannels
//	Creates the buffers for the channels. 'num3d' channels out of 
//	num_channels be in 3D, the rest will be 2D. 'bits' and 'rate' specify
//	the default configuration.
//===========================================================================
void Sfx_CreateChannels(int num2d, int bits, int rate)
{
	int	i;
	sfxchannel_t *ch;
	float parm[2];

	// Change the primary buffer's format to match the channel format.
	parm[0] = bits;
	parm[1] = rate;
	driver->Listenerv(SFXLP_PRIMARY_FORMAT, parm);

	// Try to create a buffer for each channel.
	for(i = 0, ch = channels; i < num_channels; i++, ch++)
	{
		ch->buffer = driver->Create(num2d-- > 0? 0 : SFXBF_3D, 
			bits, rate);
		if(!ch->buffer)
		{
			Con_Message("Sfx_CreateChannels: Failed to create "
				"buffer for #%i.\n", i);
			continue;
		}
	}
}

//===========================================================================
// Sfx_DestroyChannels
//	Stop all channels and destroy their buffers.
//===========================================================================
void Sfx_DestroyChannels(void)
{
	int	i;

	BEGIN_COP;
	for(i = 0; i < num_channels; i++)
	{
		Sfx_ChannelStop(channels + i);
		if(channels[i].buffer) driver->Destroy(channels[i].buffer);
		channels[i].buffer = NULL;
	}
	END_COP;
}

//===========================================================================
// Sfx_InitChannels
//===========================================================================
void Sfx_InitChannels(void)
{
	num_channels = sfx_max_channels;

	// The -sfxchan option can be used to change the number of channels.
	if(ArgCheckWith("-sfxchan", 1))
	{
		num_channels = strtol(ArgNext(), 0, 0);
		if(num_channels < 1) num_channels = 1;
		if(num_channels > SFX_MAX_CHANNELS) 
			num_channels = SFX_MAX_CHANNELS;
		Con_Message("Sfx_InitChannels: %i channels.\n", num_channels);
	}
	
	// Allocate and init the channels.
	channels = Z_Malloc(sizeof(*channels) * num_channels, PU_STATIC, 0);
	memset(channels, 0, sizeof(*channels) * num_channels);
	
	// Create channels according to the current mode.
	Sfx_CreateChannels(sfx_3d? sfx_dedicated_2d : num_channels,
		sfx_bits, sfx_rate);
}

//===========================================================================
// Sfx_ShutdownChannels
//	Frees all memory allocated for the channels. 
//===========================================================================
void Sfx_ShutdownChannels(void)
{
	Sfx_DestroyChannels();

	Z_Free(channels);
	channels = NULL;
	num_channels = 0;
}

//===========================================================================
// Sfx_InitCache
//===========================================================================
void Sfx_InitCache(void)
{
	// The cache is empty in the beginning.
	scroot.next = scroot.prev = &scroot;
}

//===========================================================================
// Sfx_ShutdownCache
//===========================================================================
void Sfx_ShutdownCache(void)
{
	// Uncache all the samples in the cache.
	while(scroot.next != &scroot) Sfx_Uncache(scroot.next);
}

//===========================================================================
// Sfx_StartRefresh
//	Start the channel refresh thread. It will stop on its own when it 
//	notices that the rest of the sound system is going down.
//===========================================================================
void Sfx_StartRefresh(void)
{
	refreshing = false;
	allowrefresh = true;
	// Create a high-priority thread for the channel refresh.
	refresh_handle = Sys_StartThread(Sfx_ChannelRefreshThread, NULL, 3);
	if(!refresh_handle)
		Con_Error("Sfx_StartRefresh: Failed to start refresh.\n");
}

//===========================================================================
// Sfx_Init
//	Initialize the Sfx module. This includes setting up the available Sfx
//	drivers and the channels, and initializing the sound cache. Returns
//	true if the module is operational after the init.
//===========================================================================
boolean Sfx_Init(void)
{
	boolean ok;

	// Check if sound has been disabled with a command line option.
	if(ArgExists("-nosfx"))
	{
		Con_Message("Sfx_Init: Disabled.\n");
		return true;
	}

	if(sfx_avail) return true; // Already initialized.

	Con_Message("Sfx_Init: Initializing ");

	// First let's set up the drivers. First we much choose which one we
	// want to use. A3D is only used if the -a3d option is found.
	if(ArgExists("-a3d"))
	{
		Con_Message("A3D...\n");
		ok = Sfx_InitDriver(SFXD_A3D);
	}
	else if(ArgExists("-oal"))
	{
		Con_Message("OpenAL...\n");
		ok = Sfx_InitDriver(SFXD_OPENAL);
	}
	else if(ArgExists("-csd")) // Compatible Sound Driver
	{
		Con_Message("Compatible...\n");
		ok = Sfx_InitDriver(SFXD_COMPATIBLE);
	}
	else
	{
		Con_Message("DirectSound...\n");
		ok = Sfx_InitDriver(SFXD_DSOUND);
	}
	// Did we succeed?
	if(!ok)
	{	
		Con_Message("Sfx_Init: Driver init failed. Sfx is disabled.\n");
		return false;
	}

	// This is based on the scientific calculations that if the Doom
	// marine is 56 units tall, 60 is about two meters.
	driver->Listener(SFXLP_UNITS_PER_METER, 30);
	driver->Listener(SFXLP_DOPPLER, 1);

	// The driver is working, let's create the channels.
	Sfx_InitChannels();

	// Init the sample cache.
	Sfx_InitCache();

	// The Sfx module is now available.
	sfx_avail = true;

	Sfx_ListenerNoReverb();

	// Finally, start the refresh thread.
	Sfx_StartRefresh();
	return true;
}

//===========================================================================
// Sfx_Shutdown
//	Shut down the whole Sfx module: drivers, channel buffers and the cache.
//===========================================================================
void Sfx_Shutdown(void)
{
	if(!sfx_avail) return; // Not initialized.

	// These will stop further refreshing.
	sfx_avail = false;
	allowrefresh = false;

	// Destroy the sample cache.
	Sfx_ShutdownCache();

	// Destroy channels.
	Sfx_ShutdownChannels();

	// Finally, close the driver.
	driver->Shutdown();
	driver = NULL;
}

//===========================================================================
// Sfx_Reset
//	Stop all channels, clear the cache.
//===========================================================================
void Sfx_Reset(void)
{
	int i;

	if(!sfx_avail) return;

	listener_sector = NULL;

	// Stop all channels.
	for(i = 0; i < num_channels; i++) Sfx_ChannelStop(channels + i);

	// Free all samples.
	Sfx_ShutdownCache();
}

//===========================================================================
// Sfx_RecreateChannels
//	Destroys all channels and creates them again.
//===========================================================================
void Sfx_RecreateChannels(void)
{
	Sfx_DestroyChannels();
	Sfx_CreateChannels(sfx_3d? sfx_dedicated_2d : num_channels,
		sfx_bits, sfx_rate);
}

//===========================================================================
// Sfx_3DMode
//	Swaps between 2D and 3D sound modes. Called automatically by 
//	Sfx_StartFrame when cvar changes.
//===========================================================================
void Sfx_3DMode(boolean activate)
{
	if(sfx_3d == activate) return; // No change; do nothing.

	sfx_3d = activate;
	// To make the change effective, re-create all channels.
	Sfx_RecreateChannels();
	// If going to 2D, make sure the reverb is off.
	Sfx_ListenerNoReverb();
}

//===========================================================================
// Sfx_SampleFormat
//	Reconfigures the sample bits and rate. Called automatically by
//	Sfx_StartFrame when changes occur.
//===========================================================================
void Sfx_SampleFormat(int new_bits, int new_rate)
{
	if(sfx_bits == new_bits && sfx_rate == new_rate) 
		return; // No change; do nothing.

	// Set the new buffer format.
	sfx_bits = new_bits;
	sfx_rate = new_rate;
	Sfx_RecreateChannels();

	// The cache just became useless, clear it.
	Sfx_ShutdownCache();
}

//===========================================================================
// Sfx_LevelChange
//	Must be done before the level is changed (from P_SetupLevel, via
//	S_LevelChange).
//===========================================================================
void Sfx_LevelChange(void)
{
	int	i;
	sfxchannel_t *ch;

	for(i = 0, ch = channels; i < num_channels; i++, ch++)
	{
		// Mobjs are about to be destroyed.
		ch->emitter = NULL;
	}

	// Sectors, too, for that matter.
	listener_sector = NULL;
}

//===========================================================================
// Sfx_DebugInfo
//===========================================================================
void Sfx_DebugInfo(void)
{
	int i, lh = FR_TextHeight("W") - 3;
	sfxchannel_t *ch;
	char buf[200];
	int cachesize = 0, ccnt = 0;
	sfxcache_t *it;

	gl.Color3f(1, 1, 0);
	if(!sfx_avail)
	{
		FR_TextOut("Sfx disabled", 0, 0);
		return;
	}

	if(refmonitor) FR_TextOut("!", 0, 0);

	// Sample cache information.
	for(it = scroot.next; it != &scroot; it = it->next, ccnt++)
		cachesize += it->sample.size;
	sprintf(buf, "Cached:%i (%i)", cachesize, ccnt);
	gl.Color3f(1, 1, 1);
	FR_TextOut(buf, 10, 0);

	// Print a line of info about each channel.
	for(i = 0, ch = channels; i < num_channels; i++, ch++)
	{
		if(ch->buffer && ch->buffer->flags & SFXBF_PLAYING)
			gl.Color3f(1, 1, 1);
		else 
			gl.Color3f(1, 1, 0);

		sprintf(buf, "%02i: %c%c%c v=%3.1f f=%3.3f st=%i et=%u",
			i, 
			!(ch->flags & SFXCF_NO_ORIGIN)? 'O' : '.',
			!(ch->flags & SFXCF_NO_ATTENUATION)? 'A' : '.',
			ch->emitter? 'E' : '.',
			ch->volume,
			ch->frequency,
			ch->starttime,
			ch->buffer? ch->buffer->endtime : 0);
		FR_TextOut(buf, 5, lh*(1 + i*2));

		if(!ch->buffer) continue;
		sprintf(buf, "    %c%c%c%c id=%03i/%-8s ln=%05i b=%i rt=%2i bs=%05i "
			"(C%05i/W%05i)",
			ch->buffer->flags & SFXBF_3D? '3' : '.',
			ch->buffer->flags & SFXBF_PLAYING? 'P' : '.',
			ch->buffer->flags & SFXBF_REPEAT? 'R' : '.',
			ch->buffer->flags & SFXBF_RELOAD? 'L' : '.',
			ch->buffer->sample? ch->buffer->sample->id : 0,
			ch->buffer->sample? defs.sounds[ch->buffer->sample->id].id : "",
			ch->buffer->sample? ch->buffer->sample->size : 0,
			ch->buffer->bytes,
			ch->buffer->rate/1000,
			ch->buffer->length, 
			ch->buffer->cursor,
			ch->buffer->written);
		FR_TextOut(buf, 5, lh*(2 + i*2));
	}
}
