/** @file s_main.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * Sound Subsystem.
 *
 * Interface to the Sfx and Mus modules.
 * High-level (and exported) sound control.
 */

// HEADER FILES ------------------------------------------------------------

#define DENG_NO_API_MACROS_SOUND

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"
#include "de_defs.h"

#include "audio/sys_audio.h"
#include "map/p_players.h"
#include "map/bspleaf.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
  PROF_SOUND_STARTFRAME
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(PlaySound);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

audiodriver_t* audioDriver = NULL;

int showSoundInfo = false;
int soundMinDist = 256; // No distance attenuation this close.
int soundMaxDist = 2025;

// Setting these variables is enough to adjust the volumes.
// S_StartFrame() will call the actual routines to change the volume
// when there are changes.
int sfxVolume = 255, musVolume = 255;

int sfxBits = 8;
int sfxRate = 11025;

byte sfxOneSoundPerEmitter = false; // Traditional Doomsday behavior: allows sounds to overlap.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean noRndPitch;

// CODE --------------------------------------------------------------------

#ifdef __CLIENT__
static void S_ReverbVolumeChanged(void)
{
    Sfx_UpdateReverb();
}
#endif

void S_Register(void)
{
    C_VAR_BYTE("sound-overlap-stop", &sfxOneSoundPerEmitter, 0, 0, 1);

#ifdef __CLIENT__
    // Cvars
    C_VAR_INT("sound-volume", &sfxVolume, 0, 0, 255);
    C_VAR_INT("sound-info", &showSoundInfo, 0, 0, 1);
    C_VAR_INT("sound-rate", &sfxSampleRate, 0, 11025, 44100);
    C_VAR_INT("sound-16bit", &sfx16Bit, 0, 0, 1);
    C_VAR_INT("sound-3d", &sfx3D, 0, 0, 1);
    C_VAR_FLOAT2("sound-reverb-volume", &sfxReverbStrength, 0, 0, 10, S_ReverbVolumeChanged);

    // Ccmds
    C_CMD_FLAGS("playsound", NULL, PlaySound, CMDF_NO_DEDICATED);

    Mus_Register();
#endif
}

boolean S_Init(void)
{
#ifdef __CLIENT__
    boolean sfxOK, musOK;
#endif

    if(CommandLine_Exists("-nosound") || CommandLine_Exists("-noaudio"))
        return true;

    // Disable random pitch changes?
    noRndPitch = CommandLine_Exists("-norndpitch");

#ifdef __CLIENT__
    // Try to load the audio driver plugin(s).
    if(!AudioDriver_Init())
    {
        Con_Message("Music and Sound Effects disabled.");
        return false;
    }

    sfxOK = Sfx_Init();
    musOK = Mus_Init();

    if(!sfxOK || !musOK)
    {
        Con_Message("Errors during audio subsystem initialization.");
        return false;
    }
#endif

    return true;
}

void S_Shutdown(void)
{
#ifdef __CLIENT__
    Sfx_Shutdown();
    Mus_Shutdown();

    // Finally, close the audio driver.
    AudioDriver_Shutdown();
#endif
}

#undef S_MapChange
void S_MapChange(void)
{
    // Stop everything in the LSM.
    Sfx_InitLogical();

#ifdef __CLIENT__
    Sfx_MapChange();
#endif

    S_ResetReverb();
}

void S_SetupForChangedMap(void)
{
#ifdef __CLIENT__
    // Update who is listening now.
    Sfx_SetListener(S_GetListenerMobj());
#endif
}

void S_Reset(void)
{
#ifdef __CLIENT__
    Sfx_Reset();
#endif
    _api_S.StopMusic();
    S_ResetReverb();
}

void S_StartFrame(void)
{
#ifdef DD_PROFILE
    static int          i;
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
    Sfx_PurgeLogical();

END_PROF( PROF_SOUND_STARTFRAME );
}

void S_EndFrame(void)
{
#ifdef __CLIENT__
    Sfx_EndFrame();
#endif
}

/**
 * Usually the display player.
 */
mobj_t* S_GetListenerMobj(void)
{
    return ddPlayers[displayPlayer].shared.mo;
}

sfxinfo_t* S_GetSoundInfo(int soundID, float* freq, float* volume)
{
    float               dummy = 0;
    sfxinfo_t*          info;
    int                 i;

    if(soundID <= 0 || soundID >= defs.count.sounds.num)
        return NULL;

    if(!freq)
        freq = &dummy;
    if(!volume)
        volume = &dummy;

    /**
     * Traverse all links when getting the definition.
     * (But only up to 10, which is certainly enough and prevents endless
     * recursion.) Update the sound id at the same time.
     * The links were checked in Def_Read() so there can't be any bogus ones.
     */
    for(info = &sounds[soundID], i = 0; info->link && i < 10;
        info = info->link, *freq =
        (info->linkPitch > 0 ? info->linkPitch / 128.0f : *freq), *volume +=
        (info->linkVolume != -1 ? info->linkVolume / 127.0f : 0), soundID =
        info - sounds, i++) {}

    assert(soundID < defs.count.sounds.num);

    return info;
}

/**
 * @return              @c true, if the specified ID is a repeating sound.
 */
boolean S_IsRepeating(int idFlags)
{
    sfxinfo_t*          info;

    if(idFlags & DDSF_REPEAT)
        return true;

    if(!(info = S_GetSoundInfo(idFlags & ~DDSF_FLAG_MASK, NULL, NULL)))
        return false;
    else
        return (info->flags & SF_REPEAT) != 0;
}

#undef S_LocalSoundAtVolumeFrom
int S_LocalSoundAtVolumeFrom(int soundIdAndFlags, mobj_t *origin,
                             coord_t *point, float volume)
{
#ifdef __CLIENT__

    int                 soundId = (soundIdAndFlags & ~DDSF_FLAG_MASK);
    sfxsample_t*        sample;
    sfxinfo_t*          info;
    float               freq = 1;
    int                 result;
    boolean             isRepeating = false;

    // A dedicated server never starts any local sounds (only logical sounds in the LSM).
    if(isDedicated || BusyMode_Active())
        return false;

    if(soundId <= 0 || soundId >= defs.count.sounds.num || sfxVolume <= 0 ||
       volume <= 0)
        return false; // This won't play...

#if _DEBUG
    if(volume > 1)
    {
        Con_Message("S_LocalSoundAtVolumeFrom: Warning! Too high volume (%f).", volume);
    }
#endif

    // This is the sound we're going to play.
    if((info = S_GetSoundInfo(soundId, &freq, &volume)) == NULL)
    {
        return false; // Hmm? This ID is not defined.
    }

    isRepeating = S_IsRepeating(soundIdAndFlags);

    // Check the distance (if applicable).
    if(!(info->flags & SF_NO_ATTENUATION) &&
       !(soundIdAndFlags & DDSF_NO_ATTENUATION))
    {
        // If origin is too far, don't even think about playing the sound.
        coord_t* fixPoint = origin? origin->origin : point;

        if(Mobj_ApproxPointDistance(S_GetListenerMobj(), fixPoint) > soundMaxDist)
            return false;
    }

    // Load the sample.
    if((sample = Sfx_Cache(soundId)) == NULL)
    {
        if(sfxAvail)
        {
            VERBOSE(Con_Message
                    ("S_LocalSoundAtVolumeFrom: Sound %i " "caching failed.",
                     soundId));
        }
        return false;
    }

    // Random frequency alteration? (Multipliers chosen to match original
    // sound code.)
    if(!noRndPitch)
    {
        if(info->flags & SF_RANDOM_SHIFT)
            freq += (RNG_RandFloat() - RNG_RandFloat()) * (7.0f / 255);
        if(info->flags & SF_RANDOM_SHIFT2)
            freq += (RNG_RandFloat() - RNG_RandFloat()) * (15.0f / 255);
    }

    // If the sound has an exclusion group, either all or the same emitter's
    // iterations of this sound will stop.
    if(info->group)
    {
        Sfx_StopSoundGroup(info->group,
                           (info->flags & SF_GLOBAL_EXCLUDE)? NULL : origin);
    }

    // Let's play it.
    result =
        Sfx_StartSound(sample, volume, freq, origin, point,
                       ((info->flags & SF_NO_ATTENUATION) ||
                        (soundIdAndFlags & DDSF_NO_ATTENUATION) ?
                        SF_NO_ATTENUATION : 0) | (isRepeating ? SF_REPEAT : 0)
                       | ((info->flags & SF_DONT_STOP) ? SF_DONT_STOP : 0));

    return result;
#else
    DENG2_UNUSED4(soundIdAndFlags, origin, point, volume);
    return false;
#endif
}

#undef S_LocalSoundAtVolume
int S_LocalSoundAtVolume(int soundID, mobj_t* origin, float volume)
{
    return S_LocalSoundAtVolumeFrom(soundID, origin, NULL, volume);
}

#undef S_LocalSound
int S_LocalSound(int soundID, mobj_t* origin)
{
    // Play local sound at max volume.
    return S_LocalSoundAtVolumeFrom(soundID, origin, NULL, 1);
}

#undef S_LocalSoundFrom
int S_LocalSoundFrom(int soundID, coord_t* fixedPos)
{
    return S_LocalSoundAtVolumeFrom(soundID, NULL, fixedPos, 1);
}

#undef S_StartSound
int S_StartSound(int soundID, mobj_t* origin)
{
#ifdef __SERVER__
    // The sound is audible to everybody.
    Sv_Sound(soundID, origin, SVSF_TO_ALL);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    return S_LocalSound(soundID, origin);
}

#undef S_StartSoundEx
int S_StartSoundEx(int soundID, mobj_t* origin)
{
#ifdef __SERVER__
    Sv_Sound(soundID, origin, SVSF_TO_ALL | SVSF_EXCLUDE_ORIGIN);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    return S_LocalSound(soundID, origin);
}

#undef S_StartSoundAtVolume
int S_StartSoundAtVolume(int soundID, mobj_t* origin, float volume)
{
#ifdef __SERVER__
    Sv_SoundAtVolume(soundID, origin, volume, SVSF_TO_ALL);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    // The sound is audible to everybody.
    return S_LocalSoundAtVolume(soundID, origin, volume);
}

#undef S_ConsoleSound
int S_ConsoleSound(int soundID, mobj_t* origin, int targetConsole)
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
 * @param soundID  Unique identifier of the sound to be stopped. If @c 0, ID not checked.
 * @param flags  @ref soundStopFlags
 */
static void stopSectorSounds(ddmobj_base_t *sectorEmitter, int soundID, int flags)
{
    ddmobj_base_t *base;

    if(!sectorEmitter || !flags) return;

    // Are we stopping with this sector's emitter?
    if(flags & SSF_SECTOR)
    {
        _api_S.StopSound(soundID, (mobj_t *)sectorEmitter);
    }

    // Are we stopping with linked emitters?
    if(!(flags & SSF_SECTOR_LINKED_SURFACES)) return;

    // Process the rest of the emitter chain.
    base = sectorEmitter;
    while((base = (ddmobj_base_t *)base->thinker.next))
    {
        // Stop sounds from this emitter.
        _api_S.StopSound(soundID, (mobj_t *)base);
    }
}

#undef S_StopSound
void S_StopSound(int soundID, mobj_t* emitter)
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

#undef S_StopSound2
void S_StopSound2(int soundID, mobj_t *emitter, int flags)
{
    // Are we performing any special stop behaviors?
    if(emitter && flags)
    {
        if(emitter->thinker.id)
        {
            // Emitter is a real Mobj.
            Sector &sector = emitter->bspLeaf->sector();
            stopSectorSounds(&sector.soundEmitter(), soundID, flags);
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

#undef S_IsPlaying
int S_IsPlaying(int soundID, mobj_t* emitter)
{
    // The Logical Sound Manager (under Sfx) provides a routine for this.
    return Sfx_IsPlaying(soundID, emitter);
}

#undef S_StartMusicNum
int S_StartMusicNum(int id, boolean looped)
{
#ifdef __CLIENT__

    // Don't play music if the volume is at zero.
    if(isDedicated) return true;

    if(id < 0 || id >= defs.count.music.num) return false;
    ded_music_t *def = &defs.music[id];

    VERBOSE( Con_Message("Starting music '%s'...", def->id) )

    return Mus_Start(def, looped);

#else
    DENG2_UNUSED2(id, looped);
    return false;
#endif
}

#undef S_StartMusic
int S_StartMusic(const char* musicID, boolean looped)
{
    LOG_AS("S_StartMusic");
    int idx = Def_GetMusicNum(musicID);
    if(idx < 0)
    {
        LOG_WARNING("Song \"%s\" not defined, cannot schedule playback.") << musicID;
        return false;
    }
    return S_StartMusicNum(idx, looped);
}

#undef S_StopMusic
void S_StopMusic(void)
{
#ifdef __CLIENT__
    Mus_Stop();
#endif
}

#undef S_PauseMusic
void S_PauseMusic(boolean paused)
{
#ifdef __CLIENT__
    Mus_Pause(paused);
#else
    DENG2_UNUSED(paused);
#endif
}

/**
 * Draws debug information on-screen.
 */
void S_Drawer(void)
{
#ifdef __CLIENT__
    if(!showSoundInfo) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_WINDOW->width(), DENG_WINDOW->height(), 0, -1, 1);

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

    coord_t fixedPos[3];
    boolean useFixedPos = false;
    float volume = 1;
    int p, id = 0;

    if(argc < 2)
    {
        Con_Printf("Usage: %s (id) (volume) at (x) (y) (z)\n", argv[0]);
        Con_Printf("(volume) must be in 0..1, but may be omitted.\n");
        Con_Printf("'at (x) (y) (z)' may also be omitted.\n");
        Con_Printf("The sound is always played locally.\n");
        return true;
    }

    // The sound ID is always first.
    id = Def_GetSoundNum(argv[1]);

    // The second argument may be a volume.
    if(argc >= 3 && stricmp(argv[2], "at"))
    {
        volume = strtod(argv[2], NULL);
        p = 3;
    }
    else
    {
        p = 2;
    }

    if(argc >= p + 4 && !stricmp(argv[p], "at"))
    {
        useFixedPos = true;
        fixedPos[VX] = strtod(argv[p + 1], NULL);
        fixedPos[VY] = strtod(argv[p + 2], NULL);
        fixedPos[VZ] = strtod(argv[p + 3], NULL);
    }

    // Check that the volume is valid.
    if(volume <= 0)
        return true;
    if(volume > 1)
        volume = 1;

    if(useFixedPos)
        S_LocalSoundAtVolumeFrom(id, NULL, fixedPos, volume);
    else
        S_LocalSoundAtVolume(id, NULL, volume);

    return true;
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
