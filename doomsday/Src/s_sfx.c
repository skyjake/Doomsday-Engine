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
 * s_sfx.c: Sound Effects
 */

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
#define UPDATE_TIME				(2.0/TICSPERSEC)	// Seconds.

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

// Console variables:
int				sound_3dmode = false;
int				sound_16bit = false;
int				sound_rate = 11025;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int num_channels = 0;
static sfxchannel_t	*channels;
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
//
//	FIXME: Use a real mutex, will you?
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
			Sys_Sleep(200);
		}
		else
		{
			// Refreshing is not allowed, so take a shorter nap while
			// waiting for allowrefresh.
			Sys_Sleep(150);
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
	/*Sys_SuspendThread(refresh_handle, !allow);*/
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
			|| (emitter && ch->emitter != emitter))
			continue;
		// This channel must stop.
		driver->Stop(ch->buffer);
	}
}

//===========================================================================
// Sfx_StopSound
//	Stops all channels that are playing the specified sound. If ID is
//	zero, all sounds are stopped. If emitter is not NULL, then the channel's 
//	emitter mobj must match it. Returns the number of samples stopped.
//===========================================================================
int Sfx_StopSound(int id, mobj_t *emitter)
{
	sfxchannel_t *ch;
	int i, stopCount = 0;

	if(!sfx_avail) return false;
	for(i = 0, ch = channels; i < num_channels; i++, ch++)
	{
		if(!ch->buffer 
		   || !(ch->buffer->flags & SFXBF_PLAYING)
		   || (id && ch->buffer->sample->id != id)
		   || (emitter && ch->emitter != emitter)) 
			continue; 

		// Can it be stopped?
		if(ch->buffer->flags & SFXBF_DONT_STOP) 
		{
			// The emitter might get destroyed...
			ch->emitter = NULL;
			ch->flags |= SFXCF_NO_UPDATE | SFXCF_NO_ORIGIN;
			continue;
		}

		// This channel must be stopped!
		driver->Stop(ch->buffer);
		++stopCount;
	}

	return stopCount;
}

/*
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
*/

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
// Sfx_Priority
//	The priority of a sound is affected by distance, volume and age.
//===========================================================================
float Sfx_Priority(mobj_t *emitter, float *fixpos, float volume, int starttic)
{
	// In five seconds all priority of a sound is gone.
	float timeoff = 1000 * (Sys_GetTime() - starttic) / (5.0f*TICSPERSEC);
	float orig[3];

	if(!listener || (!emitter && !fixpos))
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

	if(!buf || ch->flags & SFXCF_NO_UPDATE) 
		return;

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
		if(ch->flags & SFXCF_NO_ORIGIN ||
		   (ch->emitter && ch->emitter == listener))
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
//	relative and modifies the sample's rate. Returns true if a sound is 
//	started.
//
//	The 'sample' pointer must be persistent. No copying is done here.
//===========================================================================
int Sfx_StartSound
	(sfxsample_t *sample, float volume, float freq, mobj_t *emitter, 
	 float *fixedpos, int flags)
{
	sfxchannel_t	*ch, *selch, *prioch;
	sfxinfo_t		*info;
	int				i, count, nowtime;
	float			myprio, lowprio, channel_prios[SFX_MAX_CHANNELS];
	boolean			have_channel_prios = false;
	boolean			play3d = sfx_3d && (emitter || fixedpos);
	
	if(!sfx_avail
		|| sample->id < 1 
		|| sample->id >= defs.count.sounds.num
		|| volume <= 0) return false;

	// Calculate the new sound's priority.
	nowtime = Sys_GetTime();
	myprio = Sfx_Priority(emitter, fixedpos, volume, nowtime);

	// Check that there aren't already too many channels playing this sample.
	info = sounds + sample->id;
	if(info->channels > 0)
	{
		// The decision to stop channels is based on priorities.
		Sfx_GetChannelPriorities(channel_prios);
		have_channel_prios = true;

		count = Sfx_CountPlaying(sample->id);
		while(count >= info->channels)
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
				return false;
			}
			// Stop this one.
			count--;
			Sfx_ChannelStop(selch);
		}
	}

	// Hitcount tells how many times the cached sound has been used.
	Sfx_CacheHit(sample->id);

	// Pick a channel for the sound. We will do our best to play the sound,
	// cancelling existing ones if need be. The best choice would be a 
	// free channel already loaded with the sample, in the correct format
	// and mode.

	BEGIN_COP;

	// First look through the stopped channels. At this stage we're very
	// picky: only the perfect choice will be good enough.
	selch = Sfx_ChannelFindVacant(play3d, sample->bytesper, sample->rate, 
		sample->id);

	// The second step is to look for a vacant channel with any sample,
	// but preferably one with no sample already loaded.
	if(!selch) 
	{
		selch = Sfx_ChannelFindVacant(play3d, sample->bytesper, 
			sample->rate, 0);
	}

	// Then try any non-playing channel in the correct format.
	if(!selch) 
	{
		selch = Sfx_ChannelFindVacant(play3d, sample->bytesper,
			sample->rate, -1);
	}
	
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
		return false; // We couldn't find a suitable channel. For shame.
	}
	
	// Does our channel need to be reformatted?
	if(selch->buffer->rate != sample->rate
		|| selch->buffer->bytes != sample->bytesper)
	{
		driver->Destroy(selch->buffer);
		// Create a new buffer with the correct format.
		selch->buffer = driver->Create(play3d? SFXBF_3D : 0,
			sample->bytesper * 8, sample->rate);
	}

	// Clear flags.
	selch->buffer->flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);

	// Set buffer flags.
	if(flags & SF_REPEAT)
		selch->buffer->flags |= SFXBF_REPEAT;
	if(flags & SF_DONT_STOP)
		selch->buffer->flags |= SFXBF_DONT_STOP;

	// Init the channel information.
	selch->flags &= ~(SFXCF_NO_ORIGIN | SFXCF_NO_ATTENUATION 
		| SFXCF_NO_UPDATE);
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
	driver->Load(selch->buffer, sample);	
	
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

	// Sound successfully started.
	return true;
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
	static double lastupdate = 0;
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
	Sfx_PurgeCache();

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
	case SFXD_DUMMY:
		driver = &sfxd_dummy;
		break;

	case SFXD_A3D:
		if(!(driver = DS_Load("A3D"))) return false;
		break;

	case SFXD_OPENAL:
		if(!(driver = DS_Load("openal"))) return false;
		break;

	case SFXD_COMPATIBLE:
		if(!(driver = DS_Load("Compat"))) return false;
		break;

	case SFXD_SDL_MIXER:
		if(!(driver = DS_Load("sdlmixer"))) return false;
		break;
		
#ifdef WIN32
	case SFXD_DSOUND:
		driver = &sfxd_dsound;
		break;
#endif

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
	if(isDedicated || ArgExists("-dummy"))
	{
		Con_Message("Dummy...\n");
		ok = Sfx_InitDriver(SFXD_DUMMY);
	}
	else if(ArgExists("-a3d"))
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
	else // The default driver.
	{
#ifdef WIN32
		Con_Message("DirectSound...\n");
		ok = Sfx_InitDriver(SFXD_DSOUND);
#endif
#ifdef UNIX
		Con_Message("SDL_mixer...\n");
		ok = Sfx_InitDriver(SFXD_SDL_MIXER);
#endif
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
		if(ch->emitter)
		{
			// Mobjs are about to be destroyed.
			ch->emitter = NULL;

			// Stop all channels with an origin.
			Sfx_ChannelStop(ch);
		}
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
	uint cachesize, ccnt;

	gl.Color3f(1, 1, 0);
	if(!sfx_avail)
	{
		FR_TextOut("Sfx disabled", 0, 0);
		return;
	}

	if(refmonitor) FR_TextOut("!", 0, 0);

	// Sample cache information.
	Sfx_GetCacheInfo(&cachesize, &ccnt);
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

