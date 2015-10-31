/** @file audio/system.h  Client side audio subsystem.
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

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "audio/channel.h"
#include "audio/stage.h"

#include "dd_types.h"        // lumpnum_t
#include "SettingsRegister"
#include "world/p_object.h"
#include <de/DotPath>
#include <de/Error>
#include <de/Observers>
#include <de/Record>
#include <de/String>
#include <de/System>
#include <QList>
#include <functional>

#define SFX_LOWEST_PRIORITY     ( -1000 )

namespace audio {

class IPlayer;
class Mixer;
class SampleCache;

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
    LocalStage
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
     * Provides access to the Channel Mixer.
     */
    Mixer /*const*/ &mixer() const;

    /**
     * Provides access to the sample (waveform) asset cache.
     */
    SampleCache &sampleCache() const;

    /**
     * Determines the necessary upsample factor for the given waveform sample @a rate.
     */
    de::dint upsampleFactor(de::dint rate) const;

    /**
     * Reset playback tracking in the sound Stage associated with the given @a stageId.
     */
    void resetStage(StageId stage);

    /**
     * Provides access to the world sound Stage (FYI).
     */
    Stage /*const*/ &worldStage() const;

public:  // Music playback: -------------------------------------------------------------

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

public:  // Sound playback: -------------------------------------------------------------

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
     * Returns true if the referenced sound is currently playing somewhere in the given
     * @a soundStage. It does not matter if it is audible (or not).
     *
     * @param stageId  Unique identifier of the sound Stage to check.
     * @param soundId  @c 0= true if sounds are playing using the specified @a emitter.
     * @param emitter  WorldStage SoundEmitter (originator). May be @c nullptr.
     *
     * @see playSound(), stopSound()
     */
    bool soundIsPlaying(StageId stageId, de::dint soundId, SoundEmitter *emitter) const;

    /**
     * Start playing a sound in the specified @a soundStage.
     *
     * If @a emitter and @a origin are both @c nullptr, the sound will be played with stereo
     * positioning (centered).
     *
     * @param stageId          Unique identifier of the sound Stage on which to play.
     * @param soundIdAndFlags  ID of the sound to play. Flags can be included (DDSF_*).
     * @param emitter          WorldStage SoundEmitter (originator). May be @c nullptr.
     * @param origin           WorldStage space coordinates where the sound originates.
     *                         May be @c nullptr.
     * @param volume           Volume for the sound [0...1] (not final; will be affected
     *                         by the global @ref soundVolume() factor and if applicable,
     *                         attenuated according to @ref distanceToWorldStageListener()).
     *
     * @return  @c true if a sound was started.
     *
     * @see soundIsPlaying(), stopSound()
     */
    bool playSound(StageId stageId, de::dint soundIdAndFlags, SoundEmitter *emitter,
        coord_t const *origin, de::dfloat volume = 1 /*max volume*/);

    /**
     * Stop playing sound(s) in the specified @a soundStage.
     *
     * @param stageId  Unique identifier of the sound Stage on which to stop sounds.
     * @param soundId  Unique identifier of the sound(s) to stop.
     * @param emitter  WorldStage SoundEmitter (originator). May be @c nullptr.
     * @param flags    @ref soundStopFlags.
     *
     * @see soundIsPlaying(), stopSound()
     */
    void stopSound(StageId stageId, de::dint soundId, SoundEmitter *emitter,
        de::dint flags = 0 /*no special stop behaviors*/);

public:  // Low-level driver/playback interfaces: ---------------------------------------

    /// Required/referenced audio driver is missing. @ingroup errors
    DENG2_ERROR(MissingDriverError);

    /**
     * Interface for a component providing logical audio driver functionality.
     */
    class IDriver
    {
    public:
        /// Base class for property read errors. @ingroup errors
        DENG2_ERROR(ReadPropertyError);

        /// Base class for property write errors. @ingroup errors
        DENG2_ERROR(WritePropertyError);

        /**
         * Logical driver statuses.
         */
        enum Status
        {
            Loaded,      ///< Driver is loaded but not yet in use.
            Initialized  ///< Driver is loaded and initialized ready for use.
        };

        /**
         * Implementers of this interface are expected to automatically @ref deinitialize()
         * before this is called.
         */
        virtual ~IDriver() {}

        DENG2_AS_IS_METHODS()

        /// Returns a reference to the application's singleton audio::System instance.
        static inline System &audioSystem() { return System::get(); }

        inline bool isInitialized() const { return status() == Initialized; }

        /**
         * Returns the logical driver status.
         *
         * @see statusAsText()
         */
        virtual Status status() const = 0;

        /**
         * Returns a human-friendly, textual description of the current, high-level logical
         * status of the driver.
         *
         * @see status()
         */
        de::String statusAsText() const;

        /**
         * Returns detailed information about the driver as styled text. Printed by the
         * "inspectaudiodriver" console command, for instance.
         *
         * @see identityKey(), title(), statusAsText()
         */
        de::String description() const;

        /**
         * Initialize the driver if necessary, ready for use.
         */
        virtual void initialize() = 0;

        /**
         * Deinitialize the driver if necessary, so that it may be unloaded.
         */
        virtual void deinitialize() = 0;

        /**
         * Returns the textual, symbolic identifier of the audio driver (lower case), for
         * use in Config.
         *
         * @note An audio driver may have multiple identifiers, in which case they will
         * be returned here and delimited with ';' characters.
         *
         * @todo Once the driver configuration is stored persistently in Config we should
         * remove the alternative identifiers at this time. -ds
         *
         * @see title()
         */
        virtual de::String identityKey() const = 0;

        /**
         * Returns the human-friendly title of the audio driver.
         *
         * @see description(), identityKey()
         */
        virtual de::String title() const = 0;

    public:  //- Playback Channels: -----------------------------------------------------

        enum PlaybackInterfaceType
        {
            AUDIO_ICD,
            AUDIO_IMUSIC,
            AUDIO_ISFX,

            PlaybackInterfaceTypeCount
        };

        static de::String playbackInterfaceTypeAsText(PlaybackInterfaceType type);

        /**
         * Returns a listing of the logical playback interfaces implemented by the driver.
         * It is irrelevant whether said interfaces are presently available.
         *
         * Naturally, this means the driver must support interface enumeration @em before
         * driver initialization. The driver and/or interface may still fail to initialize
         * later, though.
         *
         * Each interface record must contain at least the following required elements:
         * - (NumberValue)"type"      : PlaybackInterfaceType identifier.
         *
         * - (TextValue)"identityKey" : Driver-unique, textual, symbolic identifier for
         *   the player interface (lowercase), for use in Config.
         */
        virtual QList<de::Record> listInterfaces() const = 0;

        /**
         * Perform any initialization necessary before playback can begin.
         */
        virtual void initInterface(de::String const &identityKey) {}

        /**
         * Perform any deinitializaion necessary to end playback before the driver is unloaded.
         */
        virtual void deinitInterface(de::String const &identityKey) {}

        /**
         * Construct a new playback Channel of the given @a type (note: ownership is retained).
         */
        virtual Channel *makeChannel(PlaybackInterfaceType type) = 0;

        /**
         * Iterate through available playback channels of the given @a type, and execute
         * @a callback for each.
         */
        virtual de::LoopResult forAllChannels(PlaybackInterfaceType type,
            std::function<de::LoopResult (Channel const &)> callback) const = 0;

        /**
         * Called by the audio::System to temporarily enable/disable refreshing of sound data
         * buffers in order to perform a critical task which operates on the current state of
         * that data.
         *
         * For example, when selecting a logical audio channel on which to play a new sound
         * it is imperative that Sound states do not change while doing so (e.g., some audio
         * drivers make use of a background thread for paging a subset of the waveform data,
         * for streaming purposes).
         *
         * @todo Belongs at channel/buffer level. -ds
         */
        virtual void allowRefresh(bool allow = true) = 0;
    };

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
    void startFrame();
    void endFrame();

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
     * Stop channels (playing music and all sounds), clear the Sample data cache.
     * @todo observe ClientApp?
     */
    void reset();

    /// @todo refactor away.
    void updateMusicMidiFont();

    /**
     * Enabling refresh is simple: the refresh thread(s) is resumed. When disabling refresh,
     * first make sure a new refresh doesn't begin (using allowRefresh). We still have to
     * see if a refresh is being made and wait for it to stop before we can suspend thread(s).
     */
    void allowChannelRefresh(bool allow = true);

    void worldMapChanged();

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

extern int sfxBits, sfxRate;

#endif  // CLIENT_AUDIO_SYSTEM_H
