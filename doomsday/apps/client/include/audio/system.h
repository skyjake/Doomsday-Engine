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

#include "dd_types.h"        // lumpnum_t
#include "SettingsRegister"
#include "world/p_object.h"
#include <de/DotPath>
#include <de/Error>
#include <de/Observers>
#include <de/Record>
#include <de/String>
#include <de/Range>
#include <de/System>
#include <functional>

#define SFX_LOWEST_PRIORITY     ( -1000 )

namespace audio {

class IPlayer;

class Channels;
class SampleCache;
class Sound;

/**
 * Client audio subsystem.
 *
 * @ingroup audio
 */
class System : public de::System
{
public:
    /// Notified when a new audio frame begins.
    DENG2_DEFINE_AUDIENCE2(FrameBegins, void systemFrameBegins(System &system))

    /// Notified when the current audio frame ends.
    DENG2_DEFINE_AUDIENCE2(FrameEnds,   void systemFrameEnds(System &system))

    /// Notified whenever a MIDI font change occurs.
    DENG2_DEFINE_AUDIENCE2(MidiFontChange, void systemMidiFontChanged(de::String const &newMidiFontPath))

public:
    /**
     * Instantiate the singleton audio::System instance.
     */
    System();

    // Systems observe the passage of time.
    void timeChanged(de::Clock const &) override;

    SettingsRegister &settings();

    /**
     * Returns a textual, human-friendly description of the audio system configuration
     * including an active playback interface itemization (suitable for logging, error
     * messages, etc..).
     */
    de::String description() const;

    /**
     * Provides access to the sound Channels.
     */
    Channels /*const*/ &channels() const;

    /**
     * Returns the world map object used as the current sound listener, if any (may return
     * @c nullptr if none is configured).
     */
    struct mobj_s *listener();

    /**
     * Convenient method determining the distance from the given world map space @a point
     * to the active listener, in map space units; otherwise returns @c 0 if no current
     * listener exists.
     */
    coord_t distanceToListener(de::Vector3d const &point) const;

    /**
     * Provides access to the sample (waveform) cache.
     */
    SampleCache &sampleCache() const;

    /**
     * Determines the necessary upsample factor for the given sample @a rate.
     */
    de::dint upsampleFactor(de::dint rate) const;

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
     * Returns @c true if one or more interface for audible @em music playback is
     * available on the local system.
     */
    bool musicPlaybackAvailable() const;

    /**
     * Convenient method returning the current music playback volume.
     */
    de::dint musicVolume() const;

    /**
     * Returns true if @em music is currently playing (on any music interface). It does
     * not matter if it is audible (or not).
     */
    bool musicIsPlaying() const;

    /**
     * Returns @c true if the currently playing @em music is paused.
     */
    bool musicIsPaused() const;

    /**
     * Pauses or resumes the @em music.
     */
    void pauseMusic(bool doPause = true);

    /**
     * Stop all currently playing @em music, if any (affects all music interfaces).
     */
    void stopMusic();

    /**
     * Start playing a song. The chosen interface depends on what's available and what
     * sources have been associated with the song. Any song currently playing song is
     * stopped.
     *
     * @param definition  Music definition describing the associated music sources.
     *
     * @return  Non-zero if a song is successfully played.
     */
    de::dint playMusic(de::Record const &definition, bool looped = false);

    de::dint playMusicLump(lumpnum_t lumpNum, bool looped = false);
    de::dint playMusicFile(de::String const &filePath, bool looped = false);
    de::dint playMusicCDTrack(de::dint cdTrack, bool looped = false);

public:  // Sound playback: -------------------------------------------------------------

    /**
     * Returns @c true if one or more interface for audible @em sound playback is available
     * on the local system.
     */
    bool soundPlaybackAvailable() const;

    /**
     * Convenient method returning the current sound effect playback volume.
     */
    de::dint soundVolume() const;

    /**
     * Convenient method returning the current sound effect volume attenuation range, in
     * map space units.
     */
    de::Ranged soundVolumeAttenuationRange() const;

    /**
     * Returns true if the sound is currently playing somewhere in the world. It does not
     * matter if it is audible (or not).
     *
     * @param soundId  @c 0= true if sounds are playing using the specified @a emitter.
     * @param emitter  Mobj where the sound originates. May be @c nullptr.
     */
    bool soundIsPlaying(de::dint soundId, struct mobj_s *emitter) const;

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
    bool playSound(de::dint soundIdAndFlags, struct mobj_s *emitter, coord_t const *origin,
        de::dfloat volume = 1 /*max volume*/);

public:  // Low-level driver interfaces: ------------------------------------------------

    /// Required/referenced audio driver is missing. @ingroup errors
    DENG2_ERROR(MissingDriverError);

    /**
     * Base class for a logical audio driver.
     */
    class IDriver
    {
    public:
        /// Base class for property read errors. @ingroup errors
        DENG2_ERROR(ReadPropertyError);

        /// Base class for property write errors. @ingroup errors
        DENG2_ERROR(WritePropertyError);

        /// Referenced player interface is missing. @ingroup errors
        DENG2_ERROR(MissingPlayerError);

        /**
         * Logical driver status.
         */
        enum Status
        {
            Loaded,      ///< Driver is loaded but not yet in use.
            Initialized  ///< Driver is loaded and initialized ready for use.
        };

        /**
         * If the driver is still initialized it should be automatically deinitialized
         * before this is called.
         */
        virtual ~IDriver() {}

        DENG2_AS_IS_METHODS()

        /// Returns a reference to the application's singleton audio System instance.
        static inline System &audioSystem() { return System::get(); }

        /**
         * Initialize the audio driver if necessary, ready for use.
         */
        virtual void initialize() = 0;

        /**
         * Deinitialize the audio driver if necessary, so that it may be unloaded.
         */
        virtual void deinitialize() = 0;

        /**
         * Returns the logical driver status.
         */
        virtual Status status() const = 0;

        /**
         * Returns a human-friendly, textual description of the logical driver status.
         */
        de::String statusAsText() const;

        inline bool isLoaded     () const { return status() >= Loaded;      }
        inline bool isInitialized() const { return status() == Initialized; }

        /**
         * Returns detailed information about the driver as styled text. Printed by
         * "inspectaudiodriver", for instance.
         */
        de::String description() const;

        /**
         * Returns the textual, symbolic identifier of the audio driver (lower case), for
         * use in Config.
         *
         * @note An audio driver may have multiple identifiers, in which case they will
         * be returned here and delimited with ';' characters.
         *
         * @todo Once the audio driver/interface configuration is stored persistently in
         * Config we should remove the alternative identifiers at this time. -ds
         */
        virtual de::String identityKey() const = 0;

        /**
         * Returns the human-friendly title of the audio driver.
         */
        virtual de::String title() const = 0;

    public:  // Playback Interfaces: ----------------------------------------------------

        /**
         * Returns the total number of player interfaces. 
         */
        virtual de::dint playerCount() const = 0;

        /**
         * Returns the driver-unique, textual, symbolic identifier of the player interface
         * (lower case), for use in Config.
         */
        virtual de::String playerIdentityKey(IPlayer const &player) const = 0;

        /**
         * Iterate through the player interfaces, executing @a callback for each.
         */
        virtual de::LoopResult forAllPlayers(std::function<de::LoopResult (IPlayer &)> callback) const = 0;
    };

    /**
     * Returns the total number of loaded audio drivers. 
     */
    de::dint driverCount() const;

    /**
     * Lookup the loaded audio driver associated with the given (unique) @a driverIdKey.
     */
    IDriver const &findDriver(de::String driverIdKey) const;

    /**
     * Search for a loaded audio driver associated with the given (unique) @a driverIdKey.
     *
     * @return  Pointer to the loaded audio Driver if found; otherwise @c nullptr.
     */
    IDriver const *tryFindDriver(de::String driverIdKey) const;

    /**
     * Iterate through the loaded audio drivers (in load order), executing @a callback
     * for each.
     */
    de::LoopResult forAllDrivers(std::function<de::LoopResult (IDriver const &)> callback) const;

public:  /// @todo make private:
    void startFrame();
    void endFrame();

    /**
     * Perform playback intialization (both sound effects and music).
     * @todo observe App?
     */
    void initPlayback();

    /**
     * Perform playback deintialization (both sound effects and music).
     * @todo observe App?
     */
    void deinitPlayback();

    /**
     * Stop channels (all playing sounds and music), clear the Sample data cache.
     * @todo observe ClientApp?
     */
    void reset();

    /// @todo refactor away.
    void requestListenerUpdate();

    /// @todo refactor away.
    void updateMusicMidiFont();

    /**
     * Enabling sound refresh is simple: the refresh thread(s) is resumed. When disabling
     * refresh, first make sure a new refresh doesn't begin (using allowRefresh). We still
     * have to see if a refresh is being made and wait for it to stop. Then we can suspend
     * the  refresh thread.
     */
    void allowSoundRefresh(bool allow = true);

    void worldMapChanged();

    /**
     * Determines whether a logical sound is currently playing, irrespective of whether it
     * is audible or not.
     */
    bool logicalSoundIsPlaying(de::dint soundId, struct mobj_s *emitter) const;

    /**
     * The sound is removed from the list of playing sounds. To be called whenever a/the
     * associated sound is stopped, regardless of whether it was actually playing on the
     * local system.
     *
     * @note Use @a soundId == 0 and @a emitter == nullptr to stop @em everything.
     *
     * @return  Number of sounds stopped.
     */
    de::dint stopLogicalSound(de::dint soundId, struct mobj_s *emitter);

    void startLogicalSound(de::dint soundIdAndFlags, struct mobj_s *emitter);

    void clearAllLogicalSounds();

public:
    /**
     * Returns the singleton audio::System instance.
     */
    static System &get();

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

class IPlayer
{
public:
    virtual ~IPlayer() {}
    DENG2_AS_IS_METHODS()

    /**
     * Perform any initialization necessary before playback can begin.
     * @return  Non-zero if successful (or already-initialized).
     */
    virtual de::dint initialize() = 0;

    /**
     * Perform any deinitializaion necessary before the driver is unloaded.
     */
    virtual void deinitialize() = 0;
};

/// @todo revise API:
class ICdPlayer : public IPlayer
{
public:
    virtual void update() = 0;
    virtual void setVolume(de::dfloat newVolume) = 0;
    virtual bool isPlaying() const = 0;
    virtual void pause(de::dint pause) = 0;
    virtual void stop() = 0;

    virtual de::dint play(de::dint track, de::dint looped) = 0;
};

/// @todo revise API:
class IMusicPlayer : public IPlayer
{
public:
    virtual void update() = 0;
    virtual void setVolume(de::dfloat newVolume) = 0;
    virtual bool isPlaying() const = 0;
    virtual void pause(de::dint pause) = 0;
    virtual void stop() = 0;

    /// Return @c true if the player provides playback from a managed buffer.
    virtual bool canPlayBuffer() const { return false; }

    virtual void *songBuffer(de::duint length) = 0;
    virtual de::dint play(de::dint looped) = 0;

    /// Returns @c true if the player provides playback from a native file.
    virtual bool canPlayFile() const { return false; }

    virtual de::dint playFile(char const *filename, de::dint looped) = 0;
};

/// @todo revise API:
class ISoundPlayer : public IPlayer
{
public:
    /**
     * Returns @c true if samples can use any sampler rate; otherwise @c false
     * if the user must ensure that all samples use the same sampler rate.
     */
    virtual bool anyRateAccepted() const = 0;

    /**
     * Called by the audio::System to temporarily enable/disable refreshing of
     * sound data buffers in order to perform a critical task which operates
     * on the current state of that data.
     *
     * For example, when selecting a logical audio channel on which to play a
     * new sound it is imperative that Sound states do not change while doing
     * so (e.g., some audio drivers make use of a background thread for paging
     * a subset of the waveform data, for streaming purposes).
     */
    virtual void allowRefresh(bool allow = true) = 0;

    /**
     * @param property - SFXLP_UNITS_PER_METER
     *                 - SFXLP_DOPPLER
     *                 - SFXLP_UPDATE
     */
    virtual void listener(de::dint prop, de::dfloat value) = 0;

    /**
     * Call SFXLP_UPDATE at the end of every channel update.
     */
    virtual void listenerv(de::dint prop, de::dfloat *values) = 0;

    /**
     * Prepare another Sound instance ready for loading with sample data.
     *
     * @param stereoPositioning  @c true= the resultant Sound should be configured
     * suitably for stereo positioning; otherwise use 3D positioning.
     *
     * @param bytesPer           Number of bytes per sample.
     * @param rate               Sampler rate / frequency in Hz.
     *
     * @return  Sound instance, preconfigured as specified; otherwise @c nullptr
     * if the driver does not support the given configuration.
     */
    virtual Sound *makeSound(bool stereoPositioning, de::dint bytesPer, de::dint rate) = 0;
};

}  // namespace audio

extern int sfxBits, sfxRate;

#endif  // CLIENT_AUDIO_SYSTEM_H
