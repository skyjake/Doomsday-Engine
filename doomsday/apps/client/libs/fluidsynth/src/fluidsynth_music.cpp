/**
 * @file fluidsynth_music.cpp
 * Music playback interface. @ingroup dsfluidsynth
 *
 * @authors Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "driver_fluidsynth.h"
#include "doomsday.h"
#include <de/legacy/concurrency.h>
#include <de/c_wrapper.h>
#include <de/logbuffer.h>
#include <string.h>
#include <atomic>
#include <vector>

static int sfontId = -1;
static fluid_player_t* fsPlayer = 0;
static thread_t worker;
static volatile std::atomic_bool workerShouldStop;
static sfxbuffer_t* sfxBuf;
static sfxsample_t streamSample;

#define MAX_BLOCKS          6
#define SAMPLES_PER_SECOND  44100
#define BLOCK_SAMPLES       (SAMPLES_PER_SECOND / 8)
#define BYTES_PER_SAMPLE    2
#define BLOCK_SIZE          (2 * BYTES_PER_SAMPLE * BLOCK_SAMPLES) // 16 bit

/**
 * Ring buffer for storing synthesized samples. This is thread-safe as there
 * is a separate thread where the synthesizer is being run when a song plays.
 */
class RingBuffer
{
public:
    /**
     * Constructs a ring buffer.
     * @param size  Size of the buffer in bytes.
     */
    RingBuffer(int size) : _buf(0), _size(size)
    {
        _buf = new byte[size];
        _end = _buf + size;
        _writePos = _readPos = _buf;
        _mutex = Sys_CreateMutex("fs_ringbuf");
    }

    ~RingBuffer()
    {
        delete [] _buf;
        Sys_DestroyMutex(_mutex);
    }

    int size() const { return _size; }
    void* data() { return _buf; }

    void clear()
    {
        Sys_Lock(_mutex);
        _writePos = _readPos = _buf;
        Sys_Unlock(_mutex);
    }

    int availableForWriting() const
    {
        return _size - availableForReading() - 1;
    }

    int availableForReading() const
    {
        Sys_Lock(_mutex);

        int avail;
        if (_writePos >= _readPos)
        {
            avail = _writePos - _readPos;
        }
        else
        {
            // Write position was wrapped around.
            avail = (_end - _readPos) + (_writePos - _buf);
        }

        Sys_Unlock(_mutex);
        return avail;
    }

    void write(const void* data, int length)
    {
        Sys_Lock(_mutex);

        DE_ASSERT(_writePos < _end);

        // No need to split?
        const int remainder = _end - _writePos;
        if (length <= remainder)
        {
            memcpy(_writePos, data, length);
            _writePos += length;
            if (_writePos == _end) _writePos = _buf; // May wrap around.
        }
        else
        {
            // Do the write in two parts.
            memcpy(_writePos, data, remainder);
            memcpy(_buf, (byte*)data + remainder, length - remainder);
            _writePos = _buf + length - remainder;
        }

        Sys_Unlock(_mutex);
    }

    /**
     * Reads a block of data from the buffer.
     *
     * @param data    The read data will be written here.
     * @param length  Number of bytes to read. If there aren't this many
     *                bytes currently available, reads all the available
     *                data instead.
     *
     * @return  Actual number of bytes read.
     */
    int read(void* data, int length)
    {
        Sys_Lock(_mutex);

        // We'll read as much as we have.
        int avail = availableForReading();
        length = MIN_OF(length, avail);

        const int remainder = _end - _readPos;
        if (length <= remainder)
        {
            memcpy(data, _readPos, length);
            _readPos += length;
            if (_readPos == _end) _readPos = _buf; // May wrap around.
        }
        else
        {
            memcpy(data, _readPos, remainder);
            memcpy((byte*)data + remainder, _buf, length - remainder);
            _readPos = _buf + length - remainder;
        }

        Sys_Unlock(_mutex);

        // This is how much we were able to read.
        return length;
    }

private:
    mutex_t _mutex;
    byte* _buf;
    byte* _end;
    int _size;
    byte* _writePos;
    byte* _readPos;
};

static RingBuffer* blockBuffer;
static float musicVolume = 1.0f;

static void setSynthGain(float vol)
{
    fluid_synth_set_gain(DMFluid_Synth(), vol * MAX_SYNTH_GAIN);
}

/**
 * Thread entry point for the synthesizer. Runs until the song is stopped.
 * @param parm  Not used.
 * @return Always zero.
 */
static int synthWorkThread(void* parm)
{
    DE_UNUSED(parm);
    DE_ASSERT(blockBuffer != 0);

    byte samples[BLOCK_SIZE];

    while (!workerShouldStop)
    {
        if (blockBuffer->availableForWriting() < BLOCK_SIZE)
        {
            // We should not or cannot produce samples right now, let's sleep for a while.
            Thread_Sleep(50);
            continue;
        }

        //DSFLUIDSYNTH_TRACE("Synthesizing next block using fsPlayer " << fsPlayer);

        // Synthesize a block of samples into our buffer.
        fluid_synth_write_s16(DMFluid_Synth(), BLOCK_SAMPLES, samples, 0, 2, samples, 1, 2);
        blockBuffer->write(samples, BLOCK_SIZE);

        //DSFLUIDSYNTH_TRACE("Block written.");
    }

    DSFLUIDSYNTH_TRACE("Synth worker dies.");
    return 0;
}

/**
 * Callback function for streaming out data to the SFX buffer. This is called
 * by the SFX driver when it wants more samples.
 *
 * @param buf   Buffer where the samples are being played in.
 * @param data  Data buffer for writing samples into.
 * @param size  Number of bytes to write.
 *
 * @return  Number of bytes written to @a data, or 0 if there are less than
 * the requested amount of data available.
 */
static int streamOutSamples(sfxbuffer_t* buf, void* data, unsigned int size)
{
    DE_UNUSED(buf);
    DE_ASSERT(buf == sfxBuf);

    if (blockBuffer->availableForReading() >= int(size))
    {
        //DSFLUIDSYNTH_TRACE("Streaming out " << size << " bytes.");
        blockBuffer->read(data, size);
        return size;
    }
    else
    {
        //DSFLUIDSYNTH_TRACE("Streaming out failed.");
        return 0; // Not enough data to fill the requested buffer.
    }
}

static void startWorker()
{
    DE_ASSERT(DMFluid_Driver() == NULL);
    DE_ASSERT(worker == NULL);

    workerShouldStop = false;
    worker = Sys_StartThread(synthWorkThread, nullptr, nullptr);
}

/**
 * Starts the synthesizer thread and music playback.
 */
static void startPlayer()
{
    if (DMFluid_Driver()) return;

    DE_ASSERT(!worker);
    DE_ASSERT(sfxBuf == NULL);

    // Create a sound buffer for playing the music.
    sfxBuf = DMFluid_Sfx()->Create(SFXBF_STREAM, 16, 44100);

    DSFLUIDSYNTH_TRACE("startPlayer: Created SFX buffer " << sfxBuf);

    // As a streaming buffer, the data will be read from here.
    // The length of the buffer is ignored, streaming buffers play indefinitely.
    memset(&streamSample, 0, sizeof(streamSample));
    streamSample.id = -1; // undefined sample
    streamSample.data = reinterpret_cast<void*>(streamOutSamples);
    streamSample.bytesPer = 2;
    streamSample.numSamples = MAX_BLOCKS * BLOCK_SAMPLES;
    streamSample.rate = 44100;

    DMFluid_Sfx()->Load(sfxBuf, &streamSample);

    startWorker();

    // Volume applied immediately.
    DMFluid_Sfx()->Set(sfxBuf, SFXBP_VOLUME, musicVolume);
    setSynthGain(1.0f);

    DMFluid_Sfx()->Play(sfxBuf);
}

static void stopWorker()
{
    DE_ASSERT(DMFluid_Driver() == NULL);

    if (worker)
    {
        DSFLUIDSYNTH_TRACE("stopWorker: Stopping thread " << worker);

        workerShouldStop = true;
        Sys_WaitThread(worker, 1000, NULL);
        worker = 0;

        DSFLUIDSYNTH_TRACE("stopWorker: Thread stopped.");
    }
}

static void stopPlayer()
{
    DSFLUIDSYNTH_TRACE("stopPlayer: fsPlayer " << fsPlayer);
    if (!fsPlayer) return;

    if (!DMFluid_Driver())
    {
        stopWorker();

        // Destroy the sfx buffer.
        DE_ASSERT(sfxBuf != 0);
        DSFLUIDSYNTH_TRACE("stopPlayer: Destroying SFX buffer " << sfxBuf);

        DMFluid_Sfx()->Destroy(sfxBuf);
        sfxBuf = 0;
    }

    delete_fluid_player(fsPlayer);
    fsPlayer = 0;

    blockBuffer->clear();

    fluid_synth_system_reset(DMFluid_Synth());
}

int fluidsynth_DM_Music_Init(void)
{
    if (blockBuffer) return true;

    musicVolume = 1.f;
    blockBuffer = new RingBuffer(MAX_BLOCKS * BLOCK_SIZE);
    return true;
}

void DMFluid_Shutdown(void)
{
    if (!blockBuffer) return;

    stopPlayer();

    delete blockBuffer; blockBuffer = 0;

    if (fsPlayer)
    {
        delete_fluid_player(fsPlayer);
        fsPlayer = 0;
    }

    DSFLUIDSYNTH_TRACE("Music_Shutdown.");
}

void fluidsynth_DM_Music_Shutdown(void)
{
    DMFluid_Shutdown();
}

void DMFluid_SetSoundFont(const char* fileName)
{
    if (sfontId >= 0)
    {
        // First unload the previous font.
        fluid_synth_sfunload(DMFluid_Synth(), sfontId, false);
        sfontId = -1;
    }

    if (!fileName) return;

    // Load the new one.
    sfontId = fluid_synth_sfload(DMFluid_Synth(), fileName, true);
    if (sfontId >= 0)
    {
        App_Log(DE2_LOG_VERBOSE, "FluidSynth: Loaded SF2 soundfont \"%s\" with id:%i", fileName, sfontId);
    }
    else
    {
        App_Log(DE2_LOG_VERBOSE, "FluidSynth: Failed to load soundfont \"%s\" (not SF2 or not found)", fileName);
    }
}

void fluidsynth_DM_Music_Set(int prop, float value)
{
    switch (prop)
    {
    case MUSIP_VOLUME:
        musicVolume = value;
        if (sfxBuf)
        {
            // This will take effect immediately.
            DMFluid_Sfx()->Set(sfxBuf, SFXBP_VOLUME, musicVolume);
        }
        else
        {
            // Effect will be heard only after buffered samples have been played.
            setSynthGain(musicVolume);
        }
        DSFLUIDSYNTH_TRACE("Music_Set: MUSIP_VOLUME = " << musicVolume);
        break;

    default:
        break;
    }
}

int fluidsynth_DM_Music_Get(int prop, void* ptr)
{
    switch (prop)
    {
    case MUSIP_ID:
        if (ptr)
        {
            strcpy((char*) ptr, "FluidSynth/Ext (MIDI only)");
            return true;
        }
        break;

    case MUSIP_PLAYING: {
        if (!fsPlayer) return false;
        int playing = (fluid_player_get_status(fsPlayer) == FLUID_PLAYER_PLAYING);
        DSFLUIDSYNTH_TRACE("Music_Get: MUSIP_PLAYING = " << playing);
        return playing;
    }

    default:
        break;
    }

    return false;
}

/**
 * Get the buffered output and stream it to the Sfx interface.
 */
void DMFluid_Update(void)
{
    // nothing to do
}

void fluidsynth_DM_Music_Update(void)
{
    DMFluid_Update();
}

void fluidsynth_DM_Music_Stop(void)
{
    stopPlayer();
}

void fluidsynth_DM_Music_Pause(int setPause)
{
    if (!fsPlayer || !sfxBuf) return;

    if (setPause)
    {
        DMFluid_Sfx()->Stop(sfxBuf);
        DSFLUIDSYNTH_TRACE("Song paused.");
    }
    else
    {
        DMFluid_Sfx()->Play(sfxBuf);
        DSFLUIDSYNTH_TRACE("Song resumed.");
    }
}

int fluidsynth_DM_Music_PlayFile(const char *filename, int looped)
{
    if (!filename) return false;

#if defined (WIN32)
    // Using MinGW.
    de::String pathStr = filename;
    pathStr.replace("\\", "/");
    const de::String path = pathStr;
#else
    const de::String path = filename;
#endif

    if (!fluid_is_midifile(path))
    {
        // It doesn't look like MIDI.
        App_Log(DE2_LOG_VERBOSE, "[FluidSynth] Cannot play \"%s\": not a MIDI file", path.c_str());
        return false;
    }

    if (sfontId < 0)
    {
        App_Log(DE2_LOG_VERBOSE, "[FluidSynth] Cannot play \"%s\" without an SF2 soundfont", path.c_str());
        return false;
    }

    // If we are playing something, make sure it's stopped.
    stopPlayer();

    DE_ASSERT(fsPlayer == NULL);

    // Create a new player.
    fsPlayer = new_fluid_player(DMFluid_Synth());
    fluid_player_add(fsPlayer, path);
    fluid_player_set_loop(fsPlayer, looped? -1 /*infinite times*/ : 1);
    fluid_player_play(fsPlayer);

    startPlayer();

    DSFLUIDSYNTH_TRACE("PlayFile: playing '" << path << "' using player "
                       << fsPlayer << " looped:" << looped << " sfont:" << sfontId);
    return true;
}
