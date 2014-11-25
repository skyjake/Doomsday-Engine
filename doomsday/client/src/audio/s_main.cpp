/** @file s_main.cpp  Audio Subsystem.
 *
 * Interface to the Sfx and Mus modules. High-level (and exported) audio control.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#define DENG_NO_API_MACROS_SOUND

#include "audio/s_main.h"

#ifdef __CLIENT__
#  include <de/concurrency.h>
#endif
#include <doomsday/audio/logical.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>

#ifdef __CLIENT__
#  include "audio/audiodriver.h"
#endif
#include "audio/s_cache.h"
#include "audio/s_mus.h"
#include "audio/s_sfx.h"
#include "audio/sys_audio.h"

#include "world/p_players.h"
#include "Sector"

#include "dd_main.h" // isDedicated
#include "m_profiler.h"

#ifdef __CLIENT__
#  include "gl/gl_main.h"
#  include "ui/clientwindow.h"
#endif
#ifdef __SERVER__
#  include "server/sv_sound.h"
#endif

using namespace de;

BEGIN_PROF_TIMERS()
  PROF_SOUND_STARTFRAME
END_PROF_TIMERS()

audiodriver_t *audioDriver;

int showSoundInfo;
int soundMinDist = 256; // No distance attenuation this close.
int soundMaxDist = 2025;

// Setting these variables is enough to adjust the volumes.
// S_StartFrame() will call the actual routines to change the volume
// when there are changes.
int sfxVolume = 255;
int musVolume = 255;

int sfxBits = 8;
int sfxRate = 11025;

byte sfxOneSoundPerEmitter; // Traditional Doomsday behavior: allows sounds to overlap.

static bool noRndPitch;

dd_bool S_Init()
{
#ifdef __CLIENT__
    dd_bool sfxOK, musOK;
#endif

    Sfx_Logical_SetSampleLengthCallback(Sfx_GetSoundLength);

    if(CommandLine_Exists("-nosound") || CommandLine_Exists("-noaudio"))
        return true;

    // Disable random pitch changes?
    noRndPitch = CommandLine_Exists("-norndpitch");

#ifdef __CLIENT__
    // Try to load the audio driver plugin(s).
    if(!AudioDriver_Init())
    {
        LOG_AUDIO_NOTE("Music and sound effects are disabled");
        return false;
    }

    sfxOK = Sfx_Init();
    musOK = Mus_Init();

    if(!sfxOK || !musOK)
    {
        LOG_AUDIO_NOTE("Errors during audio subsystem initialization");
        return false;
    }
#endif

    return true;
}

void S_Shutdown()
{
#ifdef __CLIENT__
    Sfx_Shutdown();
    Mus_Shutdown();

    // Finally, close the audio driver.
    AudioDriver_Shutdown();
#endif
}

void S_MapChange()
{
    // Stop everything in the LSM.    
    Sfx_InitLogical();

#ifdef __CLIENT__
    Sfx_MapChange();
#endif
}

void S_SetupForChangedMap()
{
#ifdef __CLIENT__
    // Update who is listening now.
    Sfx_SetListener(S_GetListenerMobj());
#endif
}

void S_Reset()
{
#ifdef __CLIENT__
    Sfx_Reset();
#endif
    _api_S.StopMusic();
}

void S_StartFrame()
{
#ifdef DD_PROFILE
    static int i;
    if(++i > 40)
    {
        i = 0;
        PRINT_PROF( PROF_SOUND_STARTFRAME );
    }
#endif

BEGIN_PROF( PROF_SOUND_STARTFRAME );

#ifdef __CLIENT__
    static int oldMusVolume = -1;

    if(musVolume != oldMusVolume)
    {
        oldMusVolume = musVolume;
        Mus_SetVolume(musVolume / 255.0f);
    }

    // Update all channels (freq, 2D:pan,volume, 3D:position,velocity).
    Sfx_StartFrame();
    Mus_StartFrame();
#endif

    // Remove stopped sounds from the LSM.
    Sfx_Logical_SetOneSoundPerEmitter(sfxOneSoundPerEmitter);
    Sfx_PurgeLogical();

END_PROF( PROF_SOUND_STARTFRAME );
}

void S_EndFrame()
{
#ifdef __CLIENT__
    Sfx_EndFrame();
#endif
}

mobj_t *S_GetListenerMobj()
{
    return ddPlayers[displayPlayer].shared.mo;
}

sfxinfo_t *S_GetSoundInfo(int soundID, float *freq, float *volume)
{
    if(soundID <= 0 || soundID >= defs.sounds.size())
        return nullptr;

    float dummy = 0;
    if(!freq) freq = &dummy;
    if(!volume) volume = &dummy;

    /**
     * Traverse all links when getting the definition.
     * (But only up to 10, which is certainly enough and prevents endless
     * recursion.) Update the sound id at the same time.
     * The links were checked in Def_Read() so there can't be any bogus ones.
     */
    int i;
    sfxinfo_t *info = &runtimeDefs.sounds[soundID];
    for(i = 0; info->link && i < 10;
        info     = info->link,
        *freq    = (info->linkPitch > 0   ? info->linkPitch  / 128.0f : *freq),
        *volume += (info->linkVolume != -1? info->linkVolume / 127.0f : 0),
        soundID  = runtimeDefs.sounds.indexOf(info),
        i++)
    {}

    DENG2_ASSERT(soundID < defs.sounds.size());

    return info;
}

dd_bool S_IsRepeating(int idFlags)
{
    if(idFlags & DDSF_REPEAT)
        return true;

    if(sfxinfo_t *info = S_GetSoundInfo(idFlags & ~DDSF_FLAG_MASK, nullptr, nullptr))
    {
        return (info->flags & SF_REPEAT) != 0;
    }
    return false;
}

int S_LocalSoundAtVolumeFrom(int soundIdAndFlags, mobj_t *origin, coord_t *point, float volume)
{
#ifdef __CLIENT__
    // A dedicated server never starts any local sounds (only logical sounds in the LSM).
    if(isDedicated || BusyMode_Active())
        return false;

    int soundId = (soundIdAndFlags & ~DDSF_FLAG_MASK);
    if(soundId <= 0 || soundId >= defs.sounds.size())
        return false;

    if(sfxVolume <= 0 || volume <= 0)
        return false; // This won't play...

    LOG_AS("S_LocalSoundAtVolumeFrom");

    if(volume > 1)
    {
        LOGDEV_AUDIO_WARNING("Volume is too high (%f > 1)") << volume;
    }

    float freq = 1;
    // This is the sound we're going to play.
    sfxinfo_t *info = S_GetSoundInfo(soundId, &freq, &volume);
    if(!info) return false; // Hmm? This ID is not defined.

    bool const isRepeating = S_IsRepeating(soundIdAndFlags);

    // Check the distance (if applicable).
    if(!(info->flags & SF_NO_ATTENUATION) && !(soundIdAndFlags & DDSF_NO_ATTENUATION))
    {
        // If origin is too far, don't even think about playing the sound.
        coord_t *fixPoint = (origin ? origin->origin : point);

        if(Mobj_ApproxPointDistance(S_GetListenerMobj(), fixPoint) > soundMaxDist)
            return false;
    }

    // Load the sample.
    sfxsample_t *sample = Sfx_Cache(soundId);
    if(!sample)
    {
        if(sfxAvail)
        {
            LOG_AUDIO_VERBOSE("S_LocalSoundAtVolumeFrom: Caching of sound %i failed")
                    << soundId;
        }
        return false;
    }

    // Random frequency alteration? (Multipliers chosen to match original
    // sound code.)
    if(!noRndPitch)
    {
        if(info->flags & SF_RANDOM_SHIFT)
        {
            freq += (RNG_RandFloat() - RNG_RandFloat()) * (7.0f / 255);
        }
        if(info->flags & SF_RANDOM_SHIFT2)
        {
            freq += (RNG_RandFloat() - RNG_RandFloat()) * (15.0f / 255);
        }
    }

    // If the sound has an exclusion group, either all or the same emitter's
    // iterations of this sound will stop.
    if(info->group)
    {
        mobj_t *emitter = ((info->flags & SF_GLOBAL_EXCLUDE) ? nullptr : origin);
        Sfx_StopSoundGroup(info->group, emitter);
    }

    // Let's play it.
    int flags = 0;
    flags |= (((info->flags & SF_NO_ATTENUATION) || (soundIdAndFlags & DDSF_NO_ATTENUATION)) ? SF_NO_ATTENUATION : 0);
    flags |= (isRepeating ? SF_REPEAT : 0);
    flags |= ((info->flags & SF_DONT_STOP) ? SF_DONT_STOP : 0);
    return Sfx_StartSound(sample, volume, freq, origin, point, flags);

#else
    DENG2_UNUSED4(soundIdAndFlags, origin, point, volume);
    return false;
#endif
}

int S_LocalSoundAtVolume(int soundID, mobj_t *origin, float volume)
{
    return S_LocalSoundAtVolumeFrom(soundID, origin, nullptr, volume);
}

int S_LocalSound(int soundID, mobj_t *origin)
{
    // Play local sound at max volume.
    return S_LocalSoundAtVolumeFrom(soundID, origin, nullptr, 1);
}

int S_LocalSoundFrom(int soundID, coord_t *fixedPos)
{
    return S_LocalSoundAtVolumeFrom(soundID, nullptr, fixedPos, 1);
}

int S_StartSound(int soundID, mobj_t *origin)
{
#ifdef __SERVER__
    // The sound is audible to everybody.
    Sv_Sound(soundID, origin, SVSF_TO_ALL);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    return S_LocalSound(soundID, origin);
}

int S_StartSoundEx(int soundID, mobj_t *origin)
{
#ifdef __SERVER__
    Sv_Sound(soundID, origin, SVSF_TO_ALL | SVSF_EXCLUDE_ORIGIN);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    return S_LocalSound(soundID, origin);
}

int S_StartSoundAtVolume(int soundID, mobj_t *origin, float volume)
{
#ifdef __SERVER__
    Sv_SoundAtVolume(soundID, origin, volume, SVSF_TO_ALL);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    // The sound is audible to everybody.
    return S_LocalSoundAtVolume(soundID, origin, volume);
}

int S_ConsoleSound(int soundID, mobj_t *origin, int targetConsole)
{
#ifdef __SERVER__
    Sv_Sound(soundID, origin, targetConsole);
#endif

    // If it's for us, we can hear it.
    if(targetConsole == consolePlayer)
    {
        S_LocalSound(soundID, origin);
    }

    return true;
}

/**
 * @param sectorEmitter  Sector in which to stop sounds.
 * @param soundID        Unique identifier of the sound to be stopped.
 *                       If @c 0, ID not checked.
 * @param flags          @ref soundStopFlags
 */
static void stopSectorSounds(ddmobj_base_t *sectorEmitter, int soundID, int flags)
{
    if(!sectorEmitter || !flags) return;

    // Are we stopping with this sector's emitter?
    if(flags & SSF_SECTOR)
    {
        _api_S.StopSound(soundID, (mobj_t *)sectorEmitter);
    }

    // Are we stopping with linked emitters?
    if(!(flags & SSF_SECTOR_LINKED_SURFACES)) return;

    // Process the rest of the emitter chain.
    ddmobj_base_t *base = sectorEmitter;
    while((base = (ddmobj_base_t *)base->thinker.next))
    {
        // Stop sounds from this emitter.
        _api_S.StopSound(soundID, (mobj_t *)base);
    }
}

void S_StopSound(int soundID, mobj_t *emitter)
{
#ifdef __CLIENT__
    // No special stop behavior.
    // Sfx provides a routine for this.
    Sfx_StopSound(soundID, emitter);
#endif

    // Notify the LSM.
    if(Sfx_StopLogical(soundID, emitter))
    {
#ifdef __SERVER__
        // In netgames, the server is responsible for telling clients
        // when to stop sounds. The LSM will tell us if a sound was
        // stopped somewhere in the world.
        Sv_StopSound(soundID, emitter);
#endif
    }
}

void S_StopSound2(int soundID, mobj_t *emitter, int flags)
{
    // Are we performing any special stop behaviors?
    if(emitter && flags)
    {
        if(emitter->thinker.id)
        {
            // Emitter is a real Mobj.
            stopSectorSounds(&Mobj_Sector(emitter)->soundEmitter(), soundID, flags);
            return;
        }

        // The head of the chain is the sector. Find it.
        while(emitter->thinker.prev)
        {
            emitter = (mobj_t *)emitter->thinker.prev;
        }
        stopSectorSounds((ddmobj_base_t *)emitter, soundID, flags);
        return;
    }

    // A regular stop.
    S_StopSound(soundID, emitter);
}

int S_IsPlaying(int soundID, mobj_t *emitter)
{
    // The Logical Sound Manager (under Sfx) provides a routine for this.
    return Sfx_IsPlaying(soundID, emitter);
}

int S_StartMusicNum(int id, dd_bool looped)
{
#ifdef __CLIENT__

    // Don't play music if the volume is at zero.
    if(isDedicated) return true;

    if(id < 0 || id >= defs.musics.size()) return false;
    Record const *def = &defs.musics[id];

    LOG_AUDIO_MSG("Starting music '%s'") << def->gets("id");

    return Mus_Start(def, looped);

#else
    DENG2_UNUSED2(id, looped);
    return false;
#endif
}

int S_StartMusic(char const *musicID, dd_bool looped)
{
    LOG_AS("S_StartMusic");
    int idx = Def_GetMusicNum(musicID);
    if(idx < 0)
    {
        LOG_AUDIO_WARNING("Song \"%s\" not defined, cannot start playback") << musicID;
        return false;
    }
    return S_StartMusicNum(idx, looped);
}

void S_StopMusic()
{
#ifdef __CLIENT__
    Mus_Stop();
#endif
}

void S_PauseMusic(dd_bool paused)
{
#ifdef __CLIENT__
    Mus_Pause(paused);
#else
    DENG2_UNUSED(paused);
#endif
}

void S_Drawer()
{
#ifdef __CLIENT__
    if(!showSoundInfo) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);

    Sfx_DebugInfo();

    // Back to the original.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
#endif // __CLIENT__
}

/**
 * Console command for playing a (local) sound effect.
 */
D_CMD(PlaySound)
{
    DENG2_UNUSED(src);

    if(argc < 2)
    {
        LOG_SCR_NOTE("Usage: %s (id) (volume) at (x) (y) (z)") << argv[0];
        LOG_SCR_MSG("(volume) must be in 0..1, but may be omitted.");
        LOG_SCR_MSG("'at (x) (y) (z)' may also be omitted.");
        LOG_SCR_MSG("The sound is always played locally.");
        return true;
    }
    int p = 0;

    // The sound ID is always first.
    int const id = Def_GetSoundNum(argv[1]);

    // The second argument may be a volume.
    float volume = 1;
    if(argc >= 3 && String(argv[2]).compareWithoutCase("at"))
    {
        volume = String(argv[2]).toFloat();
        p = 3;
    }
    else
    {
        p = 2;
    }

    bool useFixedPos = false;
    coord_t fixedPos[3];
    if(argc >= p + 4 && !String(argv[p]).compareWithoutCase("at"))
    {
        useFixedPos = true;
        fixedPos[VX] = strtod(argv[p + 1], nullptr);
        fixedPos[VY] = strtod(argv[p + 2], nullptr);
        fixedPos[VZ] = strtod(argv[p + 3], nullptr);
    }

    // Check that the volume is valid.
    volume = de::clamp(0.f, volume, 1.f);
    if(de::fequal(volume, 0)) return true;

    if(useFixedPos)
    {
        S_LocalSoundAtVolumeFrom(id, nullptr, fixedPos, volume);
    }
    else
    {
        S_LocalSoundAtVolume(id, nullptr, volume);
    }

    return true;
}

#ifdef __CLIENT__
static void S_ReverbVolumeChanged()
{
    Sfx_UpdateReverb();
}
#endif

void S_Register()
{
    C_VAR_BYTE  ("sound-overlap-stop",  &sfxOneSoundPerEmitter, 0, 0, 1);

#ifdef __CLIENT__
    C_VAR_INT   ("sound-volume",        &sfxVolume,             0, 0, 255);
    C_VAR_INT   ("sound-info",          &showSoundInfo,         0, 0, 1);
    C_VAR_INT   ("sound-rate",          &sfxSampleRate,         0, 11025, 44100);
    C_VAR_INT   ("sound-16bit",         &sfx16Bit,              0, 0, 1);
    C_VAR_INT   ("sound-3d",            &sfx3D,                 0, 0, 1);
    C_VAR_FLOAT2("sound-reverb-volume", &sfxReverbStrength,     0, 0, 1.5f, S_ReverbVolumeChanged);

    C_CMD_FLAGS("playsound", nullptr, PlaySound, CMDF_NO_DEDICATED);

    Mus_Register();
#endif
}

DENG_DECLARE_API(S) =
{
    { DE_API_SOUND },
    S_MapChange,
    S_LocalSoundAtVolumeFrom,
    S_LocalSoundAtVolume,
    S_LocalSound,
    S_LocalSoundFrom,
    S_StartSound,
    S_StartSoundEx,
    S_StartSoundAtVolume,
    S_ConsoleSound,
    S_StopSound,
    S_StopSound2,
    S_IsPlaying,
    S_StartMusic,
    S_StartMusicNum,
    S_StopMusic,
    S_PauseMusic
};
