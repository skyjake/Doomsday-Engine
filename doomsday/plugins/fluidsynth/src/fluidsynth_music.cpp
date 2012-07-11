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

#define MAX_BLOCKS          8
#define SAMPLES_PER_SECOND  44100
#define BLOCK_SAMPLES       (SAMPLES_PER_SECOND/8)
#define BYTES_PER_SAMPLE    2
#define BLOCK_SIZE          (2 * BYTES_PER_SAMPLE * BLOCK_SAMPLES) // 16 bit

static byte* blockBuffer;
static bool blockReady[MAX_BLOCKS];
static int nextBlock;

struct SongBuffer
{
    int size;
    char* data;

    SongBuffer(int newSize) : size(newSize) {
        data = new char[newSize];
    }

    ~SongBuffer() {
        delete [] data;
    }
};

#if 0
static FMOD::Sound* song;
static FMOD::Channel* music;
static bool needReleaseSong;
static float musicVolume;
#endif
static SongBuffer* songBuffer;
//static const char* soundFontFileName;

#if 0
static FMOD_RESULT F_CALLBACK
musicCallback(FMOD_CHANNEL* chanPtr, FMOD_CHANNEL_CALLBACKTYPE type,
              void* /*commanddata1*/, void* /*commanddata2*/)
{
    if(reinterpret_cast<FMOD::Channel*>(chanPtr) != music)
        return FMOD_OK; // Safety check.

    switch(type)
    {
    case FMOD_CHANNEL_CALLBACKTYPE_END:
        // The music has stopped.
        music = 0;
        break;

    default:
        break;
    }
    return FMOD_OK;
}

static void releaseSong()
{
    if(song)
    {
        if(needReleaseSong)
        {
            DSFMOD_TRACE("releaseSong: Song " << song << " will be released.")
            song->release();
        }
        else
        {
            DSFMOD_TRACE("releaseSong: Song " << song << " will NOT be released.");
        }
        song = 0;
        needReleaseSong = false;
    }
    music = 0;
}
#endif

static void releaseSongBuffer()
{
    if(songBuffer)
    {
        delete songBuffer;
        songBuffer = 0;
    }
}


static int synthThread(void* parm)
{
    DENG_UNUSED(parm);
    DENG_ASSERT(blockBuffer != 0);

    int block = 0;
    while(!workerShouldStop)
    {
        if(blockReady[block])
        {
            // The block we're about to write to hasn't been streamed out yet.
            // Let's wait a while.
            Thread_Sleep(50);
            continue;
        }

        DSFLUIDSYNTH_TRACE("Synthesizing block " << block << "...");

        // Synthesize a block of samples into our buffer.
        void* start = blockBuffer + block * BLOCK_SIZE;

        // Synthesize a portion of the music.
        fluid_synth_write_s16(DMFluid_Synth(), BLOCK_SAMPLES, start, 0, 2, start, 1, 2);

        DSFLUIDSYNTH_TRACE("Block " << block << " marked ready.");

        // It can now be streamed out.
        blockReady[block] = true;

        // Advance to next block.
        block = (block + 1) % MAX_BLOCKS;
    }
    return 0;
}

/**
 * Starts the synthesizer thread.
 */
static void startPlayer()
{
    DENG_ASSERT(!worker);

    workerShouldStop = false;
    worker = Sys_StartThread(synthThread, 0);
}

static void stopPlayer()
{
    if(!fsPlayer) return;

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
}

#if 0
void setDefaultStreamBufferSize()
{
    if(!fmodSystem) return;

    FMOD_RESULT result;
    result = fmodSystem->setStreamBufferSize(16*1024, FMOD_TIMEUNIT_RAWBYTES);
    DSFMOD_ERRCHECK(result);
}
#endif

int DM_Music_Init(void)
{
#if 0
    music = 0;
    song = 0;
    needReleaseSong = false;
    musicVolume = 1.f;
#endif
    songBuffer = 0;

    blockBuffer = new byte[MAX_BLOCKS * BLOCK_SIZE];

    //soundFontFileName = 0; // empty for the default
    return true; //fmodSystem != 0;
}

void DMFluid_Shutdown(void)
{
    stopPlayer();

    delete [] blockBuffer;
    blockBuffer = 0;

    memset(blockReady, 0, sizeof(blockReady));
    nextBlock = 0;

    if(fsPlayer)
    {
        delete_fluid_player(fsPlayer);
        fsPlayer = 0;
    }

    releaseSongBuffer();
    //soundFontFileName = 0;

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
#if 0
    if(!fmodSystem)
        return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        musicVolume = value;
        if(music) music->setVolume(musicVolume);
        DSFMOD_TRACE("Music_Set: MUSIP_VOLUME = " << musicVolume);
        break;

    default:
        break;
    }
#endif
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

    case MUSIP_PLAYING:
        return fsPlayer != 0;

    default:
        break;
    }

    return false;
}

void DMFluid_Update(void)
{
    // TODO: get the buffered output and stream it to the Sfx interface

}

void DM_Music_Update(void)
{
    DMFluid_Update();
}

void DM_Music_Stop(void)
{
#if 0
    if(!fmodSystem || !music) return;

    DSFMOD_TRACE("Music_Stop.");

    music->stop();
#endif
    if(!fsPlayer) return;

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
#if 0
    if(!fmodSystem || !music) return;

    music->setPaused(setPause != 0);
#endif
    if(!fsPlayer) return;

    // stop/play?
}

#if 0
void* DM_Music_SongBuffer(unsigned int length)
{
    releaseSongBuffer();

    DSFLUIDSYNTH_TRACE("Music_SongBuffer: Allocating a song buffer for " << length << " bytes.");

    // The caller will put data in this buffer. Before playing, we will create
    // the FluidSynth sound based on the data in the song buffer.

    songBuffer = new SongBuffer(length);
    return songBuffer->data;
}
#endif

int DM_Music_PlayFile(const char *filename, int looped)
{
    if(sfontId < 0)
    {
        Con_Message("Cannot play \"%s\" without a soundfont. Define one with the cvar 'music-soundfont'.\n", filename);
        return false;
    }

    // If we are playing something, make sure it's stopped.
    stopPlayer();

    // Get rid of the current song.
    //releaseSong();
    //releaseSongBuffer();

    DENG_ASSERT(fsPlayer == NULL);

    // Create a new player.
    fsPlayer = new_fluid_player(DMFluid_Synth());
    fluid_player_add(fsPlayer, filename);
    fluid_player_play(fsPlayer);

    startPlayer();

    DSFLUIDSYNTH_TRACE("PlayFile: playing '" << filename << "' using player " << fsPlayer);
    return true;
}
