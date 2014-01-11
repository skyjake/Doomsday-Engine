/** @file s_sfx.h Sound Effects
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifdef __CLIENT__
#ifndef DENG_CLIENT_SOUND_SFX_H
#define DENG_CLIENT_SOUND_SFX_H

#include "api_audiod.h"
#include "api_audiod_sfx.h"
#include "world/map.h"

#ifdef __cplusplus
extern "C" {
#endif

// Begin and end macros for Critical Operations. They are operations
// that can't be done while a refresh is being made. No refreshing
// will be done between BEGIN_COP and END_COP.
#define BEGIN_COP       Sfx_AllowRefresh(false)
#define END_COP         Sfx_AllowRefresh(true)

// Channel flags.
#define SFXCF_NO_ORIGIN         (0x1) // Sound is coming from a mystical emitter.
#define SFXCF_NO_ATTENUATION    (0x2) // Sound is very, very loud.
#define SFXCF_NO_UPDATE         (0x4) // Channel update is skipped.

typedef struct sfxchannel_s {
    int             flags;
    sfxbuffer_t    *buffer;
    struct mobj_s  *emitter; // Mobj that is emitting the sound.
    coord_t         origin[3]; // Emit from here (synced with emitter).
    float           volume; // Sound volume: 1.0 is max.
    float           frequency; // Frequency adjustment: 1.0 is normal.
    int             startTime; // When was the channel last started?
} sfxchannel_t;

extern dd_bool sfxAvail;
extern float sfxReverbStrength;
extern int sfxMaxCacheKB, sfxMaxCacheTics;
extern int sfx3D, sfx16Bit, sfxSampleRate;

/**
 * Initialize the Sfx module. This includes setting up the available Sfx
 * drivers and the channels, and initializing the sound cache. Returns
 * true if the module is operational after the init.
 */
dd_bool Sfx_Init(void);

/**
 * Shut down the whole Sfx module: drivers, channel buffers and the cache.
 */
void Sfx_Shutdown(void);

/**
 * Stop all channels, clear the cache.
 */
void Sfx_Reset(void);

/**
 * Enabling refresh is simple: the refresh thread is resumed. When
 * disabling refresh, first make sure a new refresh doesn't begin (using
 * allowRefresh). We still have to see if a refresh is being made and wait
 * for it to stop. Then we can suspend the refresh thread.
 */
void Sfx_AllowRefresh(dd_bool allow);

/**
 * Must be done before the map is changed (from P_SetupMap, via
 * S_MapChange).
 */
void Sfx_MapChange(void);

void Sfx_SetListener(struct mobj_s *mobj);

/**
 * Periodical routines: channel updates, cache purge, cvar checks.
 */
void Sfx_StartFrame(void);

void Sfx_EndFrame(void);

/**
 * Called periodically by S_Ticker(). If the cache is too large, stopped
 * samples with the lowest hitcount will be uncached.
 */
void Sfx_PurgeCache(void);

void Sfx_RefreshChannels(void);

/**
 * Used by the high-level sound interface to play sounds on the local system.
 *
 * @param sample        Sample to play. Ptr must be stored persistently!
 *                      No copying is done here.
 * @param volume        Volume at which the sample should be played.
 * @param freq          Relative and modifies the sample's rate.
 * @param emitter       If @c NULL, @a fixedpos is checked for a position.
 *                      If both @a emitter and @a fixedpos are @c NULL, then
 *                      the sound is played as centered 2D.
 * @param fixedpos      Fixed position where the sound if emitted, or @c NULL.
 * @param flags         Additional flags (@ref soundPlayFlags).
 *
 * @return  @c true, if a sound is started.
 */
int Sfx_StartSound(sfxsample_t *sample, float volume, float freq,
                   struct mobj_s *emitter, coord_t *fixedpos, int flags);

int Sfx_StopSound(int id, struct mobj_s *emitter);

/**
 * Stops all channels that are playing the specified sound.
 *
 * @param id            @c 0 = all sounds are stopped.
 * @param emitter       If not @c NULL, then the channel's emitter mobj
 *                      must match it.
 * @param defPriority   If >= 0, the currently playing sound must have
 *                      a lower priority than this to be stopped. Returns -1
 *                      if the sound @a id has a lower priority than a
 *                      currently playing sound.
 *
 * @return  The number of samples stopped.
 */
int Sfx_StopSoundWithLowerPriority(int id, struct mobj_s *emitter, ddboolean_t byPriority);

/**
 * Stop all sounds of the group. If an emitter is specified, only it's
 * sounds are checked.
 */
void Sfx_StopSoundGroup(int group, struct mobj_s *emitter);

/**
 * @return  The number of channels the sound is playing on.
 */
int Sfx_CountPlaying(int id);

/**
 * The specified sample will soon no longer exist. All channel buffers
 * loaded with the sample will be reset.
 */
void Sfx_UnloadSoundID(int id);

/**
 * Requests listener reverb update at the end of the frame.
 */
void Sfx_UpdateReverb(void);

void Sfx_DebugInfo(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DENG_CLIENT_SOUND_SFX_H
#endif // __CLIENT__
