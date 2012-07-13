/**
 * @file s_mus.c
 * Music subsystem. @ingroup audio
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

//#if WIN32
//# include <math.h> // for sqrt() ?
//#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_audio.h"
#include "de_misc.h"

#include "sys_audio.h"
#include "sys_reslocator.h"
#include "m_mus2midi.h"

typedef struct interface_info_s {
    audiointerface_music_generic_t* ip;
    const char* name;
} interface_info_t;

static interface_info_t interfaces[] = {
    { 0, "Music"},
    { 0, "CD"}
};

#define NUM_INTERFACES (sizeof(interfaces)/sizeof(interfaces[0]))

D_CMD(PlayMusic);
D_CMD(PauseMusic);
D_CMD(StopMusic);

static void Mus_UpdateSoundFont(void);

static int     musPreference = MUSP_EXT;
static char*   soundFontPath = "";

static boolean musAvail = false;
static boolean musicPaused = false;
static int     currentSong = -1;

void Mus_Register(void)
{
    // Cvars
    C_VAR_INT("music-volume", &musVolume, 0, 0, 255);
    C_VAR_INT("music-source", &musPreference, 0, 0, 2);
    C_VAR_CHARPTR2("music-soundfont", &soundFontPath, 0, 0, 0, Mus_UpdateSoundFont);

    // Ccmds
    C_CMD_FLAGS("playmusic", NULL, PlayMusic, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("pausemusic", NULL, PauseMusic, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("stopmusic", "", StopMusic, CMDF_NO_DEDICATED);
}

/**
 * Initialize the Mus module and choose the interfaces to use.
 *
 * @return  @c true, if no errors occur.
 */
boolean Mus_Init(void)
{
    unsigned int i;

    if(musAvail)
        return true; // Already initialized.

    if(isDedicated || CommandLine_Exists("-nomusic"))
    {
        Con_Message("Music disabled.\n");
        return true;
    }

    VERBOSE( Con_Message("Initializing Music subsystem...\n") );

    interfaces[0].ip = (audiointerface_music_generic_t*) AudioDriver_Music();
    interfaces[1].ip = (audiointerface_music_generic_t*) AudioDriver_CD();
    currentSong = -1;

    for(i = 0; i < NUM_INTERFACES; ++i)
    {
        if(interfaces[i].ip && !interfaces[i].ip->Init())
        {
            Con_Message("Warning:Mus_Init: Failed to initialize %s interface.\n",
                        interfaces[i].name);
            interfaces[i].ip = NULL;
        }
    }

    // Print a list of the available interfaces.
    if(verbose)
    {
        char buf[80];
        Con_Message("Music configuration:\n");
        for(i = 0; i < NUM_INTERFACES; ++i)
        {
            if(!interfaces[i].ip)
                strcpy(buf, "N/A");
            else if(!interfaces[i].ip->Get(MUSIP_ID, buf))
                strcpy(buf, "?");
            Con_Message("  %-5s: %s\n", interfaces[i].name, buf);
        }
    }

    if(!AudioDriver_Music() && !AudioDriver_CD())
    {
        // No interface for Music playback.
        return false;
    }

    // Tell the audio driver about our soundfont config.
    AudioDriver_Music_SetSoundFont(soundFontPath);

    musAvail = true;
    return true;
}

void Mus_Shutdown(void)
{
    int i;

    if(!musAvail)
        return;

    musAvail = false;

    // Shutdown interfaces.
    for(i = 0; i < NUM_INTERFACES; ++i)
    {
        if(interfaces[i].ip && interfaces[i].ip->Shutdown)
        {
            interfaces[i].ip->Shutdown();
        }
        interfaces[i].ip = 0;
    }
}

/**
 * Called on each frame by S_StartFrame.
 */
void Mus_StartFrame(void)
{
    unsigned int        i;

    if(!musAvail)
        return;

    // Update all interfaces.
    for(i = 0; i < NUM_INTERFACES; ++i)
    {
        if(interfaces[i].ip)
            interfaces[i].ip->Update();
    }
}

/**
 * Set the general music volume. Affects all music played by all interfaces.
 */
void Mus_SetVolume(float vol)
{
    unsigned int        i;

    if(!musAvail)
        return;

    // Set volume of all available interfaces.
    for(i = 0; i < NUM_INTERFACES; ++i)
    {
        if(interfaces[i].ip)
            interfaces[i].ip->Set(MUSIP_VOLUME, vol);
    }
}

/**
 * Pauses or resumes the music.
 */
void Mus_Pause(boolean doPause)
{
    unsigned int        i;

    if(!musAvail)
        return;

    // Pause all interfaces.
    for(i = 0; i < NUM_INTERFACES; ++i)
    {
        if(interfaces[i].ip)
            interfaces[i].ip->Pause(doPause);
    }
}

void Mus_Stop(void)
{
    unsigned int        i;

    if(!musAvail)
        return;

    currentSong = -1;

    // Stop all interfaces.
    for(i = 0; i < NUM_INTERFACES; ++i)
    {
        if(interfaces[i].ip)
            interfaces[i].ip->Stop();
    }
}

/**
 * @return: @c true, if the specified lump contains a MUS song.
 */
boolean Mus_IsMUSLump(lumpnum_t lumpNum)
{
    char buf[4];
    int lumpIdx;
    abstractfile_t* fsObject = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    if(!fsObject) return false;

    F_ReadLumpSection(fsObject, lumpIdx, (uint8_t*)buf, 0, 4);
    // ASCII "MUS" and CTRL-Z (hex 4d 55 53 1a)
    return !strncmp(buf, "MUS\x01a", 4);
}

/**
 * Check for the existence of an "external" music file.
 * Songs can be either in external files or non-MUS lumps.
 *
 * @return  Non-zero if an external file of that name exists.
 */
int Mus_GetExt(ded_music_t* def, ddstring_t* retPath)
{
    if(!musAvail || !AudioDriver_Music()) return false;

    // All external music files are specified relative to the base path.
    if(def->path && !Str_IsEmpty(Uri_Path(def->path)))
    {
        ddstring_t fullPath, *path;

        Str_Init(&fullPath);
        F_PrependBasePath(&fullPath, Uri_Path(def->path));
        if(F_Access(Str_Text(&fullPath)))
        {
            if(retPath)
                Str_Set(retPath, Str_Text(&fullPath));
            return true;
        }

        path = Uri_ToString(def->path);
        Con_Message("Warning \"%s\" for id '%s' not found.\n", Str_Text(path), def->id);
        Str_Delete(path);
    }

    // Try the resource locator.
    return F_FindResource2(RC_MUSIC, def->lumpName, retPath);
}

/**
 * @return  The track number if successful else zero.
 */
int Mus_GetCD(ded_music_t* def)
{
    if(!musAvail || !AudioDriver_CD() || !def)
        return 0;

    if(def->cdTrack)
        return def->cdTrack;

    if(def->path && !stricmp(Str_Text(Uri_Scheme(def->path)), "cd"))
        return atoi(Str_Text(Uri_Path(def->path)));

    return 0;
}

/// @return  Composed music file name.


/**
 * @return 1, if music was started. 0, if attempted to start but failed.
 *         -1, if it was MUS data and @a canPlayMUS says we can't play it.
 */
int Mus_StartLump(lumpnum_t lump, boolean looped, boolean canPlayMUS)
{
    if(!AudioDriver_Music() || lump < 0) return 0;

    if(Mus_IsMUSLump(lump))
    {    
        // Lump is in DOOM's MUS format. We must first convert it to MIDI.
        AutoStr* srcFile = 0;
        abstractfile_t* fsObject;
        size_t lumpLength;
        int lumpIdx;
        uint8_t* buf;

        if(!canPlayMUS)
            return -1;

        srcFile = AudioDriver_Music_ComposeTempBufferFilename(".mid");

        // Read the lump, convert to MIDI and output to a temp file in the
        // working directory. Use a filename with the .mid extension so that
        // any player which relies on the it for format recognition works as
        // expected.

        lumpLength = F_LumpLength(lump);
        buf = (uint8_t*) malloc(lumpLength);
        if(!buf)
        {
            Con_Message("Warning:Mus_Start: Failed on allocation of %lu bytes for "
                        "temporary MUS to MIDI conversion buffer.\n", (unsigned long) lumpLength);
            return 0;
        }

        fsObject = F_FindFileForLumpNum2(lump, &lumpIdx);
        F_ReadLumpSection(fsObject, lumpIdx, buf, 0, lumpLength);
        M_Mus2Midi((void*)buf, lumpLength, Str_Text(srcFile));
        free(buf);

        return AudioDriver_Music_PlayNativeFile(Str_Text(srcFile), looped);
    }
    else
    {
        return AudioDriver_Music_PlayLump(lump, looped);
    }
}

/**
 * Start playing a song. The chosen interface depends on what's available
 * and what kind of resources have been associated with the song.
 * Any previously playing song is stopped.
 *
 * @return              Non-zero if the song is successfully played.
 */
int Mus_Start(ded_music_t* def, boolean looped)
{
    int i, order[3], songID;
    ddstring_t path;

    if(!musAvail) return false;

    songID = def - defs.music;

    DEBUG_Message(("Mus_Start: Starting ID:%i looped:%i, currentSong ID:%i\n", songID, looped, currentSong));

    // We will not restart the currently playing song.
    if(songID == currentSong &&
       (AudioDriver_Music_IsPlaying() ||
        (AudioDriver_CD() && AudioDriver_CD()->gen.Get(MUSIP_PLAYING, NULL))))
        return false;

    // Stop the currently playing song.
    Mus_Stop();

    AudioDriver_Music_SwitchBufferFilenames();

    // This is the song we're playing now.
    currentSong = songID;

    // Choose the order in which to try to start the song.
    order[0] = musPreference;

    switch(musPreference)
    {
    case MUSP_CD:
        order[1] = MUSP_EXT;
        order[2] = MUSP_MUS;
        break;

    case MUSP_EXT:
        order[1] = MUSP_MUS;
        order[2] = MUSP_CD;
        break;

    default: // MUSP_MUS
        order[1] = MUSP_EXT;
        order[2] = MUSP_CD;
        break;
    }

    // Try to start the song.
    for(i = 0; i < 3; ++i)
    {
        boolean canPlayMUS = true;

        switch(order[i])
        {
        case MUSP_CD:
            if(Mus_GetCD(def))
            {
                return AudioDriver_CD()->Play(Mus_GetCD(def), looped);
            }
            break;

        case MUSP_EXT:
            Str_Init(&path);
            if(Mus_GetExt(def, &path))
            {
                VERBOSE( Con_Message("Attempting to play song '%s' (file \"%s\").\n",
                                     def->id, F_PrettyPath(Str_Text(&path))) )

                // Its an external file.
                return AudioDriver_Music_PlayFile(Str_Text(&path), looped);
            }

            // Next, try non-MUS lumps.
            canPlayMUS = false;

            // Note: Intentionally falls through to MUSP_MUS.

        case MUSP_MUS:
            if(AudioDriver_Music())
            {
                lumpnum_t lump;
                if(def->lumpName && (lump = F_CheckLumpNumForName2(def->lumpName, true)) >= 0)
                {
                    int result = Mus_StartLump(lump, looped, canPlayMUS);
                    if(result < 0) break;
                    return result;
                }
            }
            break;

        default:
            Con_Error("Mus_Start: Invalid value order[i] = %i.", order[i]);
            break;
        }
    }

    // No song was started.
    return false;
}

static void Mus_UpdateSoundFont(void)
{
    AudioDriver_Music_SetSoundFont(Con_GetString("music-soundfont"));
}

/**
 * CCmd: Play a music track.
 */
D_CMD(PlayMusic)
{
    if(!musAvail)
    {
        Con_Printf("The Music module is not available.\n");
        return false;
    }

    switch(argc)
    {
    default:
        Con_Printf("Usage:\n  %s (music-def)\n", argv[0]);
        Con_Printf("  %s lump (lumpname)\n", argv[0]);
        Con_Printf("  %s file (filename)\n", argv[0]);
        Con_Printf("  %s cd (track)\n", argv[0]);
        break;

    case 2: {
        int musIdx = Def_GetMusicNum(argv[1]);
        if(musIdx < 0)
        {
            Con_Printf("Music '%s' not defined.\n", argv[1]);
            return false;
        }

        Mus_Start(&defs.music[musIdx], true);
        break;
      }
    case 3:
        if(!stricmp(argv[1], "lump"))
        {
            lumpnum_t lump = F_CheckLumpNumForName2(argv[2], true);
            if(lump < 0) return false; // No such lump.

            Mus_Stop();
            return AudioDriver_Music_PlayLump(lump, true);
        }
        else if(!stricmp(argv[1], "file"))
        {
            Mus_Stop();
            return AudioDriver_Music_PlayFile(argv[2], true);
        }
        else
        {   // Perhaps a CD track?
            if(!stricmp(argv[1], "cd"))
            {
                if(!AudioDriver_CD())
                {
                    Con_Printf("No CDAudio interface available.\n");
                    return false;
                }

                Mus_Stop();
                return AudioDriver_CD()->Play(atoi(argv[2]), true);
            }
        }
        break;
    }

    return true;
}

D_CMD(StopMusic)
{
    Mus_Stop();
    return true;
}

D_CMD(PauseMusic)
{
    musicPaused = !musicPaused;
    Mus_Pause(musicPaused);
    return true;
}

