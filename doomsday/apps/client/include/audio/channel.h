/** @file channel.h  Interface for an audio playback channel.
 * @ingroup audio
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

#ifndef CLIENT_AUDIO_CHANNEL_H
#define CLIENT_AUDIO_CHANNEL_H

#include "dd_share.h"        // SoundEmitter
#include "api_audiod_sfx.h"  // sfxsample_t
#include <de/Observers>
#include <de/Vector>

// Playback flags.
#define SFXCF_NO_ORIGIN         ( 0x1 )  ///< The originator is a mystical emitter.
#define SFXCF_NO_ATTENUATION    ( 0x2 )  ///< Play it very, very loud.
#define SFXCF_NO_UPDATE         ( 0x4 )  ///< Channel update is skipped.

namespace audio {

/**
 * Positioning modes for sound stage environment effects.
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
class Channel
{
public:
    /// Audience to be notified when the channel is about to be deleted.
    DENG2_DEFINE_AUDIENCE2(Deletion, void channelBeingDeleted(Channel &))

    enum PlayingMode {
        NotPlaying,
        Once,            ///< Play once and then discard the loaded data / stream.
        OnceDontDelete,  ///< Play once then pause.
        Looping          ///< Keep looping.
    };

public:
    Channel();
    virtual ~Channel();
    DENG2_AS_IS_METHODS()

    /**
     * Change the volume factor to @a newVolume.
     */
    virtual Channel &setVolume(de::dfloat newVolume) = 0;

    /**
     * Stop if playing and forget about any loaded data in the buffer.
     *
     * @note Just stopping doesn't affect refresh!
     */
    virtual void stop() = 0;

private:
    DENG2_PRIVATE(d)
};

class BaseMusicChannel : public Channel
{
public:
    BaseMusicChannel() : Channel() {}
    virtual ~BaseMusicChannel() {}

    virtual void update() = 0;
    virtual bool isPlaying() const = 0;
    virtual void pause(de::dint pause) = 0;
};

class CdChannel : public BaseMusicChannel
{
public:
    CdChannel() : BaseMusicChannel() {}
    virtual de::dint play(de::dint track, de::dint looped) = 0;
};

class MusicChannel : public BaseMusicChannel
{
public:
    MusicChannel() : BaseMusicChannel() {}

    /// Return @c true if the player provides playback from a managed buffer.
    virtual bool canPlayBuffer() const { return false; }

    virtual void *songBuffer(de::duint length) = 0;
    virtual de::dint play(de::dint looped) = 0;

    /// Returns @c true if the player provides playback from a native file.
    virtual bool canPlayFile() const { return false; }

    virtual de::dint playFile(de::String const &filename, de::dint looped) = 0;
};

class SoundChannel : public Channel
{
public:
    SoundChannel();

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

public:  //- Sound properties: ----------------------------------------------------------

    virtual SoundEmitter *emitter() const = 0;
    virtual de::dfloat frequency() const = 0;
    virtual de::Vector3d origin() const = 0;
    virtual Positioning positioning() const = 0;
    virtual de::dfloat volume() const = 0;

    virtual SoundChannel &setEmitter(SoundEmitter *newEmitter) = 0;
    virtual SoundChannel &setFrequency(de::dfloat newFrequency) = 0;
    virtual SoundChannel &setOrigin(de::Vector3d const &newOrigin) = 0;

public:
    virtual de::dint flags() const = 0;
    virtual void setFlags(de::dint newFlags) = 0;

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
     * @return  @c true if the (re)format completed successfully (equivalent to calling
     * @ref isValid() after this), for caller convenience.
     */
    virtual bool format(Positioning positioning, de::dint bytesPer, de::dint rate) = 0;

    /**
     * Returns @c true if the channel is in valid state (i.e., the previous @ref format()
     * completed successfully and it is ready to receive a sample of that format).
     */
    virtual bool isValid() const = 0;

    /**
     * Prepare the buffer for playing a sample by filling the buffer with as much sample
     * data as fits. The pointer to sample is saved, so the caller mustn't free it while
     * the sample is loaded.
     *
     * @note The sample is not reloaded if the buffer is already loaded with data with the
     * same sound ID.
     */
    virtual void load(sfxsample_t const &sample) = 0;

    virtual de::dint bytes() const = 0;
    virtual de::dint rate() const = 0;

    /**
     * Returns the time in tics that the sound was last played.
     */
    virtual de::dint startTime() const = 0;

    /**
     * Returns the time in tics that the currently loaded sample last ended; otherwise @c 0.
     */
    virtual de::dint endTime() const = 0;

    virtual sfxsample_t const *samplePtr() const = 0;

    virtual void updateEnvironment() = 0;
};

}  // namespace audio

#endif  // CLIENT_AUDIO_CHANNEL_H
