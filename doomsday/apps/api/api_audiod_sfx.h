/** @file api_audiod_sfx.h  Sound effect playback interface for audio drivers.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef CLIENT_API_AUDIO_DRIVER_SFX_H
#define CLIENT_API_AUDIO_DRIVER_SFX_H

/// @addtogroup audio
///@{

/**
 * @defgroup sfxBufferFlags Sfx Buffer Flags
 * @ingroup audio apiFlags
 */
///@{
#define SFXBF_PLAYING       ( 0x1     )  ///< The buffer is playing.
#define SFXBF_3D            ( 0x2     )  ///< Otherwise playing in 2D mode.
#define SFXBF_REPEAT        ( 0x4     )  ///< Buffer will repeat until stopped.
#define SFXBF_DONT_STOP     ( 0x8     )  ///< Never stop until normal finish.
#define SFXBF_STREAM        ( 0x10    )  ///< Buffer plays in streaming mode (looping).
#define SFXBF_RELOAD        ( 0x10000 )  ///< Sample must be reloaded before playing.
///@}

/**
 * Sfx interface properties.
 */
enum
{
    SFXIP_DISABLE_CHANNEL_REFRESH  = 1,  ///< The channel refresh thread is not needed.
    SFXIP_ANY_SAMPLE_RATE_ACCEPTED = 2,  ///< Samples don't all need the same rate.
    SFXIP_IDENTITYKEY
};

/**
 * Events.
 */
enum
{
    SFXEV_BEGIN,  ///< An update is about to begin.
    SFXEV_END     ///< The update is done.
};

/**
 * Buffer properties.
 */
enum
{
    SFXBP_VOLUME,        ///< 0..1
    SFXBP_FREQUENCY,     ///< 1 = normal
    SFXBP_PAN,           ///< -1..1 (2D only)
    SFXBP_MIN_DISTANCE,  ///< 3D only
    SFXBP_MAX_DISTANCE,
    SFXBP_POSITION,
    SFXBP_VELOCITY,
    SFXBP_RELATIVE_MODE
};

/**
 * Listener properties.
 */
enum
{
    SFXLP_UPDATE,           ///< Not a real value (commit deferred)
    SFXLP_PRIMARY_FORMAT,   ///< Arguments are bits and rate.
    SFXLP_UNITS_PER_METER,
    SFXLP_DOPPLER,
    SFXLP_POSITION,
    SFXLP_VELOCITY,
    SFXLP_ORIENTATION,
    SFXLP_REVERB            ///< Use SRD_* for indices.
};

typedef struct sfxsample_s {
    int             soundId;     ///< Id number of the sound.
    void *          data;        ///< Actual sample data.
    unsigned int    size;        ///< Size in bytes.
    int             numSamples;  ///< Number of samples.
    int             bytesPer;    ///< Bytes per sample (1 or 2).
    int             rate;        ///< Samples per second.
    int             group;       ///< Exclusion group (0, if none).

#ifdef __cplusplus
    /**
     * Returns the duration/length of the sample in milliseconds.
     */
    unsigned int milliseconds() const { return (1000 * numSamples) / rate; }
#endif
} sfxsample_t;

typedef struct sfxbuffer_s {
    void *          ptr;         ///< Pointer to driver's own buffer object.
    void *          ptr3D;       ///< Pointer to driver's 3D buffer data.
    struct sfxsample_s *sample;  ///< Source sample data.
    int             bytes;       ///< Bytes per sample (1 or 2).
    int             rate;        ///< Samples per second.
    int             flags;       ///< @ref sfxBufferFlags
    unsigned int    length;      ///< Length of the buffer (bytes).
    unsigned int    cursor;      ///< Write cursor position (%length).
    unsigned int    written;     ///< Total bytes written.
    unsigned int    endTime;     ///< System time, milliseconds (if !repeating).
    unsigned int    freq;        ///< Played samples per second (real freq).

#ifdef __cplusplus
    /**
     * Returns the length of the buffer in milliseconds.
     */
    unsigned int milliseconds() const {
        return sample ? (1000 * sample->numSamples / freq) : 0;
    }
#endif
} sfxbuffer_t;

/**
 * When a buffer is using SFXBF_STREAM, a sample's data pointer is interpreted
 * as a sfxstreamfunc_t and will be called whenever the sample needs more data
 * streamed in.
 */
typedef int (*sfxstreamfunc_t)(sfxbuffer_t *buf, void *data, unsigned int size);

/**
 * Generic driver interface. All other interfaces are based on this.
 */
typedef struct audiointerface_sfx_generic_s
{
    /**
     * Perform any initialization necessary before playback can begin.
     *
     * @return  Non-zero if successful (or already-initialized).
     */
    int (*Init) (void);

    /**
     * Allocate a managed sample buffer with the given specification.
     *
     * @param flags
     * @param bits
     * @param rate
     *
     * @return  Suitable (and possibly newly allocated) sample buffer. Ownership
     * is retained.
     */
    sfxbuffer_t *(*Create) (int flags, int bits, int rate);

    /**
     * Release the managed sample @a buffer. Calling this invalidates any other
     * existing references or pointers to @a buffer.
     */
    void (*Destroy) (sfxbuffer_t *buffer);

    /**
     * Prepare the buffer for playing a sample by filling the buffer with as
     * much sample data as fits. The pointer to sample is saved, so the caller
     * mustn't free it while the sample is loaded.
     *
     * @param buffer  Sound buffer.
     * @param sample  Sample data.
     */
    void (*Load) (sfxbuffer_t *buffer, struct sfxsample_s *sample);

    /**
     * Stop @a buffer if playing and forget about it's sample.
     *
     * @param buffer  Sound buffer.
     */
    void (*Reset) (sfxbuffer_t *buffer);

    /**
     * Start playing the sample loaded in @a buffer.
     */
    void (*Play) (sfxbuffer_t *buffer);

    /**
     * Stop @a buffer if playing and forget about it's sample.
     */
    void (*Stop) (sfxbuffer_t *buffer);

    /**
     * Called periodically by the audio system's refresh thread, so that @a buffer
     * can be filled with sample data, for streaming purposes.
     *
     * @note Don't do anything too time-consuming...
     */
    void (*Refresh) (sfxbuffer_t *buffer);

    /**
     * @param buffer   Sound buffer.
     * @param property  Buffer property:
     *              - SFXBP_VOLUME (if negative, interpreted as attenuation)
     *              - SFXBP_FREQUENCY
     *              - SFXBP_PAN (-1..1)
     *              - SFXBP_MIN_DISTANCE
     *              - SFXBP_MAX_DISTANCE
     *              - SFXBP_RELATIVE_MODE
     * @param value Value for the property.
     */
    void (*Set) (sfxbuffer_t *buffer, int prop, float value);

    /**
     * Coordinates specified in world coordinate system, converted to DSound's:
     * +X to the right, +Y up and +Z away (Y and Z swapped, i.e.).
     *
     * @param property      SFXBP_POSITION
     *                      SFXBP_VELOCITY
     */
    void (*Setv) (sfxbuffer_t *buffer, int prop, float *values);

    /**
     * @param property      SFXLP_UNITS_PER_METER
     *                      SFXLP_DOPPLER
     *                      SFXLP_UPDATE
     */
    void (*Listener) (int prop, float value);

    /**
     * Call SFXLP_UPDATE at the end of every channel update.
     */
    void (*Listenerv) (int prop, float *values);

    /**
     * Gets a driver property.
     *
     * @param prop    Property (SFXP_*).
     * @param values  Pointer to return value(s).
     */
    int  (*Getv) (int prop, void *values);

} audiointerface_sfx_generic_t;

typedef struct audiointerface_sfx_s {
    audiointerface_sfx_generic_t gen;
} audiointerface_sfx_t;

///@}

#endif  // CLIENT_API_AUDIO_DRIVER_SFX_H
