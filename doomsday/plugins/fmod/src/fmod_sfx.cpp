/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "driver_fmod.h"
#include <stdlib.h>

int DS_SFX_Init(void)
{
    return fmodSystem != 0;
}

sfxbuffer_t* DS_SFX_CreateBuffer(int flags, int bits, int rate)
{
    sfxbuffer_t* buf;

    // Clear the buffer.
    buf = reinterpret_cast<sfxbuffer_t*>(calloc(sizeof(*buf), 1));

    buf->bytes = bits / 8;
    buf->rate = rate;
    buf->flags = flags;
    buf->freq = rate; // Modified by calls to Set(SFXBP_FREQUENCY).

    return buf;
}

void DS_SFX_DestroyBuffer(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    // Free the memory allocated for the buffer.
    free(buf);
}

/**
 * Prepare the buffer for playing a sample by filling the buffer with as
 * much sample data as fits. The pointer to sample is saved, so the caller
 * mustn't free it while the sample is loaded.
 */
void DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample)
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
 */
void DS_SFX_Reset(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    DS_SFX_Stop(buf);
    buf->sample = NULL;
    buf->flags &= ~SFXBF_RELOAD;
}

#if 0
/**
 * @return              The length of the buffer in milliseconds.
 */
static unsigned int DS_SFX_BufferLength(sfxbuffer_t* buf)
{
    if(!buf)
        return 0;

    return 1000 * buf->sample->numSamples / buf->freq;
}
#endif

void DS_SFX_Play(sfxbuffer_t* buf)
{
    // Playing is quite impossible without a sample.
    if(!buf || !buf->sample)
        return;

    /*
    // Do we need to reload?
    if(buf->flags & SFXBF_RELOAD)
        DS_SFX_Load(buf, buf->sample);

    // The sound starts playing now?
    if(!(buf->flags & SFXBF_PLAYING))
    {
        // Calculate the end time (milliseconds).
        buf->endTime = Sys_GetRealTime() + DS_DummyBufferLength(buf);
    }*/

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_SFX_Stop(sfxbuffer_t* buf)
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
 * FMOD handles this for us.
 */
void DS_SFX_Refresh(sfxbuffer_t*)
{}

/**
 * @param prop          SFXBP_VOLUME (if negative, interpreted as attenuation)
 *                      SFXBP_FREQUENCY
 *                      SFXBP_PAN (-1..1)
 *                      SFXBP_MIN_DISTANCE
 *                      SFXBP_MAX_DISTANCE
 *                      SFXBP_RELATIVE_MODE
 */
void DS_SFX_Set(sfxbuffer_t* buf, int prop, float value)
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
 * Coordinates specified in world coordinate system:
 * +X to the right, +Y up and +Z away (Y and Z swapped, i.e.).
 *
 * @param property      SFXBP_POSITION
 *                      SFXBP_VELOCITY
 */
void DS_SFX_Setv(sfxbuffer_t* buf, int prop, float* values)
{
    // Nothing to do.
}

/**
 * @param property      SFXLP_UNITS_PER_METER
 *                      SFXLP_DOPPLER
 *                      SFXLP_UPDATE
 */
void DS_SFX_Listener(int prop, float value)
{
    // Nothing to do.
}

static void setListenerEnvironment(float* rev)
{
    // Nothing to do.
}

void DS_SFX_Listenerv(int prop, float* values)
{
    // Nothing to do.
}

/**
 * Gets a driver property.
 *
 * @param prop    Property (SFXP_*).
 * @param values  Pointer to return value(s).
 */
int DS_SFX_Getv(int prop, void* values)
{
    switch(prop)
    {
    case SFXP_DISABLE_CHANNEL_REFRESH:
    {
        /// For SFXP_DISABLE_CHANNEL_REFRESH, the return value is a single 32-bit int.
        int* i = reinterpret_cast<int*>(values);
        if(i)
        {
            // Channel refresh is handled by FMOD, so we don't need to do anything.
            *i = true;
        }
        break;
    }

    default:
        return false;
    }
    return true;
}
