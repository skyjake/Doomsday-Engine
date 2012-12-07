/**
 * @file sys_audiod_sfx.h
 * Sound effects playback interface for an audio driver. @ingroup audio
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef __DOOMSDAY_AUDIO_DRIVER_SFX_H__
#define __DOOMSDAY_AUDIO_DRIVER_SFX_H__

/// @addtogroup audio
///@{

/**
 * @defgroup sfxBufferFlags Sfx Buffer Flags
 * @ingroup audio apiFlags
 */
///@{
#define SFXBF_PLAYING       0x1         ///< The buffer is playing.
#define SFXBF_3D            0x2         ///< Otherwise playing in 2D mode.
#define SFXBF_REPEAT        0x4         ///< Buffer will repeat until stopped.
#define SFXBF_DONT_STOP     0x8         ///< Never stop until normal finish.
#define SFXBF_STREAM        0x10        ///< Buffer plays in streaming mode (looping).
#define SFXBF_RELOAD        0x10000     ///< Sample must be reloaded before playing.
///@}

/// Sfx interface properties.
enum {
    SFXIP_DISABLE_CHANNEL_REFRESH = 1,  ///< The channel refresh thread is not needed.
    SFXIP_ANY_SAMPLE_RATE_ACCEPTED = 2  ///< Samples don't all need the same rate.
};

/// Events.
enum {
    SFXEV_BEGIN,    ///< An update is about to begin.
    SFXEV_END       ///< The update is done.
};

/// Buffer properties.
enum {
    SFXBP_VOLUME, ///< 0..1
    SFXBP_FREQUENCY, ///< 1 = normal
    SFXBP_PAN, ///< -1..1 (2D only)
    SFXBP_MIN_DISTANCE, ///< 3D only
    SFXBP_MAX_DISTANCE,
    SFXBP_POSITION,
    SFXBP_VELOCITY,
    SFXBP_RELATIVE_MODE
};

/// Listener properties.
enum {
    SFXLP_UPDATE, ///< Not a real value (commit deferred)
    SFXLP_PRIMARY_FORMAT, ///< Arguments are bits and rate.
    SFXLP_UNITS_PER_METER,
    SFXLP_DOPPLER,
    SFXLP_POSITION,
    SFXLP_VELOCITY,
    SFXLP_ORIENTATION,
    SFXLP_REVERB ///< Use SRD_* for indices.
};

typedef struct sfxsample_s {
    int             id;         ///< Id number of the sound sample.
    void*           data;       ///< Actual sample data.
    unsigned int    size;       ///< Size in bytes.
    int             numSamples; ///< Number of samples.
    int             bytesPer;   ///< Bytes per sample (1 or 2).
    int             rate;       ///< Samples per second.
    int             group;      ///< Exclusion group (0, if none).
} sfxsample_t;

typedef struct sfxbuffer_s {
    void*           ptr;        ///< Pointer to driver's own buffer object.
    void*           ptr3D;      ///< Pointer to driver's 3D buffer data.
    struct sfxsample_s* sample; ///< Source sample data.
    int             bytes;      ///< Bytes per sample (1 or 2).
    int             rate;       ///< Samples per second.
    int             flags;
    unsigned int    length;     ///< Length of the buffer (bytes).
    unsigned int    cursor;     ///< Write cursor position (%length).
    unsigned int    written;    ///< Total bytes written.
    unsigned int    endTime;    ///< System time, milliseconds (if !repeating).
    unsigned int    freq;       ///< Played samples per second (real freq).
} sfxbuffer_t;

/**
 * When a buffer is using SFXBF_STREAM, a sample's data pointer is interpreted
 * as a sfxstreamfunc_t and will be called whenever the sample needs more data
 * streamed in.
 */
typedef int (*sfxstreamfunc_t)(sfxbuffer_t* buf, void* data, unsigned int size);

/// Generic driver interface. All other interfaces are based on this.
typedef struct audiointerface_sfx_generic_s {
    int             (*Init) (void);
    sfxbuffer_t*    (*Create) (int flags, int bits, int rate);
    void            (*Destroy) (sfxbuffer_t* buf);
    void            (*Load) (sfxbuffer_t* buf, struct sfxsample_s* sample);
    void            (*Reset) (sfxbuffer_t* buf);
    void            (*Play) (sfxbuffer_t* buf);
    void            (*Stop) (sfxbuffer_t* buf);
    void            (*Refresh) (sfxbuffer_t* buf);
    void            (*Set) (sfxbuffer_t* buf, int prop, float value);
    void            (*Setv) (sfxbuffer_t* buf, int prop, float* values);
    void            (*Listener) (int prop, float value);
    void            (*Listenerv) (int prop, float* values);
    int             (*Getv) (int prop, void* values);
} audiointerface_sfx_generic_t;

typedef struct audiointerface_sfx_s {
    audiointerface_sfx_generic_t gen;
} audiointerface_sfx_t;

///@}

#endif
