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

#include "dd_types.h"  // lumpnum_t
#include "api_sound.h"
#ifdef __CLIENT__
#  include "audio/channel.h"
#  include "audio/driver.h"
#  include <de/Range>
#  include <de/Record>
#endif
#include <de/String>
#include <de/System>

#define SFX_LOWEST_PRIORITY     ( -1000 )

namespace audio {

#ifdef __CLIENT__
class SampleCache;
#endif

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

#ifdef __CLIENT__

    /**
     * Returns a textual, human-friendly description of the audio system configuration
     * including an active playback interface itemization (suitable for logging, error
     * messages, etc..).
     */
    de::String description() const;

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
     * Convenient method returning the current music playback volume.
     */
    int musicVolume() const;

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
     * sources have been associated with the song. Any song currently playing song is
     * stopped.
     *
     * @param definition  Music definition describing the associated music sources.
     *
     * @return  Non-zero if a song is successfully played.
     */
    int playMusic(de::Record const &definition, bool looped = false);

    int playMusicLump(lumpnum_t lumpNum, bool looped = false);
    int playMusicFile(de::String const &filePath, bool looped = false);
    int playMusicCDTrack(int cdTrack, bool looped = false);

    void updateMusicMidiFont();

public:  // Sound effect playback: ---------------------------------------------------

    /**
     * Convenient method returning the current sound effect playback volume.
     */
    int soundVolume() const;

#endif  // __CLIENT__

    /**
     * Convenient method returning the current sound effect volume attenuation range,
     * in map space units.
     */
    de::Rangei soundVolumeAttenuationRange() const;

#ifdef __CLIENT__

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

#endif  // __CLIENT__

    /**
     * Returns true if the sound is currently playing somewhere in the world. It does
     * not matter if it is audible (or not).
     *
     * @param soundId  @c 0= true if sounds are playing using the specified @a emitter.
     */
    bool soundIsPlaying(int soundId, struct mobj_s *emitter) const;

#ifdef __CLIENT__

    /**
     * Stop all sounds of the given sound @a group. If an emitter is specified, only
     * it's sounds are checked.
     */
    void stopSoundGroup(int group, struct mobj_s *emitter);

    /**
     * Stops all channels that are playing the specified sound.
     *
     * @param soundId      @c 0 = all sounds are stopped.
     * @param emitter      If not @c nullptr, then the channel's emitter mobj must match.
     * @param defPriority  If >= 0, the currently playing sound must have a lower priority
     *                     than this to be stopped. Returns -1 if the sound @a id has
     *                     a lower priority than a currently playing sound.
     *
     * @return  The number of samples stopped.
     */
    int stopSoundWithLowerPriority(int soundId, struct mobj_s *emitter, int defPriority);

#endif  // __CLIENT__

    /**
     * @param soundId  @c 0: stops all sounds originating from the given @a emitter.
     * @param emitter  @c nullptr: stops all sounds with the given @a soundId. Otherwise
     *                 both @a soundId and @a emitter must match.
     * @param flags    @ref soundStopFlags.
     */
    void stopSound(int soundId, struct mobj_s *emitter, int flags = 0);

#ifdef __CLIENT__

    /**
     * Start playing a sound.
     *
     * If @a emitter and @a origin are both @c nullptr, the sound is played in 2D and
     * centered.
     *
     * @param soundIdAndFlags  ID of the sound to play. Flags can be included (DDSF_*).
     * @param emitter          Mobj where the sound originates. May be @c nullptr.
     * @param origin           World coordinates where the sound originate. May be @c nullptr.
     * @param volume           Volume for the sound (0...1).
     *
     * @return  @c true if a sound was started.
     */
    bool playSound(int soundIdAndFlags, struct mobj_s *emitter, coord_t const *origin,
        float volume = 1 /*max volume*/);

    /**
     * The priority of a sound is affected by distance, volume and age.
     */
    float rateSoundPriority(struct mobj_s *emitter, coord_t const *origin, float volume,
        int startTic);

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

    /**
     * Perform playback intialization for Sound Effects and Music.
     * @todo observe App?
     */
    void initPlayback();

    /**
     * Provides immutable access to the sample cache (waveforms).
     */
    SampleCache const &sampleCache() const;

    /// @todo refactor away.
    bool hasChannels();

    /**
     * Provides mutable access to the sound channels.
     */
    Channels &channels() const;

    /**
     * Enabling refresh is simple: the refresh thread is resumed. When disabling refresh,
     * first make sure a new refresh doesn't begin (using allowRefresh). We still have
     * to see if a refresh is being made and wait for it to stop. Then we can suspend
     * the refresh thread.
     */
    void allowSfxRefresh(bool allow = true);

    /// @todo refactor away.
    void requestSfxListenerUpdate();

    /**
     * Lookup the unique identifier associated with the given audio @a driver.
     * @todo refactor away.
     */
    audiodriverid_t toDriverId(Driver const *driver) const;

#endif  // __CLIENT__

    /// @todo Should not be exposed to users of this class. -ds
    void startLogical(int soundIdAndFlags, struct mobj_s *emitter);
    void clearLogical();

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

extern int sfxBits, sfxRate;

#endif  // CLIENT_AUDIO_SYSTEM_H
