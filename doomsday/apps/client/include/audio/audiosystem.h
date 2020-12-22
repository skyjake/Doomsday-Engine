/** @file audiosystem.h  Audio subsystem.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "dd_types.h"  // lumpnum_t
#include "api_sound.h"
#include "audio/s_cache.h"
#ifdef __CLIENT__
#  include "api_audiod_sfx.h"  // sfxsample_t
#  include "audio/audiodriver.h"
#  include "audio/sfxchannel.h"
#endif
#include <doomsday/audio/audio.h>
#include <de/record.h>
#include <de/string.h>
#include <de/system.h>

#define SFX_LOWEST_PRIORITY     ( -1000 )

/**
 * Client audio subsystem.
 *
 * Singleton: there can only be one instance of the audio system at a time.
 *
 * @ingroup audio
 */
class AudioSystem : public audio::Audio
{
public:
    static AudioSystem &get();

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

public:
    AudioSystem();

    void reinitialize();

    /**
     * Returns a textual, human-friendly description of the audio system configuration
     * including an active playback interface itemization (suitable for logging, error
     * messages, etc..).
     */
    de::String description() const;

    /**
     * Perform playback intialization for Sound Effects and Music.
     * @todo observe App?
     */
    void initPlayback();

#ifdef __CLIENT__

    /**
     * Perform playback deintialization for Sound Effects and Music.
     * @todo observe ClientApp?
     */
    void deinitPlayback();

    /**
     * Stop all channels and music, delete the entire sample cache.
     * @todo observe ClientApp?
     */
    void reset();

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
     * Provides a human-friendly, textual representation of the given music @a source.
     */
    static de::String musicSourceAsText(MusicSource source);

    /**
     * Determines if a @em music playback interface is available.
     */
    bool musicIsAvailable() const;

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
     * is stopped. If the song is already playing, no changes are applied.
     *
     * @param definition  Music definition describing which associated music file to play.
     *
     * @return  Non-zero if the song is successfully played (or already playing).
     */
    int playMusic(const de::Record &definition, bool looped = false);

    int playMusicLump(lumpnum_t lumpNum, bool looped = false);
    int playMusicFile(const de::String &filePath, bool looped = false);
    int playMusicCDTrack(int cdTrack, bool looped = false);

    void updateMusicMidiFont();

public:  // Sound effect playback: ---------------------------------------------------

    /**
     * Determines if a @em sfx playback interface is available.
     */
    bool sfxIsAvailable() const;

    /**
     * Determines whether the active @em SFX interface expects all samples to use the
     * same sampler rate.
     *
     * @return  @c true if resampling is required; otherwise @c false.
     */
    bool mustUpsampleToSfxRate() const;

    struct mobj_s *sfxListener();
    void setSfxListener(struct mobj_s *newListener);

#endif  // __CLIENT__

    /**
     * Returns true if the sound is currently playing somewhere in the world. It doesn't
     * matter if it's audible or not.
     *
     * @param soundId  @c 0= true if any sounds are playing using the specified @a emitter.
     */
    bool soundIsPlaying(int soundId, struct mobj_s *emitter) const;

#ifdef __CLIENT__
    void stopSoundGroup(int group, const struct mobj_s *emitter);
    int stopSoundWithLowerPriority(int soundId, const struct mobj_s *emitter, int defPriority);
#endif

    /**
     * @param soundId  @c 0: stops all sounds originating from the given @a emitter.
     * @param emitter  @c nullptr: stops all sounds with the ID. Otherwise both @a soundId
     *                 and @a emitter must match.
     * @param flags    @ref soundStopFlags.
     */
    void stopSound(int soundId, const struct mobj_s *emitter, int flags = 0) override;

#ifdef __CLIENT__

    /**
     * Used by the high-level sound interface to play sounds on the local system.
     *
     * @param sample    Sample to play (must be stored persistently! No copy is made).
     * @param volume    Volume at which the sample should be played.
     * @param freq      Relative and modifies the sample's rate.
     * @param emitter   If @c nullptr, @a fixedpos is checked for a position. If both
     *                  @a emitter and @a fixedpos are @c nullptr, then the sound is played
     *                  as centered 2D.
     * @param fixedpos  Fixed position where the sound if emitted, or @c nullptr.
     * @param flags     Additional flags (@ref soundPlayFlags).
     *
     * @return  @c true, if a sound is started.
     */
    int playSound(sfxsample_t *sample, float volume, float freq, const struct mobj_s *emitter,
                  coord_t *fixedOrigin, int flags);

    /**
     * The priority of a sound is affected by distance, volume and age.
     */
    float rateSoundPriority(const struct mobj_s *emitter, const coord_t *point, float volume, int startTic);

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

#endif  // __CLIENT__

public:  /// @todo make private:
    void startFrame();
#ifdef __CLIENT__
    void endFrame();
#endif

    void aboutToUnloadMap();
#ifdef __CLIENT__
    void worldMapChanged();
#endif

    /**
     * Provides mutable access to the sound sample cache (waveforms).
     */
    audio::SfxSampleCache /*const*/ &sfxSampleCache() const;

#ifdef __CLIENT__
    /// @todo refactor away.
    bool hasSfxChannels();

    /**
     * Provides mutable access to the sound channels.
     */
    audio::SfxChannels &sfxChannels() const;

    /**
     * Enabling refresh is simple: the refresh thread is resumed. When
     * disabling refresh, first make sure a new refresh doesn't begin (using
     * allowRefresh). We still have to see if a refresh is being made and wait
     * for it to stop. Then we can suspend the refresh thread.
     */
    void allowSfxRefresh(bool allow = true);

    /// @todo refactor away.
    void requestSfxListenerUpdate();

    /**
     * Lookup the unique identifier associated with the given audio @a driver.
     * @todo refactor away.
     */
    audiodriverid_t toDriverId(const AudioDriver *driver) const;

#endif  // __CLIENT__

    /// @todo Should not be exposed to users of this class. -ds
    void startLogical(int soundIdAndFlags, const struct mobj_s *emitter);

private:
    DE_PRIVATE(d)
};

// Music: ---------------------------------------------------------------------------

int Mus_Start(const de::Record &definition, bool looped);
int Mus_StartLump(lumpnum_t lumpNum, bool looped);
int Mus_StartFile(const char *filePath, bool looped);
int Mus_StartCDTrack(int cdTrack, bool looped);

// Sound effects: -------------------------------------------------------------------

extern int soundMinDist, soundMaxDist;
extern int sfxBits, sfxRate;

#ifdef __CLIENT__
extern int sfxVolume, musVolume;

/**
 * Usually the display player.
 */
mobj_t *S_GetListenerMobj();

/**
 * Stop all sounds of the group. If an emitter is specified, only it's sounds are checked.
 */
void S_StopSoundGroup(int group, const struct mobj_s *emitter);

/**
 * Stops all channels that are playing the specified sound.
 *
 * @param soundId      @c 0 = all sounds are stopped.
 * @param emitter      If not @c nullptr, then the channel's emitter mobj must match.
 * @param defPriority  If >= 0, the currently playing sound must have a lower priority
 *                     than this to be stopped. Returns -1 if the sound @a id has a lower
 *                     priority than a currently playing sound.
 *
 * @return  The number of samples stopped.
 */
int S_StopSoundWithLowerPriority(int soundId, struct mobj_s *emitter, ddboolean_t byPriority);

#endif  // __CLIENT__

/**
 * @defgroup soundPlayFlags  Sound Start Flags
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

#endif  // CLIENT_AUDIO_SYSTEM_H
