/**
 * @file fluidsynth_music.cpp
 * Music playback interface. @ingroup dsfluidsynth
 *
 * @authors Copyright © 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <string.h>
#include <de/concurrency.h>
#include <vector>

static int sfontId = -1;
static fluid_player_t* fsPlayer = 0;
static thread_t worker;
static volatile bool workerShouldStop;
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
        delete _buf;
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
        if(_writePos >= _readPos)
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

        DENG_ASSERT(_writePos < _end);

        // No need to split?
        const int remainder = _end - _writePos;
        if(length <= remainder)
        {
            memcpy(_writePos, data, length);
            _writePos += length;
            if(_writePos == _end) _writePos = _buf; // May wrap around.
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
        if(length <= remainder)
        {
            memcpy(data, _readPos, length);
            _readPos += length;
            if(_readPos == _end) _readPos = _buf; // May wrap around.
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
static float musicVolume;

/**
 * Thread entry point for the synthesizer. Runs until the song is stopped.
 * @param parm  Not used.
 * @return Always zero.
 */
static int synthWorkThread(void* parm)
{
    DENG_UNUSED(parm);
    DENG_ASSERT(blockBuffer != 0);

    byte samples[BLOCK_SIZE];

    while(!workerShouldStop)
    {
        if(blockBuffer->availableForWriting() < BLOCK_SIZE)
        {
            // There's no room for the next block, let's sleep for a while.
            Thread_Sleep(50);
            continue;
        }

        //DSFLUIDSYNTH_TRACE("Synthesizing next block...");

        // Synthesize a block of samples into our buffer.
        fluid_synth_write_s16(DMFluid_Synth(), BLOCK_SAMPLES, samples, 0, 2, samples, 1, 2);
        blockBuffer->write(samples, BLOCK_SIZE);

        //DSFLUIDSYNTH_TRACE("Block written.");
    }
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
    DENG_UNUSED(buf);
    DENG_ASSERT(buf == sfxBuf);

    if(blockBuffer->availableForReading() >= int(size))
    {
        //DSFLUIDSYNTH_TRACE("Streaming out " << size << " bytes.");
        blockBuffer->read(data, size);
        return size;
    }
    else
    {
        return 0; // Not enough data to fill the requested buffer.
    }
}

/**
 * Starts the synthesizer thread and music playback.
 */
static void startPlayer()
{
    DENG_ASSERT(!worker);
    DENG_ASSERT(sfxBuf == NULL);

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

    workerShouldStop = false;
    worker = Sys_StartThread(synthWorkThread, 0);

    // Update the buffer's volume.
    DMFluid_Sfx()->Set(sfxBuf, SFXBP_VOLUME, musicVolume);

    DMFluid_Sfx()->Play(sfxBuf);
}

static void stopPlayer()
{
    if(!fsPlayer) return;

    // Destroy the sfx buffer.
    DENG_ASSERT(sfxBuf != 0);
    DSFLUIDSYNTH_TRACE("stopPlayer: Destroying SFX buffer " << sfxBuf);
    DMFluid_Sfx()->Destroy(sfxBuf);
    sfxBuf = 0;

    if(worker)
    {
        DSFLUIDSYNTH_TRACE("stopPlayer: Stopping thread " << worker);

        workerShouldStop = true;
        Sys_WaitThread(worker, 1000);
        worker = 0;

        DSFLUIDSYNTH_TRACE("stopPlayer: Thread stopped.");
    }

    DSFLUIDSYNTH_TRACE("stopPlayer: " << fsPlayer);
    delete_fluid_player(fsPlayer);
    fsPlayer = 0;

    blockBuffer->clear();
}

int DM_Music_Init(void)
{
    musicVolume = 1.f;
    blockBuffer = new RingBuffer(MAX_BLOCKS * BLOCK_SIZE);

    //soundFontFileName = 0; // empty for the default
    return true; //fmodSystem != 0;
}

void DMFluid_Shutdown(void)
{
    stopPlayer();

    delete blockBuffer; blockBuffer = 0;

    if(fsPlayer)
    {
        delete_fluid_player(fsPlayer);
        fsPlayer = 0;
    }

    DSFLUIDSYNTH_TRACE("Music_Shutdown.");
}

void DM_Music_Shutdown(void)
{
    DMFluid_Shutdown();
}

void DMFluid_SetSoundFont(const char* fileName)
{
    if(sfontId >= 0)
    {
        // First unload the previous font.
        fluid_synth_sfunload(DMFluid_Synth(), sfontId, false);
    }

    if(!fileName) return;

    // Load the new one.
    sfontId = fluid_synth_sfload(DMFluid_Synth(), fileName, true);
    if(sfontId >= 0)
    {
        Con_Message("Loaded soundfont \"%s\" with id:%i.\n", fileName, sfontId);
    }
    else
    {
        Con_Message("Failed to load soundfont \"%s\".\n", fileName);
    }
}

void DM_Music_Set(int prop, float value)
{
    switch(prop)
    {
    case MUSIP_VOLUME:
        musicVolume = value;
        if(sfxBuf)
        {
            DMFluid_Sfx()->Set(sfxBuf, SFXBP_VOLUME, musicVolume);
        }
        DSFLUIDSYNTH_TRACE("Music_Set: MUSIP_VOLUME = " << musicVolume);
        break;

    default:
        break;
    }
}

int DM_Music_Get(int prop, void* ptr)
{
    switch(prop)
    {
    case MUSIP_ID:
        if(ptr)
        {
            strcpy((char*) ptr, "FluidSynth/Ext");
            return true;
        }
        break;

    case MUSIP_PLAYING: {
        if(!fsPlayer) return false;
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

void DM_Music_Update(void)
{
    DMFluid_Update();
}

void DM_Music_Stop(void)
{
    if(!fsPlayer) return;

    DMFluid_Sfx()->Stop(sfxBuf);
    fluid_player_stop(fsPlayer);
}

#if 0
int DM_Music_Play(int looped)
{
    if(!fmodSystem) return false;

    if(songBuffer)
    {
        // Get rid of the old song.
        releaseSong();

        setDefaultStreamBufferSize();

        FMOD_CREATESOUNDEXINFO extra;
        zeroStruct(extra);
        extra.length = songBuffer->size;
        if(endsWith(soundFontFileName, ".dls"))
        {
            extra.dlsname = soundFontFileName;
        }

        // Load a new song.
        FMOD_RESULT result;
        result = fmodSystem->createSound(songBuffer->data,
                                         FMOD_CREATESTREAM | FMOD_OPENMEMORY |
                                         (looped? FMOD_LOOP_NORMAL : 0),
                                         &extra, &song);
        DSFMOD_TRACE("Music_Play: songBuffer has " << songBuffer->size << " bytes, created Sound " << song);
        DSFMOD_ERRCHECK(result);

        needReleaseSong = true;

        // The song buffer remains in memory, in case FMOD needs to stream from it.
    }
    return startSong();
    return 0;
}
#endif

void DM_Music_Pause(int setPause)
{
    if(!fsPlayer) return;

    if(setPause)
    {
        DMFluid_Sfx()->Stop(sfxBuf);
    }
    else
    {
        DMFluid_Sfx()->Play(sfxBuf);
    }
}

int DM_Music_PlayFile(const char *filename, int looped)
{
    if(sfontId < 0)
    {
        Con_Message("Cannot play \"%s\" without a soundfont. Define one with the cvar 'music-soundfont'.\n", filename);
        return false;
    }

    // If we are playing something, make sure it's stopped.
    stopPlayer();

    DENG_ASSERT(fsPlayer == NULL);

    if(!fluid_is_midifile(filename))
    {
        // It doesn't look like MIDI.
        Con_Message("Cannot play \"%s\": not a MIDI file.\n", filename);
        return false;
    }

    // Create a new player.
    fsPlayer = new_fluid_player(DMFluid_Synth());
    fluid_player_add(fsPlayer, filename);
    fluid_player_set_loop(fsPlayer, looped? -1 /*infinite times*/ : 1);
    fluid_player_play(fsPlayer);

    startPlayer();

    DSFLUIDSYNTH_TRACE("PlayFile: playing '" << filename << "' using player " << fsPlayer << " looped:" << looped);
    return true;
}
