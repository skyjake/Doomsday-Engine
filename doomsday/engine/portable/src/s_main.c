/**\file s_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"
#include "de_defs.h"

#include "sys_reslocator.h"
#include "sys_audio.h"

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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean noRndPitch;

// CODE --------------------------------------------------------------------

static void S_ReverbVolumeChanged(void)
{
    Sfx_UpdateReverb();
}

void S_Register(void)
{
    // Cvars
    C_VAR_INT("sound-volume", &sfxVolume, 0, 0, 255);
    C_VAR_INT("sound-info", &showSoundInfo, 0, 0, 1);
    C_VAR_INT("sound-rate", &sfxSampleRate, 0, 11025, 44100);
    C_VAR_INT("sound-16bit", &sfx16Bit, 0, 0, 1);
    C_VAR_INT("sound-3d", &sfx3D, 0, 0, 1);
    C_VAR_BYTE("sound-overlap-stop", &sfxOneSoundPerEmitter, 0, 0, 1);
    C_VAR_FLOAT2("sound-reverb-volume", &sfxReverbStrength, 0, 0, 10, S_ReverbVolumeChanged);

    // Ccmds
    C_CMD_FLAGS("playsound", NULL, PlaySound, CMDF_NO_DEDICATED);

    Mus_Register();
}

/**
 * Main sound system initialization. Inits both the Sfx and Mus modules.
 *
 * @return  @c true, if there were no errors.
 */
boolean S_Init(void)
{
    boolean ok = false, sfxOK, musOK;

    if(ArgExists("-nosound"))
        return true;

    // Try to load the audio driver plugin(s).
    if(!AudioDriver_Init())
    {
        Con_Message("Music and Sound Effects disabled.\n");
        return false;
    }

    // Disable random pitch changes?
    noRndPitch = ArgExists("-noRndPitch");

    sfxOK = Sfx_Init();
    musOK = Mus_Init();

    if(!sfxOK || !musOK)
    {
        Con_Message("Errors during audio subsystem initialization.\n");
        return false;
    }
    return true;
}

/**
 * Shutdown the whole sound system (Sfx + Mus).
 */
void S_Shutdown(void)
{
    Sfx_Shutdown();
    Mus_Shutdown();

    // Finally, close the audio driver.
    AudioDriver_Shutdown();
}

/**
 * Must be called before the map is changed.
 */
void S_MapChange(void)
{
    // Stop everything in the LSM.
    Sfx_InitLogical();

    Sfx_MapChange();
}

void S_SetupForChangedMap(void)
{
    // Update who is listening now.
    Sfx_SetListener(S_GetListenerMobj());
}

/**
 * Stop all channels and music, delete the entire sample cache.
 */
void S_Reset(void)
{
    Sfx_Reset();
    S_StopMusic();
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

    if(musVolume != oldMusVolume)
    {
        oldMusVolume = musVolume;
        Mus_SetVolume(musVolume / 255.0f);
    }

    // Update all channels (freq, 2D:pan,volume, 3D:position,velocity).

    Sfx_StartFrame();
    Mus_StartFrame();

    // Remove stopped sounds from the LSM.
    Sfx_PurgeLogical();

END_PROF( PROF_SOUND_STARTFRAME );
}

void S_EndFrame(void)
{
    Sfx_EndFrame();
}

/**
 * Usually the display player.
 */
mobj_t* S_GetListenerMobj(void)
{
    return ddPlayers[displayPlayer].shared.mo;
}

/**
 * \note freq and volume may be NULL(they will be modified by sound links).
 *
 * @param freq          May be @c NULL.
 * @param volume        May be @c NULL.
 */
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
 * \note: Flags can be included in the sound ID number (DDSF_*).
 * Origin and fixedpos can be both NULL, in which case the sound is played
 * in 2D and centered.
 *
 * @param origin        May be @c NULL.
 * @param fixedPos      May be @c NULL.
 *
 * @return              Non-zero if a sound was started.
 */
int S_LocalSoundAtVolumeFrom(int soundIdAndFlags, mobj_t* origin,
                             coord_t* point, float volume)
{
    int                 soundId = (soundIdAndFlags & ~DDSF_FLAG_MASK);
    sfxsample_t*        sample;
    sfxinfo_t*          info;
    float               freq = 1;
    int                 result;
    boolean             isRepeating = false;

    // A dedicated server never starts any local sounds (only logical sounds in the LSM).
    if(isDedicated || Con_IsBusy())
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
 * @param soundId       Id of the sound.
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
 * @param sec  Sector in which to stop sounds.
 * @param soundID  Unique identifier of the sound to be stopped. @c= 0 (do not match).
 * @param flags  @ref soundStopFlags
 */
static void stopSectorSounds(Sector* sec, int soundID, int flags)
{
    ddmobj_base_t* base;

    if(!sec || !flags) return;

    base = &sec->base;
    // Are we stopping with this sector's emitter?
    if(flags & SSF_SECTOR)
    {
        S_StopSound(soundID, (mobj_t*)base);
    }

    // Are we stopping with linked emitters?
    if(!(flags & (SSF_SECTOR_LINKED_PLANES | SSF_SECTOR_LINKED_SIDEDEFS))) return;

    // Process the rest of the emitter chain.
    while((base = (ddmobj_base_t*)base->thinker.next))
    {
        // Are we excluding one or more emitter types?
        if(!(flags & SSF_SECTOR_LINKED_PLANES) || !(flags & SSF_SECTOR_LINKED_SIDEDEFS))
        {
            switch(DMU_GetType(base))
            {
            case DMU_PLANE:   if(!(flags & SSF_SECTOR_LINKED_PLANES)) continue;
                break;
            case DMU_SIDEDEF: if(!(flags & SSF_SECTOR_LINKED_SIDEDEFS)) continue;
                break;
            default:
                DEBUG_Message(("stopSectorSounds: Invalid DMU type %s for ddmobj_base_t owner object %p.",
                               DMU_Str(DMU_GetType(base)), base));
                continue;
            }
        }

        // Stop sounds from this emitter.
        S_StopSound(soundID, (mobj_t*)base);
    }
}

void S_StopSound(int soundID, mobj_t* emitter)
{
    // No special stop behavior.
    // Sfx provides a routine for this.
    Sfx_StopSound(soundID, emitter);

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
        Sector* sector = 0;

        if(emitter->thinker.id)
        {
            // Emitter is a real Mobj.
            sector = emitter->bspLeaf->sector;
        }
        else
        {
            switch(DMU_GetType(emitter))
            {
            case DMU_SECTOR:
                // Emitter is a sector.
                sector = (Sector*)emitter;
                break;

            case DMU_SURFACE: {
                // Emitter is a map surface.
                Surface* suf = (Surface*)emitter;
                assert(suf->owner);
                switch(DMU_GetType(suf->owner))
                {
                case DMU_PLANE:   sector = ((Plane*)suf->owner)->sector;   break;
                case DMU_SIDEDEF: sector = ((SideDef*)suf->owner)->sector; break;
                default:
                    DEBUG_Message(("S_StopSound2: Invalid DMU type %s for Surface owner object %p, ignoring.",
                                   DMU_Str(DMU_GetType(suf->owner)), suf->owner));
                }
                break; }

            default:
                DEBUG_Message(("S_StopSound2: Invalid DMU type %s for emitter object %p, ignoring.",
                               DMU_Str(DMU_GetType(emitter)), emitter));
            }
        }

        if(sector)
        {
            stopSectorSounds(sector, soundID, flags);
        }
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
    ded_music_t*        def;

    if(id < 0 || id >= defs.count.music.num)
        return false;

    def = &defs.music[id];

    // Don't play music if the volume is at zero.
    if(isDedicated)
        return true;

    VERBOSE( Con_Message("Starting music '%s'...\n", def->id) )

    return Mus_Start(def, looped);
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
    Mus_Stop();
}

/**
 * Change paused state of the current music.
 */
void S_PauseMusic(boolean paused)
{
    Mus_Pause(paused);
}

/**
 * Draws debug information on-screen.
 */
void S_Drawer(void)
{
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
