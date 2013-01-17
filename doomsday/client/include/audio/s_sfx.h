/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * s_sfx.h: Sound Effects
 */

#ifndef __DOOMSDAY_SOUND_SFX_H__
#define __DOOMSDAY_SOUND_SFX_H__

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "api_audiod.h"
#include "api_audiod_sfx.h"
#include "map/gamemap.h"

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
    sfxbuffer_t*    buffer;
    mobj_t*         emitter; // Mobj that is emitting the sound.
    coord_t         origin[3]; // Emit from here (synced with emitter).
    float           volume; // Sound volume: 1.0 is max.
    float           frequency; // Frequency adjustment: 1.0 is normal.
    int             startTime; // When was the channel last started?
} sfxchannel_t;

extern boolean sfxAvail;
extern float sfxReverbStrength;
extern int sfxMaxCacheKB, sfxMaxCacheTics;
extern int sfx3D, sfx16Bit, sfxSampleRate;

boolean         Sfx_Init(void);
void            Sfx_Shutdown(void);
void            Sfx_Reset(void);
void            Sfx_AllowRefresh(boolean allow);
void            Sfx_MapChange(void);
void            Sfx_SetListener(mobj_t* mobj);
void            Sfx_StartFrame(void);
void            Sfx_EndFrame(void);
void            Sfx_PurgeCache(void);
void            Sfx_RefreshChannels(void);

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
 * @return              @c true, if a sound is started.
 */
int Sfx_StartSound(sfxsample_t* sample, float volume, float freq,
                   mobj_t* emitter, coord_t* fixedpos, int flags);

int             Sfx_StopSound(int id, mobj_t* emitter);
int             Sfx_StopSoundWithLowerPriority(int id, mobj_t* emitter, ddboolean_t byPriority);
void            Sfx_StopSoundGroup(int group, mobj_t* emitter);
int             Sfx_CountPlaying(int id);
void            Sfx_UnloadSoundID(int id);
void            Sfx_UpdateReverb(void);

void            Sfx_DebugInfo(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
