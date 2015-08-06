/** @file audio/system.h  Audio subsystem.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_AUDIO_SYSTEM_H
#define CLIENT_AUDIO_SYSTEM_H

#include "api_sound.h"
#include "def_main.h"        // sfxinfo_t
#include "world/p_object.h"  // mobj_t
#include <de/System>

namespace audio {

/**
 * Client audio subsystem.
 *
 * Singleton: there can only be one instance of the audio system at a time.
 *
 * @ingroup audio
 */
class System : public de::System
{
public:
    static System &get();

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

public:
    System();

    // Systems observe the passage of time.
    void timeChanged(de::Clock const &) override;

    /**
     * Stop all channels and music, delete the entire sample cache.
     */
    void reset();

    /**
     * Perform playback intialization for Sound Effects and Music.
     * @return  @c true if no error occurred.
     */
    bool initPlayback();

    /**
     * Perform playback deintialization for Sound Effects and Music.
     */
    void deinitPlayback();

public:  /// @todo make private:
    void startFrame();
    void endFrame();

    void aboutToUnloadMap();
    void worldMapChanged();

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

extern int soundMinDist, soundMaxDist;
extern int sfxVolume, musVolume;
extern int sfxBits, sfxRate;
extern byte sfxOneSoundPerEmitter;
extern bool noRndPitch;

/**
 * @defgroup soundPlayFlags Sound Start Flags
 * @ingroup flags
 * @{
 */
#define SF_RANDOM_SHIFT     0x1   ///< Random frequency shift.
#define SF_RANDOM_SHIFT2    0x2   ///< 2x bigger random frequency shift.
#define SF_GLOBAL_EXCLUDE   0x4   ///< Exclude all emitters.
#define SF_NO_ATTENUATION   0x8   ///< Very, very loud...
#define SF_REPEAT           0x10  ///< Repeats until stopped.
#define SF_DONT_STOP        0x20  ///< Sound can't be stopped while playing.
/// @}

/**
 * Gets information about a defined sound. Linked sounds are resolved.
 *
 * @param soundID  ID number of the sound.
 * @param freq     Defined frequency for the sound is returned here. May be @c nullptr.
 * @param volume   Defined volume for the sound is returned here. May be @c nullptr.
 *
 * @return  Sound info (from definitions).
 */
sfxinfo_t *S_GetSoundInfo(int soundID, float *freq, float *volume);

/**
 * @return  @c true if the specified ID is a repeating sound.
 */
dd_bool S_IsRepeating(int idFlags);

/**
 * Usually the display player.
 */
mobj_t *S_GetListenerMobj();

#endif  // CLIENT_AUDIO_SYSTEM_H
