/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * s_mus.c: Music Subsystem.
 */

// HEADER FILES ------------------------------------------------------------

#if WIN32
# include <math.h> // for sqrt()
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_audio.h"
#include "de_misc.h"

#include "sys_audio.h"
#include "sys_reslocator.h"
#include "m_mus2midi.h"

// MACROS ------------------------------------------------------------------

#define NUM_INTERFACES (sizeof(interfaces)/sizeof(interfaces[0]))

// TYPES -------------------------------------------------------------------

typedef struct interface_info_s {
    audiointerface_music_generic_t** ip;
    const char*         name;
} interface_info_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(PlayMusic);
D_CMD(PauseMusic);
D_CMD(StopMusic);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// Music playback interfaces loaded from a sound driver plugin.
extern audiointerface_music_t audiodExternalIMusic;
extern audiointerface_cd_t audiodExternalICD;

#ifdef MACOSX
extern audiointerface_music_t audiodQuickTimeMusic;
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int musPreference = MUSP_EXT;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean musAvail = false;

static int currentSong = -1;
static boolean musicPaused = false;

// The interfaces.
static audiointerface_music_t* iMusic;
static audiointerface_cd_t* iCD;

// The interface list. Used to access the common features of all the
// interfaces conveniently.
static interface_info_t interfaces[] = {
    {(audiointerface_music_generic_t**) &iMusic, "Music"},
    {(audiointerface_music_generic_t**) &iCD, "CD"}
};

// CODE --------------------------------------------------------------------

void Mus_Register(void)
{
    // Cvars
    C_VAR_INT("music-volume", &musVolume, 0, 0, 255);
    C_VAR_INT("music-source", &musPreference, 0, 0, 2);

    // Ccmds
    C_CMD_FLAGS("playmusic", NULL, PlayMusic, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("pausemusic", NULL, PauseMusic, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("stopmusic", "", StopMusic, CMDF_NO_DEDICATED);
}

/**
 * Initialize the Mus module and choose the interfaces to use.
 *
 * @return              @c true, if no errors occur.
 */
boolean Mus_Init(void)
{
    unsigned int        i;

    if(musAvail)
        return true; // Already initialized.

    if(isDedicated || ArgExists("-nomusic"))
        return true;

    // Use the external music playback facilities, if available.
    if(audioDriver == &audiod_dummy)
    {
        iMusic = NULL;
        iCD  = NULL;
    }
    else if(audioDriver == &audiod_sdlmixer)
    {
        iMusic = (audiointerface_music_t*) &audiod_sdlmixer_music;
        iCD  = NULL;
    }
    else
    {
        iMusic = (audiodExternalIMusic.gen.Init ? &audiodExternalIMusic : 0);
        iCD  = (audiodExternalICD.gen.Init  ? &audiodExternalICD  : 0);
    }

#ifdef MACOSX
    // On the Mac, just use QuickTime for the music and be done with it.
    iMusic = &audiodQuickTimeMusic;
#endif

    // Initialize the chosen interfaces.
    for(i = 0; i < NUM_INTERFACES; ++i)
    {
        if(*interfaces[i].ip && !(*interfaces[i].ip)->Init())
        {
            Con_Message("Mus_Init: Failed to initialize %s interface.\n",
                        interfaces[i].name);

            *interfaces[i].ip = NULL;
        }
    }

    // Print a list of the chosen interfaces.
    if(verbose)
    {
        char                buf[40];

        Con_Printf("Mus_Init: Interfaces:");
        for(i = 0; i < NUM_INTERFACES; ++i)
        {
            if(*interfaces[i].ip)
            {
                if(!(*interfaces[i].ip)->Get(MUSIP_ID, buf))
                    strcpy(buf, "?");

                Con_Printf(" %s", buf);
            }
        }

        Con_Printf("\n");
    }

    currentSong = -1;
    musAvail = true;

    return true;
}

void Mus_Shutdown(void)
{
    if(!musAvail)
        return;

    musAvail = false;

    // No more interfaces.
    iMusic = 0;
    iCD = 0;
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
        if(*interfaces[i].ip)
            (*interfaces[i].ip)->Update();
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
        if(*interfaces[i].ip)
            (*interfaces[i].ip)->Set(MUSIP_VOLUME, vol);
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
        if(*interfaces[i].ip)
            (*interfaces[i].ip)->Pause(doPause);
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
        if(*interfaces[i].ip)
            (*interfaces[i].ip)->Stop();
    }
}

/**
 * @return:             @c true, if the specified lump contains a MUS song.
 */
boolean Mus_IsMUSLump(int lump)
{
    char                buf[4];

    W_ReadLumpSection(lump, buf, 0, 4);

    // ASCII "MUS" and CTRL-Z (hex 4d 55 53 1a)
    return !strncmp(buf, "MUS\x01a", 4);
}

/**
 * Check for the existence of an "external" music file.
 * Songs can be either in external files or non-MUS lumps.
 *
 * @return              Non-zero if an external file of that name exists.
 */
int Mus_GetExt(ded_music_t* def, filename_t retPath)
{
    filename_t path;

    if(!musAvail || !iMusic)
        return false;

    // All external music files are specified relative to the base path.
    if(def->path.path[0])
    {
        M_PrependBasePath(path, def->path.path, DED_PATH_LEN);
        if(F_Access(path))
        {
            if(retPath)
                strncpy(retPath, path, FILENAME_T_MAXLEN);
            return true;
        }
        Con_Message("Mus_GetExt: Song %s: %s not found.\n", def->id, def->path.path);
    }

    // Try the resource locator.
    if(F_FindResource(RT_MUSIC, path, def->lumpName, 0, FILENAME_T_MAXLEN))
    {
        if(retPath)
            strncpy(retPath, path, FILENAME_T_MAXLEN);
        return true;
    }
    return false;
}

/**
 * @return:             The track number if successful else zero.
 */
int Mus_GetCD(ded_music_t* def)
{
    if(!musAvail || !iCD)
        return 0;

    if(def->cdTrack)
        return def->cdTrack;

    if(!strnicmp(def->path.path, "cd:", 3))
        return atoi(def->path.path + 3);

    return 0;
}

static void composeBufferedMusicFilename(char* path, size_t len, int id, const char* ext)
{
#define BUFFERED_MUSIC_FILE      "dd-buffered-song"

    if(ext && ext[0])
    {
        dd_snprintf(path, len, "%s%i%s", BUFFERED_MUSIC_FILE, id, ext);
        return;
    }

    dd_snprintf(path, len, "%s%i", BUFFERED_MUSIC_FILE, id);

#undef BUFFERED_MUSIC_FILE
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
    static int          currentBufFile = 0;

    filename_t          path;
    int                 i, order[3], songID;

    if(!musAvail)
        return false;

    songID = def - defs.music;

    // We will not restart the currently playing song.
    if(songID == currentSong &&
       ((iMusic && iMusic->gen.Get(MUSIP_PLAYING, NULL)) ||
        (iCD && iCD->gen.Get(MUSIP_PLAYING, NULL))))
        return false;

    // Stop the currently playing song.
    Mus_Stop();

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
        boolean             canPlayMUS = true;

        switch(order[i])
        {
        case MUSP_CD:
            if(Mus_GetCD(def))
                return iCD->Play(Mus_GetCD(def), looped);
            break;

        case MUSP_EXT:
            if(Mus_GetExt(def, path))
            {   // Its an external file.
                // The song may be in a virtual file, so we must buffer
                // it ourselves.
                DFILE* file = F_Open(path, "rb");
                size_t len = F_Length(file);

                if(!iMusic->Play)
                {   // Music interface does not offer buffer playback.
                    // Write to disk and play from there.
                    filename_t fname;

                    composeBufferedMusicFilename(fname, FILENAME_T_MAXLEN, currentBufFile ^= 1, NULL);

                    { FILE* outFile;
                    if((outFile = fopen(fname, "wb")))
                    {
                        void* buf = M_Malloc(len);

                        F_Read(buf, len, file);
                        fwrite(buf, 1, len, outFile);
                        fclose(outFile);
                        F_Close(file);

                        M_Free(buf);

                        return iMusic->PlayFile(fname, looped);
                    }}

                    Con_Message("Mus_Start: Couldn't open %s for writing. %s\n", fname, strerror(errno));
                    F_Close(file);
                    return false;
                }
                else
                {   // Music interface offers buffered playback. Use it.
                    void*               ptr;

                    VERBOSE(Con_Message("Mus_GetExt: Opened Song %s "
                                        "(File \"%s\" %u bytes)\n",
                                        def->id, M_PrettyPath(path),
                                        (unsigned int) len));

                    ptr = iMusic->SongBuffer(len);
                    F_Read(ptr, len, file);
                    F_Close(file);

                    return iMusic->Play(looped);
                }
            }

            // Next, try non-MUS lumps.
            canPlayMUS = false;
            // Fall through.

        case MUSP_MUS:
            if(iMusic)
            {
                lumpnum_t lump;
                if(def->lumpName && (lump = W_CheckNumForName(def->lumpName)) != -1)
                {
                    const char* srcFile = 0;
                    filename_t fname;

                    if(Mus_IsMUSLump(lump))
                    {   // Lump is in DOOM's MUS format.
                        void* buf;
                        size_t len;

                        if(!canPlayMUS)
                            break;

                        composeBufferedMusicFilename(fname, FILENAME_T_MAXLEN, currentBufFile ^= 1, ".mid");
                        srcFile = fname;

                        // Read the lump, convert to MIDI and output to a temp file in the
                        // working directory. Use a filename with the .mid extension so that
                        // any player which relies on the it for format recognition works as
                        // expected.

                        len = W_LumpLength(lump);
                        buf = M_Malloc(len);
                        W_ReadLump(lump, buf);

                        M_Mus2Midi(buf, len, srcFile);
                        M_Free(buf);
                    }
                    else if(!iMusic->Play)
                    {   // Music interface does not offer buffer playback.
                        // Write this lump to disk and play from there.
                        composeBufferedMusicFilename(fname, FILENAME_T_MAXLEN, currentBufFile ^= 1, 0);
                        srcFile = fname;
                        if(!W_DumpLump(lump, srcFile))
                            return false;
                    }

                    if(srcFile)
                        return iMusic->PlayFile(srcFile, looped);

                    W_ReadLump(lump, iMusic->SongBuffer(W_LumpLength(lump)));
                    return iMusic->Play(looped);
                }
            }
            break;

        default:
            Con_Error("Mus_Start: Invalid value, order[i] = %i.", order[i]);
            break;
        }
    }

    // The song was not started.
    return false;
}

/**
 * CCmd: Play a music track.
 */
D_CMD(PlayMusic)
{
    int                 i;
    size_t              len;
    void*               ptr;
    filename_t          buf;

    if(!musAvail)
    {
        Con_Printf("The Mus module is not available.\n");
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

    case 2:
        i = Def_GetMusicNum(argv[1]);
        if(i < 0)
        {
            Con_Printf("Music '%s' not defined.\n", argv[1]);
            return false;
        }

        Mus_Start(&defs.music[i], true);
        break;

    case 3:
        if(!stricmp(argv[1], "lump"))
        {
            i = W_CheckNumForName(argv[2]);
            if(i < 0)
                return false; // No such lump.

            if(iMusic)
            {
                Mus_Stop();

                ptr = iMusic->SongBuffer(len = W_LumpLength(i));
                W_ReadLump(i, ptr);

                return iMusic->Play(true);
            }

            Con_Printf("No music interface available.\n");
            return false;
        }
        else if(!stricmp(argv[1], "file"))
        {
            M_TranslatePath(buf, argv[2], FILENAME_T_MAXLEN);
            if(iMusic)
            {
                Mus_Stop();
                return iMusic->PlayFile(buf, true);
            }

            Con_Printf("No music interface available.\n");
            return false;
        }
        else
        {   // Perhaps a CD track?
            if(!stricmp(argv[1], "cd"))
            {
                if(iCD)
                {
                    Mus_Stop();
                    return iCD->Play(atoi(argv[2]), true);
                }

                Con_Printf("No CDAudio interface available.\n");
                return false;
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
