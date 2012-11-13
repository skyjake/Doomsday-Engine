/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * sys_audiod_dummy.c: Dummy Audio Driver.
 *
 * Used in dedicated server mode, when it's necessary to simulate
 * sound playing but not actually play anything.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "sys_audiod.h"
#include "sys_audiod_sfx.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int         DS_DummyInit(void);
void        DS_DummyShutdown(void);
void        DS_DummyEvent(int type);

int         DS_Dummy_SFX_Init(void);
sfxbuffer_t* DS_Dummy_SFX_CreateBuffer(int flags, int bits, int rate);
void        DS_Dummy_SFX_DestroyBuffer(sfxbuffer_t* buf);
void        DS_Dummy_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample);
void        DS_Dummy_SFX_Reset(sfxbuffer_t* buf);
void        DS_Dummy_SFX_Play(sfxbuffer_t* buf);
void        DS_Dummy_SFX_Stop(sfxbuffer_t* buf);
void        DS_Dummy_SFX_Refresh(sfxbuffer_t* buf);
void        DS_Dummy_SFX_Set(sfxbuffer_t* buf, int prop, float value);
void        DS_Dummy_SFX_Setv(sfxbuffer_t* buf, int prop, float* values);
void        DS_Dummy_SFX_Listener(int prop, float value);
void        DS_Dummy_SFX_Listenerv(int prop, float* values);
int         DS_Dummy_SFX_Getv(int prop, void* values);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

audiodriver_t audiod_dummy = {
    DS_DummyInit,
    DS_DummyShutdown,
    DS_DummyEvent
};

audiointerface_sfx_t audiod_dummy_sfx = { {
    DS_Dummy_SFX_Init,
    DS_Dummy_SFX_CreateBuffer,
    DS_Dummy_SFX_DestroyBuffer,
    DS_Dummy_SFX_Load,
    DS_Dummy_SFX_Reset,
    DS_Dummy_SFX_Play,
    DS_Dummy_SFX_Stop,
    DS_Dummy_SFX_Refresh,
    DS_Dummy_SFX_Set,
    DS_Dummy_SFX_Setv,
    DS_Dummy_SFX_Listener,
    DS_Dummy_SFX_Listenerv,
    DS_Dummy_SFX_Getv
} };

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited;

// CODE --------------------------------------------------------------------

/**
 * Initialization of the sound driver.
 * @return @c true if successful.
 */
int DS_DummyInit(void)
{
    if(inited)
        return true; // Already initialized.

    inited = true;
    return true;
}

/**
 * Shut everything down.
 */
void DS_DummyShutdown(void)
{
    inited = false;
}

/**
 * The Event function is called to tell the driver about certain critical
 * events like the beginning and end of an update cycle.
 *
 * @param type  Type of event.
 */
void DS_DummyEvent(int type)
{
    // Do nothing...
}

int DS_Dummy_SFX_Init(void)
{
    return inited;
}

sfxbuffer_t* DS_Dummy_SFX_CreateBuffer(int flags, int bits, int rate)
{
    sfxbuffer_t* buf;

    // Clear the buffer.
    buf = Z_Calloc(sizeof(*buf), PU_APPSTATIC, 0);

    buf->bytes = bits / 8;
    buf->rate = rate;
    buf->flags = flags;
    buf->freq = rate; // Modified by calls to Set(SFXBP_FREQUENCY).

    return buf;
}

void DS_Dummy_SFX_DestroyBuffer(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    // Free the memory allocated for the buffer.
    Z_Free(buf);
}

/**
 * Prepare the buffer for playing a sample by filling the buffer with as
 * much sample data as fits. The pointer to sample is saved, so the caller
 * mustn't free it while the sample is loaded.
 *
 * @param buf     Sound buffer.
 * @param sample  Sample data.
 */
void DS_Dummy_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample)
{
    if(!buf || !sample)
        return;

    // Now the buffer is ready for playing.
    buf->sample = sample;
    buf->written = sample->size;
    buf->flags &= ~SFXBF_RELOAD;
}

/**
 * Stops the buffer and makes it forget about its sample.
 *
 * @param buf  Sound buffer.
 */
void DS_Dummy_SFX_Reset(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    DS_Dummy_SFX_Stop(buf);
    buf->sample = NULL;
    buf->flags &= ~SFXBF_RELOAD;
}

/**
 * @param buf  Sound buffer.
 * @return The length of the buffer in milliseconds.
 */
unsigned int DS_DummyBufferLength(sfxbuffer_t* buf)
{
    if(!buf)
        return 0;

    return 1000 * buf->sample->numSamples / buf->freq;
}

void DS_Dummy_SFX_Play(sfxbuffer_t* buf)
{
    // Playing is quite impossible without a sample.
    if(!buf || !buf->sample)
        return;

    // Do we need to reload?
    if(buf->flags & SFXBF_RELOAD)
        DS_Dummy_SFX_Load(buf, buf->sample);

    // The sound starts playing now?
    if(!(buf->flags & SFXBF_PLAYING))
    {
        // Calculate the end time (milliseconds).
        buf->endTime = Timer_RealMilliseconds() + DS_DummyBufferLength(buf);
    }

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_Dummy_SFX_Stop(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    // Clear the flag that tells the Sfx module about playing buffers.
    buf->flags &= ~SFXBF_PLAYING;

    // If the sound is started again, it needs to be reloaded.
    buf->flags |= SFXBF_RELOAD;
}

/**
 * Buffer streamer. Called by the Sfx refresh thread.
 * @param buf  Sound buffer.
 */
void DS_Dummy_SFX_Refresh(sfxbuffer_t* buf)
{
    // Can only be done if there is a sample and the buffer is playing.
    if(!buf || !buf->sample || !(buf->flags & SFXBF_PLAYING))
        return;

    // Have we passed the predicted end of sample?
    if(!(buf->flags & SFXBF_REPEAT) && Timer_RealMilliseconds() >= buf->endTime)
    {
        // Time for the sound to stop.
        DS_Dummy_SFX_Stop(buf);
    }
}

/**
 * @param buf   Sound buffer.
 * @param prop  Buffer property:
 *              - SFXBP_VOLUME (if negative, interpreted as attenuation)
 *              - SFXBP_FREQUENCY
 *              - SFXBP_PAN (-1..1)
 *              - SFXBP_MIN_DISTANCE
 *              - SFXBP_MAX_DISTANCE
 *              - SFXBP_RELATIVE_MODE
 * @param value Value for the property.
 */
void DS_Dummy_SFX_Set(sfxbuffer_t* buf, int prop, float value)
{
    if(!buf)
        return;

    switch(prop)
    {
    case SFXBP_FREQUENCY:
        buf->freq = buf->rate * value;
        break;

    default:
        break;
    }
}

/**
 * Coordinates specified in world coordinate system, converted to DSound's:
 * +X to the right, +Y up and +Z away (Y and Z swapped, i.e.).
 *
 * @param property      SFXBP_POSITION
 *                      SFXBP_VELOCITY
 */
void DS_Dummy_SFX_Setv(sfxbuffer_t* buf, int prop, float* values)
{
    // Nothing to do.
}

/**
 * @param property      SFXLP_UNITS_PER_METER
 *                      SFXLP_DOPPLER
 *                      SFXLP_UPDATE
 */
void DS_Dummy_SFX_Listener(int prop, float value)
{
    // Nothing to do.
}

/**
 * Values use SRD_* for indices.
 */
void DS_DummyListenerEnvironment(float* rev)
{
    // Nothing to do.
}

/**
 * Call SFXLP_UPDATE at the end of every channel update.
 */
void DS_Dummy_SFX_Listenerv(int prop, float* values)
{
    // Nothing to do.
}

/**
 * Gets a driver property.
 *
 * @param prop    Property (SFXP_*).
 * @param values  Pointer to return value(s).
 */
int DS_Dummy_SFX_Getv(int prop, void* values)
{
    switch(prop)
    {
    case SFXIP_DISABLE_CHANNEL_REFRESH: {
        /// The return value is a single 32-bit int.
        int* wantDisable = (int*) values;
        if(wantDisable)
        {
            // We are not playing any audio.
            *wantDisable = true;
        }
        break; }

    default:
        return false;
    }
    return true;
}
