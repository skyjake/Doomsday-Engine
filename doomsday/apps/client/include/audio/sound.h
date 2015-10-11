/** @file sound.h  Interface for playing sounds.
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

#ifndef CLIENT_AUDIO_SOUND_H
#define CLIENT_AUDIO_SOUND_H

#include "api_audiod_sfx.h"  // sfxbuffer_t
#include "world/p_object.h"  // mobj_t
#include <de/Observers>
#include <de/Vector>

// Sound flags.
#define SFXCF_NO_ORIGIN         ( 0x1 )  ///< Sound is coming from a mystical emitter.
#define SFXCF_NO_ATTENUATION    ( 0x2 )  ///< Sound is very, very loud.
#define SFXCF_NO_UPDATE         ( 0x4 )  ///< Channel update is skipped.

namespace audio {

/**
 * Interface model for a playable sound.
 */
class Sound
{
public:
    /// Audience to be notified when the sound instance is about to be deleted.
    DENG2_DEFINE_AUDIENCE2(Deletion, void soundBeingDeleted(Sound &))

    enum PlayingMode {
        NotPlaying,
        Once,           ///< Play once and then delete the sound.
        OnceDontDelete, ///< Play once then pause the sound.
        Looping         ///< Keep looping the sound.
    };

public:
    Sound();
    virtual ~Sound();
    DENG2_AS_IS_METHODS()

    /**
     * Returns @c true if the sound is in valid state (i.e., the previous format
     * completed successfully and it is ready to receive a sample of that format).
     */
    virtual bool isValid() const = 0;

    /**
     * Returns a pointer to the loaded sample data, if any (may return @c nullptr).
     */
    virtual sfxsample_t const *samplePtr() const = 0;

    virtual int flags() const = 0;
    virtual void setFlags(int newFlags) = 0;

    virtual void format(bool stereoPositioning, de::dint bytesPer, de::dint rate) = 0;

    virtual bool stereoPositioning() const = 0;
    virtual de::dint bytes() const = 0;
    virtual de::dint rate() const = 0;

    /**
     * Returns the attributed emitter if any (may be @c nullptr).
     */
    virtual SoundEmitter *emitter() const = 0;
    virtual void setEmitter(SoundEmitter *newEmitter) = 0;

    virtual void setOrigin(de::Vector3d const &newOrigin) = 0;
    virtual de::Vector3d origin() const = 0;

    /**
     * Change the playback frequency modifier to @a newFrequency (Hz).
     */
    virtual Sound &setFrequency(de::dfloat newFrequency) = 0;

    /**
     * Change the playback volume modifier to @a newVolume.
     */
    virtual Sound &setVolume(de::dfloat newVolume) = 0;

    virtual PlayingMode mode() const = 0;

    /**
     * Returns @c true if the sound is currently playing.
     */
    virtual bool isPlaying() const;

    /**
     * Returns the current playback frequency modifier: 1.0 is normal.
     */
    virtual de::dfloat frequency() const = 0;

    /**
     * Returns the current playback volume modifier: 1.0 is max.
     */
    virtual de::dfloat volume() const = 0;

    /**
     * Prepare the buffer for playing a sample by filling the buffer with as much
     * sample data as fits. The pointer to sample is saved, so the caller mustn't
     * free it while the sample is loaded.
     *
     * @note The sample is not reloaded if the buffer is already loaded with data
     * with the same sound ID.
     */
    virtual void load(sfxsample_t &sample) = 0;

    /**
     * Stop the sound if playing and forget about any sample loaded in the buffer.
     *
     * @note Just stopping doesn't affect refresh!
     */
    virtual void stop() = 0;

    /**
     * Stop the sound if playing and forget about any sample loaded in the buffer.
     *
     * @todo Logically distinct from @ref stop() ? -ds
     */
    virtual void reset() = 0;

    /**
     * Start playing the sound.
     */
    virtual void play(PlayingMode mode = Once) = 0;

    /**
     * Returns the time in tics that the sound was last played.
     */
    virtual de::dint startTime() const = 0;

    /**
     * Returns the time in tics that the currently loaded sample last ended; otherwise @c 0.
     */
    virtual de::dint endTime() const = 0;

    /**
     * Called periodically by the audio system's refresh thread, so that the buffer
     * can be filled with sample data, for streaming purposes.
     *
     * @note Don't do anything too time-consuming...
     */
    virtual void refresh() = 0;

public:
    /**
     * The priority of a sound is affected by distance, volume and age. These points
     * are used by the audio channel mapper to determine which currently playing Sound(s)
     * can be overridden with new sounds. Zero is the lowest priority.
     */
    static de::dfloat ratePriority(struct mobj_s *listener, SoundEmitter *emitter,
        coord_t const *origin, de::dfloat volume, de::dint startTic);

    /**
     * Calculate priority points for the sound. 
     */
    de::dfloat priority() const;

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_SOUND_H
