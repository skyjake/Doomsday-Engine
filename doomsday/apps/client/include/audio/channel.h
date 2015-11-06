/** @file channel.h  Interface for an audio playback channel.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_AUDIO_CHANNEL_H
#define CLIENT_AUDIO_CHANNEL_H

#include "dd_share.h"        // SoundEmitter
#include "api_audiod_sfx.h"  // sfxsample_t
#include <de/Deletable>
#include <de/Observers>
#include <de/Vector>

namespace audio {

/**
 * Playback behaviors.
 */
enum PlayingMode
{
    NotPlaying,
    Once,            ///< Play once only.
    OnceDontDelete,  ///< Play once then suspend (without stopping).
    Looping          ///< Play again when the end is reached (without stopping).
};

/**
 * Positioning models for sound stage environment effects.
 */
enum Positioning
{
    StereoPositioning,   ///< Simple 2D stereo, not 3D.
    AbsolutePositioning  ///< Originates from a fixed point in the sound stage.
};

/**
 * Interface model for a playback channel.
 *
 * @ingroup audio
 */
class Channel : public de::Deletable
{
public:
    enum Type
    {
        Cd,
        Music,
        Sound,

        TypeCount
    };

    static de::String typeAsText(Type type);

public:
    DENG2_AS_IS_METHODS()

    /**
     * Returns the current playback mode (set when @ref play() is called).
     */
    virtual PlayingMode mode() const = 0;

    inline bool isPlaying()       const { return mode() != NotPlaying; }
    inline bool isPlayingLooped() const { return mode() == Looping;    }

    /**
     * Start playing the currently configured stream/waveform/whatever data.
     */
    virtual void play(PlayingMode mode = Once) = 0;

    /**
     * Stop if playing and forget about currently configured stream/waveform/whatever data.
     *
     * @note Just stopping doesn't affect refresh!
     */
    virtual void stop() = 0;

    virtual bool isPaused() const = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;

    /**
     * Change the frequency/pitch modifier (factor) to @a newFrequency. Normally 1.0
     *
     * @note Not all audio libraries support changing the frequency dynamically, in which
     * case any changes will be ignored.
     */
    virtual Channel &setFrequency(de::dfloat newFrequency) = 0;

    /**
     * Change the current Positioning model to @a newPositioning.
     *
     * @note Not all positioning models will make sense for all channels. For example, if
     * a Channel is specialized for playing music it may not be possible to play it with
     * 3D positioning and/or Environment effects.
     *
     * @note Some audio libraries use different playback buffers that are specialized for
     * a certain model, in which case it may be necessary to reallocate/replace the backing
     * buffer in order to effect this change (e.g., Direct Sound). Consequently the user
     * should try to avoid changing the models dynamically, when/where possible.
     */
    virtual Channel &setPositioning(Positioning newPositioning) = 0;

    /**
     * Change the volume modifier (factor) to @a newVolume. Maximum is 1.0
     */
    virtual Channel &setVolume(de::dfloat newVolume) = 0;

    /**
     * Returns the current frequency modifier (factor).
     */
    virtual de::dfloat frequency() const = 0;

    /**
     * Returns the current Positioning model.
     */
    virtual Positioning positioning() const = 0;

    /**
     * Returns the current volume modifier (factor).
     */
    virtual de::dfloat volume() const = 0;

    /**
     * Returns @c true if the channel supports sources with "any" sampler rate; otherwise
     * @c false if the user is responsible for ensuring the source matches the configured
     * sampler rate.
     */
    virtual bool anyRateAccepted() const { return true; }
};

// --------------------------------------------------------------------------------------

/**
 * Constructs audio::Channels for the audio::System.
 */
class IChannelFactory
{
public:
    virtual ~IChannelFactory() {}

    /**
     * Called when the audio::System needs a new playback Channel of the given @a type.
     * This allows specialized factories to choose the concrete channel type and customize
     * it accordingly.
     *
     * @note: Ownership is currently retained!
     */
    virtual Channel *makeChannel(Channel::Type type) = 0;
};

// --------------------------------------------------------------------------------------

class CdChannel : public Channel
{
public:
    virtual void bindTrack(de::dint track) = 0;
};

class MusicChannel : public Channel
{
public:
    /// Returns @c true if playback is possible from a bound data buffer.
    virtual bool canPlayBuffer() const { return false; }
    virtual void *songBuffer(de::duint length) = 0;

    /// Returns @c true if playback is possible from a bound data file.
    virtual bool canPlayFile() const { return false; }
    virtual void bindFile(de::String const &filename) = 0;
};

class Sound;

class SoundChannel : public Channel
{
public:
    /**
     * Returns the logical Sound being played if currently playing (may return @c nullptr).
     */
    virtual ::audio::Sound *sound() const = 0;

    /**
     * Perform a channel update. Can be used for filling the channel with waveform data
     * for streaming purposes, or similar.
     *
     * @note Don't do anything too time-consuming...
     */
    virtual void update() = 0;

    /**
     * Stop the sound if playing and forget about any sample loaded in the buffer.
     *
     * @todo Logically distinct from @ref stop() ? -ds
     */
    virtual void reset() = 0;

    /**
     * Suspend updates to the channel if playing and wait until further notice.
     */
    virtual void suspend() = 0;

    /**
     * Prepare the buffer for playing a sample by filling the buffer with as much sample
     * data as fits. The pointer to sample is saved, so the caller mustn't free it while
     * the sample is loaded.
     *
     * @note The sample is not reloaded if the buffer is already loaded with data with the
     * same sound ID.
     */
    virtual void bindSample(sfxsample_t const &sample) = 0;

    virtual de::dint bytes() const = 0;
    virtual de::dint rate() const = 0;

    /**
     * Returns the time in tics that the sound was last played.
     */
    virtual de::dint startTime() const = 0;

    /**
     * Returns the time in milliseconds when playback of the currently loaded sample has
     * ended (or, will end if called before then); otherwise returns @c 0 if no sample is
     * currently loaded.
     */
    virtual de::duint endTime() const = 0;

    virtual void updateEnvironment() = 0;
};

}  // namespace audio

#endif  // CLIENT_AUDIO_CHANNEL_H
