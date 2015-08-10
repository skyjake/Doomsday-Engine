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
#ifdef __CLIENT__
#  include "audio/audiodriver.h"
#endif
#include "def_main.h"        // sfxinfo_t
#include "world/p_object.h"  // mobj_t
#ifdef __CLIENT__
#  include <de/Record>
#  include <de/String>
#endif
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
     *
     * @todo make __CLIENT__ only.
     */
    void initPlayback();

    /**
     * Perform playback deintialization for Sound Effects and Music.
     *
     * @todo make __CLIENT__ only.
     */
    void deinitPlayback();

#ifdef __CLIENT__
public:  // Music playback: ----------------------------------------------------------

    /**
     * Music source preference.
     */
    enum MusicSource
    {
        MUSP_MUS,  ///< WAD lump/file.
        MUSP_EXT,  ///< "External" file.
        MUSP_CD    ///< CD track.
    };

    /**
     * Determines if one or more @em music playback interface is available.
     */
    bool musicIsAvailable() const;

    /**
     * Change the @em music volume to @a newVolume (affects all music interfaces).
     */
    void setMusicVolume(float vol);

    /**
     * Determines if @em music is currently playing (on any music interface).
     */
    bool musicIsPlaying() const;

    /**
     * Stop all currently playing @em music, if any (affects all music interfaces).
     */
    void stopMusic();

    /**
     * Pauses or resumes the @em music.
     */
    void pauseMusic(bool doPause = true);

    /**
     * Returns @c true if the currently playing @em music is paused.
     */
    bool musicIsPaused() const;

    /**
     * Start playing a song. The chosen interface depends on what's available and what
     * kind of resources have been associated with the song. Any previously playing song
     * is stopped.
     *
     * @param definition  Music definition describing which associated music file to play.
     *
     * @return  Non-zero if the song is successfully played.
     */
    int playMusic(de::Record const &definition, bool looped = false);

    int playMusicLump(lumpnum_t lumpNum, bool looped = false);
    int playMusicFile(de::String const &filePath, bool looped = false);
    int playMusicCDTrack(int cdTrack, bool looped = false);

    void updateSoundFont();

public:  // Low-level driver interfaces: ---------------------------------------------

    /**
     * Returns the currently active, primary SFX interface. @c nullptr is returned if
     * SFX playback is @em not available.
     */
    audiointerface_sfx_generic_t *sfx() const;
    
    /**
     * Returns the currently active, primary CD playback interface. @c nullptr is returned
     * if CD playback is @em not available.
     *
     * @note  The CD interface is considered to belong in the music aggregate interface
     * and usually does not need to be individually manipulated.
     */
    audiointerface_cd_t *cd() const;

    /**
     * Prints a list of the selected, active interfaces to the log.
     */
    void printAllInterfaces() const;

    /**
     * Returns a textual, human-friendly description of the selected, active interface
     * configuration (suitable for logging, error messages, etc..).
     */
    de::String interfaceDescription() const;

    /**
     * Retrieves the Base interface of the audio driver to which @a anyAudioInterface
     * belongs.
     *
     * @param anyAudioInterface  Pointer to a SFX, Music, or CD interface.
     *                           See @ref sfx(), music(), cd().
     *
     * @return Audio driver interface, or @c nullptr if the none of the loaded drivers
     * match.
     */
    audiodriver_t *interface(void *anyAudioInterface) const;

    de::String interfaceName(void *anyAudioInterface) const;

    audiointerfacetype_t interfaceType(void *anyAudioInterface) const;

#endif  // __CLIENT__

public:  /// @todo make private:
    void startFrame();
    void endFrame();

    void aboutToUnloadMap();
    void worldMapChanged();

#ifdef __CLIENT__
    /**
     * Lookup the unique identifier associated with the given audio @a driver.
     * @todo refactor away.
     */
    audiodriverid_t toDriverId(AudioDriver const *driver) const;
#endif

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

// Sound effects: --------------------------------------------------------------

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

// Music: ----------------------------------------------------------------------

int Mus_Start(de::Record const &definition, bool looped);
int Mus_StartLump(lumpnum_t lumpNum, bool looped);
int Mus_StartFile(char const *filePath, bool looped);
int Mus_StartCDTrack(int cdTrack, bool looped);

#endif  // CLIENT_AUDIO_SYSTEM_H
