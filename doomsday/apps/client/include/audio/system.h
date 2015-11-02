/** @file audio/system.h  System module for audio playback.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENT_AUDIO_SYSTEM_H
#define CLIENT_AUDIO_SYSTEM_H

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "audio/stage.h"

#include "dd_types.h"        // lumpnum_t
#include "SettingsRegister"
#include "world/p_object.h"
#include <de/Error>
#include <de/Observers>
#include <de/Record>
#include <de/String>
#include <de/System>
#include <functional>

namespace audio {

/**
 * Stages provide the means for concurrent playback in logically independent contexts.
 */
enum StageId
{
    /// The "world" sound stage supports playing sounds that originate from world/map
    /// space SoundEmitters, with (optional) distance based volume attenuation and/or
    /// environmental audio effects.
    WorldStage,

    /// The "local" sound stage is a simpler context intended for playing sounds with
    /// no emitters, no volume attenuation, or most other features implemented for the
    /// WorldStage. This context is primarily intended for playing UI sounds.
    LocalStage,

    StageCount
};

/**
 * Symbolic music source identifiers.
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
de::String MusicSourceAsText(MusicSource source);

class IDriver;
class Mixer;
class SampleCache;

/**
 * Client audio subsystem.
 *
 * @ingroup audio
 */
class System : public de::System
{
public:
    /// Notified when a new audio frame begins.
    DENG2_DEFINE_AUDIENCE2(FrameBegins,    void systemFrameBegins(System &system))

    /// Notified when the current audio frame ends.
    DENG2_DEFINE_AUDIENCE2(FrameEnds,      void systemFrameEnds(System &system))

    /// Notified whenever a MIDI font change occurs.
    DENG2_DEFINE_AUDIENCE2(MidiFontChange, void systemMidiFontChanged(de::String const &newMidiFontPath))

public:
    /**
     * Instantiate the singleton audio::System instance.
     */
    System();

    /**
     * Returns the singleton audio::System instance.
     */
    static System &get();

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * Provides access to the settings register of this module (cvars etc...).
     */
    SettingsRegister &settings();

    /**
     * Returns a textual, human-friendly description of the audio system configuration
     * including an active playback interface itemization (suitable for logging, error
     * messages, etc..).
     */
    de::String description() const;

    /**
     * Determines the necessary upsample factor for the given waveform sample @a rate.
     */
    de::dint upsampleFactor(de::dint rate) const;

    /**
     * Provides access to the channel Mixer.
     */
    Mixer /*const*/ &mixer() const;

    /**
     * Provides access to the waveform asset cache.
     */
    SampleCache &sampleCache() const;

    /**
     * Provides access to the soundstages (FYI).
     *
     * @param stageId  Unique identifier of the Stage to locate.
     */
    Stage /*const*/ &stage(StageId stageId) const;

    inline Stage /*const*/ &localStage() const { return stage(LocalStage); }
    inline Stage /*const*/ &worldStage() const { return stage(WorldStage); }

public:  //- Music playback: ------------------------------------------------------------

    /**
     * Returns @c true if one or more interface for audible @em music playback is
     * available on the local system.
     *
     * @see soundPlaybackAvailable()
     */
    bool musicPlaybackAvailable() const;

    /**
     * Convenient method returning the current music playback volume.
     *
     * @see soundVolume()
     */
    de::dint musicVolume() const;

    /**
     * Returns true if @em music is currently playing (on any music interface). It does
     * not matter if it is audible (or not).
     *
     * @see musicIsPaused(), playMusic()
     */
    bool musicIsPlaying() const;

    /**
     * Returns @c true if the currently playing @em music is paused.
     *
     * @see pauseMusic(), musicIsPlaying()
     */
    bool musicIsPaused() const;

    /**
     * Pauses or resumes the currently playing @em music, if any.
     *
     * @see musicIsPaused(), musicIsPlaying(), stopMusic()
     */
    void pauseMusic(bool doPause = true);

    /**
     * Start playing a song. The chosen interface depends on what's available and what
     * sources have been associated with the song. Any song currently playing is stopped.
     *
     * @param definition  Music definition describing the associated music sources.
     * @param looped      @c true= restart the music each time playback completes.
     *
     * @return  Non-zero if a song is successfully played.
     *
     * @see playMusicLump(), playMusicFile(), playMusicCDTrack()
     * @see musicIsPlaying(), musicIsPaused(), pauseMusic()
     */
    de::dint playMusic(de::Record const &definition, bool looped = false);

    de::dint playMusicLump(lumpnum_t lumpNum, bool looped = false);
    de::dint playMusicFile(de::String const &filePath, bool looped = false);
    de::dint playMusicCDTrack(de::dint cdTrack, bool looped = false);

    /**
     * Stop all currently playing @em music, if any (affects all music interfaces).
     *
     * @see musicIsPlaying(), pauseMusic(),
     */
    void stopMusic();

public:  //- Sound playback: ------------------------------------------------------------

    /**
     * Returns @c true if one or more interface for audible @em sound playback is available
     * on the local system.
     *
     * @see musicPlaybackAvailable()
     */
    bool soundPlaybackAvailable() const;

    /**
     * Convenient method returning the current sound effect playback volume.
     *
     * @see musicVolume()
     */
    de::dint soundVolume() const;

    /**
     * Start playing a sound in the specified @a soundStage.
     *
     * If @a emitter and @a origin are both @c nullptr, the sound will be played with stereo
     * positioning (centered).
     *
     * @param stageId   Unique identifier of the sound Stage on which to play.
     * @param effectId  Identifier of the sound-effect to play.
     * @param emitter   Soundstage SoundEmitter (originator). May be @c nullptr.
     * @param origin    Soundstage space coordinates where the sound originates (if used).
     * @param volume    Volume modifier [0...1] (not final; will be affected by the global
     *                  @ref soundVolume() factor and if applicable, attenuated according
     *                  to it's distance from the soundstage Listener).
     *
     * @return  @c true if playback was started and the sound is actually audible.
     *
     * @see soundIsPlaying(), stopSound()
     */
    bool playSound(StageId stageId, de::dint effectId, SoundFlags flags, SoundEmitter *emitter,
        de::Vector3d const &origin, de::dfloat volume = 1 /*max volume*/);

    /**
     * Stop playing sound(s) in the specified @a soundStage.
     *
     * @param stageId   Unique identifier of the sound Stage on which to stop sounds.
     * @param effectId  Unique identifier of the sound-effect(s) to stop.
     * @param emitter   Soundstage SoundEmitter (originator). May be @c nullptr.
     * @param flags     @ref soundStopFlags.
     *
     * @see soundIsPlaying(), stopSound()
     */
    void stopSound(StageId stageId, de::dint effectId, SoundEmitter *emitter,
        de::dint flags = 0 /*no special stop behaviors*/);

public:  //- Low-level driver interface: ------------------------------------------------

    /// Required/referenced audio driver is missing. @ingroup errors
    DENG2_ERROR(MissingDriverError);

    /**
     * Returns the total number of loaded audio drivers.
     */
    de::dint driverCount() const;

    /**
     * Lookup the loaded audio driver associated with the given (unique) @a driverIdKey.
     *
     * @see tryFindDriver(), forAllDrivers()
     */
    IDriver const &findDriver(de::String driverIdKey) const;

    /**
     * Search for a loaded audio driver associated with the given (unique) @a driverIdKey.
     *
     * @return  Pointer to the loaded audio Driver if found; otherwise @c nullptr.
     *
     * @see findDriver(), forAllDrivers()
     */
    IDriver const *tryFindDriver(de::String driverIdKey) const;

    /**
     * Iterate through the loaded audio drivers (in load order), executing @a callback
     * for each.
     *
     * @see driverCount(), findDriver(), tryFindDriver()
     */
    de::LoopResult forAllDrivers(std::function<de::LoopResult (IDriver const &)> callback) const;

public:
    // Systems observe the passage of time.
    void timeChanged(de::Clock const &) override;

public:  /// @todo make private:
    /**
     * Stop channels (playing music and all sounds), clear the Sample data cache.
     * @todo observe ClientApp?
     */
    void reset();

    /**
     * Perform playback intialization (both music and sounds).
     * @todo observe ClientApp?
     */
    void initPlayback();

    /**
     * Perform playback deintialization (both music and sounds).
     * @todo observe ClientApp?
     */
    void deinitPlayback();

    /**
     * Enabling refresh is simple: the refresh thread(s) is resumed. When disabling refresh,
     * first make sure a new refresh doesn't begin (using allowRefresh). We still have to
     * see if a refresh is being made and wait for it to stop before we can suspend thread(s).
     */
    void allowChannelRefresh(bool allow = true);

    /// @todo refactor away.
    void startFrame();
    void endFrame();
    void updateMusicMidiFont();
    void worldMapChanged();

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

extern de::dint sfxBits, sfxRate;

#endif  // CLIENT_AUDIO_SYSTEM_H
