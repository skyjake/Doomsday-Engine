/**\file s_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

static void S_ReverbVolumeChanged(void)
{
#ifdef __CLIENT__
    Sfx_UpdateReverb();
#endif
}

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

/**
 * Main sound system initialization. Inits both the Sfx and Mus modules.
 *
 * @return  @c true, if there were no errors.
 */
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
        Con_Message("Music and Sound Effects disabled.\n");
        return false;
    }

    sfxOK = Sfx_Init();
    musOK = Mus_Init();

    if(!sfxOK || !musOK)
    {
        Con_Message("Errors during audio subsystem initialization.\n");
        return false;
    }
#endif

    return true;
}

/**
 * Shutdown the whole sound system (Sfx + Mus).
 */
void S_Shutdown(void)
{
#ifdef __CLIENT__
    Sfx_Shutdown();
    Mus_Shutdown();

    // Finally, close the audio driver.
    AudioDriver_Shutdown();
#endif
}

/**
 * Must be called before the map is changed.
 */
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

/**
 * Stop all channels and music, delete the entire sample cache.
 */
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
    static int          oldMusVolume = -1;
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
        info - sounds, i++);

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

/**
 * Play a sound on the local system. A public interface.
 *
 * If @a origin and @a point are both @c NULL, the sound is played in 2D and
 * centered.
 *
 * @param soundIdAndFlags ID of the sound to play. Flags can be included (DDSF_*).
 * @param origin        Mobj where the sound originates. May be @c NULL.
 * @param point         World coordinates where the sound originate. May be @c NULL.
 * @param volume        Volume for the sound (0...1).
 *
 * @return              Non-zero if a sound was started.
 */
int S_LocalSoundAtVolumeFrom(int soundIdAndFlags, mobj_t* origin,
                             coord_t* point, float volume)
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
        Con_Message("S_LocalSoundAtVolumeFrom: Warning! "
                    "Too high volume (%f).\n", volume);
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
                    ("S_LocalSoundAtVolumeFrom: Sound %i " "caching failed.\n",
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
    return false;
#endif
}

/**
 * Plays a sound on the local system at the given volume.
 * This is a public sound interface.
 *
 * @return              Non-zero if a sound was started.
 */
int S_LocalSoundAtVolume(int soundID, mobj_t* origin, float volume)
{
    return S_LocalSoundAtVolumeFrom(soundID, origin, NULL, volume);
}

/**
 * Plays a sound on the local system from the given origin.
 * This is a public sound interface.
 *
 * @return              Non-zero if a sound was started.
 */
int S_LocalSound(int soundID, mobj_t* origin)
{
    // Play local sound at max volume.
    return S_LocalSoundAtVolumeFrom(soundID, origin, NULL, 1);
}

/**
 * Plays a sound on the local system at a give distance from listener.
 * This is a public sound interface.
 *
 * @return              Non-zero if a sound was started.
 */
int S_LocalSoundFrom(int soundID, coord_t* fixedPos)
{
    return S_LocalSoundAtVolumeFrom(soundID, NULL, fixedPos, 1);
}

/**
 * Play a world sound. All players in the game will hear it.
 *
 * @return              Non-zero if a sound was started.
 */
int S_StartSound(int soundID, mobj_t* origin)
{
    // The sound is audible to everybody.
    Sv_Sound(soundID, origin, SVSF_TO_ALL);
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    return S_LocalSound(soundID, origin);
}

/**
 * Play a world sound. The sound is sent to all players except the one who
 * owns the origin mobj. The server assumes that the owner of the origin plays
 * the sound locally, which is done here, in the end of S_StartSoundEx().
 *
 * @param soundID       Id of the sound.
 * @param origin        Origin mobj for the sound.
 *
 * @return              Non-zero if a sound was successfully started.
 */
int S_StartSoundEx(int soundID, mobj_t* origin)
{
    Sv_Sound(soundID, origin, SVSF_TO_ALL | SVSF_EXCLUDE_ORIGIN);
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    return S_LocalSound(soundID, origin);
}

/**
 * Play a world sound. All players in the game will hear it.
 *
 * @return              Non-zero if a sound was started.
 */
int S_StartSoundAtVolume(int soundID, mobj_t* origin, float volume)
{
    Sv_SoundAtVolume(soundID, origin, volume, SVSF_TO_ALL);
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    // The sound is audible to everybody.
    return S_LocalSoundAtVolume(soundID, origin, volume);
}

/**
 * Play a player sound. Only the specified player will hear it.
 *
 * @return              Non-zero if a sound was started (always).
 */
int S_ConsoleSound(int soundID, mobj_t* origin, int targetConsole)
{
    Sv_Sound(soundID, origin, targetConsole);

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
static void stopSectorSounds(ddmobj_base_t* sectorEmitter, int soundID, int flags)
{
    ddmobj_base_t* base;

    if(!sectorEmitter || !flags) return;

    // Are we stopping with this sector's emitter?
    if(flags & SSF_SECTOR)
    {
        _api_S.StopSound(soundID, (mobj_t*)sectorEmitter);
    }

    // Are we stopping with linked emitters?
    if(!(flags & SSF_SECTOR_LINKED_SURFACES)) return;

    // Process the rest of the emitter chain.
    base = (ddmobj_base_t*)sectorEmitter;
    while((base = (ddmobj_base_t*)base->thinker.next))
    {
        // Stop sounds from this emitter.
        _api_S.StopSound(soundID, (mobj_t*)base);
    }
}

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
        // In netgames, the server is responsible for telling clients
        // when to stop sounds. The LSM will tell us if a sound was
        // stopped somewhere in the world.
        Sv_StopSound(soundID, emitter);
    }
}

void S_StopSound2(int soundID, mobj_t* emitter, int flags)
{
    // Are we performing any special stop behaviors?
    if(emitter && flags)
    {
        if(emitter->thinker.id)
        {
            // Emitter is a real Mobj.
            sector_s* sector = emitter->bspLeaf->sector;
            stopSectorSounds((ddmobj_base_t*)&sector->base, soundID, flags);
            return;
        }

        // The head of the chain is the sector. Find it.
        while(emitter->thinker.prev)
        {
            emitter = (mobj_t*)emitter->thinker.prev;
        }
        stopSectorSounds((ddmobj_base_t*)emitter, soundID, flags);
        return;
    }

    // A regular stop.
    S_StopSound(soundID, emitter);
}

/**
 * Is an instance of the sound being played using the given emitter?
 * If sound_id is zero, returns true if the source is emitting any sounds.
 * An exported function.
 *
 * @return              Non-zero if a sound is playing.
 */
int S_IsPlaying(int soundID, mobj_t* emitter)
{
    // The Logical Sound Manager (under Sfx) provides a routine for this.
    return Sfx_IsPlaying(soundID, emitter);
}

/**
 * Start a song based on its number.
 *
 * @return              @c NULL, if the ID exists.
 */
int S_StartMusicNum(int id, boolean looped)
{
#ifdef __CLIENT__

    ded_music_t*        def;

    if(id < 0 || id >= defs.count.music.num)
        return false;

    def = &defs.music[id];

    // Don't play music if the volume is at zero.
    if(isDedicated)
        return true;

    VERBOSE( Con_Message("Starting music '%s'...\n", def->id) )

    return Mus_Start(def, looped);

#else
    return false;
#endif
}

/**
 * @return  @c NULL, if the song is found.
 */
int S_StartMusic(const char* musicID, boolean looped)
{
    int idx = Def_GetMusicNum(musicID);
    if(idx < 0)
    {
        Con_Message("Warning:S_StartMusic: Song \"%s\" not defined.\n", musicID);
        return false;
    }
    return S_StartMusicNum(idx, looped);
}

/**
 * Stops playing a song.
 */
void S_StopMusic(void)
{
#ifdef __CLIENT__
    Mus_Stop();
#endif
}

/**
 * Change paused state of the current music.
 */
void S_PauseMusic(boolean paused)
{
#ifdef __CLIENT__
    Mus_Pause(paused);
#endif
}

/**
 * Draws debug information on-screen.
 */
void S_Drawer(void)
{
#ifdef __CLIENT__
    if(!showSoundInfo) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);

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
    S_StopSound2,
    S_StopSound,
    S_IsPlaying,
    S_StartMusic,
    S_StartMusicNum,
    S_StopMusic,
    S_PauseMusic
};
