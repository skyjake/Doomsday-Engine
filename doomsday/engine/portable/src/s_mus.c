/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
#include "r_extres.h"
#include "m_mus2midi.h"

// MACROS ------------------------------------------------------------------

#define BUFFERED_MUSIC_FILE      "dd-buffered-song"
#define BUFFERED_MIDI_MUSIC_FILE "dd-buffered-song.mid"
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
#if WIN32
    int                 val;

    if(!musAvail)
        return;

    // Under Win32 we need to do this via the mixer.
    val = MINMAX_OF(0, (byte) (vol * 255 + .5f), 255);

    // Straighten the volume curve.
    val <<= 8; // Make it a word.
    val = (int) (255.9980469 * sqrt(vol));

    Sys_Mixer4i(MIX_MIDI, MIX_SET, MIX_VOLUME, val);
    Sys_Mixer4i(MIX_CDAUDIO, MIX_SET, MIX_VOLUME, val);
#else
    unsigned int        i;

    if(!musAvail)
        return;

    // Set volume of all available interfaces.
    for(i = 0; i < NUM_INTERFACES; ++i)
    {
        if(*interfaces[i].ip)
            (*interfaces[i].ip)->Set(MUSIP_VOLUME, vol);
    }
#endif
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
    filename_t          path;

    if(!musAvail || !iMusic)
        return false;

    // All external music files are specified relative to the base path.
    if(def->path.path[0])
    {
        M_PrependBasePath(path, def->path.path, DED_PATH_LEN);
        if(F_Access(path))
        {
            // Return the real file name if not just checking.
            if(retPath)
            {
                strncpy(retPath, path, FILENAME_T_MAXLEN);
                retPath[FILENAME_T_MAXLEN] = '\0';
            }

            return true;
        }

        Con_Message("Mus_GetExt: Song %s: %s not found.\n", def->id,
                    def->path.path);
    }

    // Try the resource locator.
    if(R_FindResource(RT_MUSIC, path, def->lumpName, NULL,
                      FILENAME_T_MAXLEN))
    {
        if(retPath)
        {
            strncpy(retPath, path, FILENAME_T_MAXLEN);
            retPath[FILENAME_T_MAXLEN] = '\0';
        }

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

/**
 * Start playing a song. The chosen interface depends on what's available
 * and what kind of resources have been associated with the song.
 * Any previously playing song is stopped.
 *
 * @return              Non-zero if the song is successfully played.
 */
int Mus_Start(ded_music_t* def, boolean looped)
{
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
                DFILE*              file = F_Open(path, "rb");
                size_t              len = F_Length(file);

                if(!iMusic->Play)
                {   // Music interface does not offer buffer playback.
                    // Write to disk and play from there.
                    FILE*               outFile;
                    void*               buf = malloc(len);

                    if((outFile = fopen(BUFFERED_MUSIC_FILE, "wb")) == NULL)
                    {
                        Con_Message("Mus_Start: Couldn't open %s for writing. %s\n",
                                    BUFFERED_MUSIC_FILE, strerror(errno));
                        F_Close(file);
                        return false;
                    }

                    F_Read(buf, len, file);

                    fwrite(buf, 1, len, outFile);
                    fclose(outFile);

                    F_Close(file);

                    return iMusic->PlayFile(BUFFERED_MUSIC_FILE, looped);
                }
                else
                {   // Music interface offers buffered playback. Use it.
                    void*               ptr;

                    VERBOSE(Con_Message("Mus_GetExt: Opened Song %s "
                                        "(File \"%s\" %ul bytes)\n",
                                        def->id, M_PrettyPath(path),
                                        len));

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
                lumpnum_t           lump;

                if((lump = W_CheckNumForName(def->lumpName)) != -1)
                {
                    size_t              len;
                    void*               ptr;

                    if(Mus_IsMUSLump(lump))
                    {   // Lump is in DOOM's MUS format.
                        void*               buf;

                        if(!canPlayMUS)
                            break;

                        // Read the lump, convert to MIDI and output to a
                        // temp file in the working directory. Use a
                        // filename with the .mid extension so that the
                        // player knows the format.

                        len = W_LumpLength(lump);
                        buf = M_Malloc(len);
                        W_ReadLump(lump, buf);

                        M_Mus2Midi(buf, len, BUFFERED_MIDI_MUSIC_FILE);

                        M_Free(buf);

                        // Play the newly converted MIDI file.
                        return iMusic->PlayFile(BUFFERED_MIDI_MUSIC_FILE,
                                                looped);
                    }

                    if(!iMusic->Play)
                    {   // Music interface does not offer buffer playback.
                        // Write this lump to disk and play from there.
                        if(!W_DumpLump(lump, BUFFERED_MUSIC_FILE))
                            return false;

                        // Play the cached music file.
                        return iMusic->PlayFile(BUFFERED_MUSIC_FILE,
                                                looped);
                    }

                    ptr = iMusic->SongBuffer(len = W_LumpLength(lump));
                    W_ReadLump(lump, ptr);
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
