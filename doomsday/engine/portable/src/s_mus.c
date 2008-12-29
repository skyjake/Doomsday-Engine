/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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
D_CMD(PlayExt);
D_CMD(StopMusic);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// Music playback interfaces loaded from a sound driver plugin.
extern audiointerface_mus_t audiodExternalIMus;
extern audiointerface_ext_t audiodExternalIExt;
extern audiointerface_cd_t audiodExternalICD;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int musPreference = MUSP_EXT;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean musAvail = false;

static int currentSong = -1;

// The interfaces.
static audiointerface_mus_t* iMus;
static audiointerface_ext_t* iExt;
static audiointerface_cd_t* iCD;

// The interface list. Used to access the common features of all the
// interfaces conveniently.
static interface_info_t interfaces[] = {
    {(audiointerface_music_generic_t**) &iMus, "Mus"},
    {(audiointerface_music_generic_t**) &iExt, "Ext"},
    {(audiointerface_music_generic_t**) &iCD, "CD"}
};

// CODE --------------------------------------------------------------------

void Mus_Register(void)
{
    // Cvars
    C_VAR_INT("music-volume", &musVolume, 0, 0, 255);
    C_VAR_INT("music-source", &musPreference, 0, 0, 2);

    // Ccmds
    C_CMD_FLAGS("playext", "s", PlayExt, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("playmusic", NULL, PlayMusic, CMDF_NO_DEDICATED);
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
        iMus = NULL;
        iExt = NULL;
        iCD  = NULL;
    }
    else if(audioDriver == &audiod_sdlmixer)
    {
        iMus = (audiointerface_mus_t*) &audiod_sdlmixer_mus;
        iExt = (audiointerface_ext_t*) &audiod_sdlmixer_ext;
        iCD  = NULL;
    }
    else
    {
        iMus = (audiodExternalIMus.gen.Init ? &audiodExternalIMus : 0);
        iExt = (audiodExternalIExt.gen.Init ? &audiodExternalIExt : 0);
        iCD  = (audiodExternalICD.gen.Init  ? &audiodExternalICD  : 0);
    }

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
    iMus = 0;
    iExt = 0;
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
 * @return:             @c true, if the specified lump contains a MIDI song.
 */
boolean Mus_IsMIDILump(int lump)
{
    char                buf[4];

    W_ReadLumpSection(lump, buf, 0, 4);

    // ASCII "MThd"
    return !strncmp(buf, "MThd", 4);
}

/**
 * The lump may contain non-MUS data.
 *
 * @return              Non-zero if successful.
 */
int Mus_GetMUS(ded_music_t* def)
{
    lumpnum_t           lump;
    size_t              len;
    void*               ptr;

    if(!musAvail || !iMus)
        return false;

    lump = W_CheckNumForName(def->lumpName);
    if(lump < 0)
        return false; // No such lump.

    // Is this MUS or MIDI data or what?
    if(!(Mus_IsMUSLump(lump) || Mus_IsMIDILump(lump)))
        return false;

    ptr = iMus->SongBuffer(len = W_LumpLength(lump));
    W_ReadLump(lump, ptr);
    return true;
}

/**
 * Load a song file.
 * Songs can be either in external files or non-MUS lumps.
 *
 * @return              Non-zero if an external file of that name exists.
 */
int Mus_GetExt(ded_music_t* def, char* path)
{
    char                buf[300];
    lumpnum_t           lump;
    size_t              len;
    void*               ptr;

    if(!musAvail || !iExt)
        return false;

    if(path)
        strcpy(path, "");

    // All external music files are specified relative to the base path.
    if(def->path.path[0])
    {
        M_PrependBasePath(def->path.path, buf);
        if(F_Access(buf))
        {
            // Return the real file name if not just checking.
            if(path)
            {
                // Because the song can be in a virtual file, we must buffer
                // it ourselves.
                DFILE*              file = F_Open(buf, "rb");

                VERBOSE(Con_Message("Mus_GetExt: Opened Song %s "
                                    "(File \"%s\" %ul bytes)\n",
                                    def->id, M_PrettyPath(def->path.path),
                                    F_Length(file)));

                ptr = iExt->SongBuffer(len = F_Length(file));
                F_Read(ptr, len, file);
                F_Close(file);

                // Clear the path so the caller knows it's in the buffer.
                strcpy(path, "");
            }

            return true;
        }

        Con_Message("Mus_GetExt: Song %s: %s not found.\n", def->id,
                    def->path.path);
    }

    // Try the resource locator.
    if(R_FindResource(RC_MUSIC, def->lumpName, NULL, path))
    {
        // We must read the song into a buffer, because the path may
        // be a virtual file and the audio driver may not know anything about those.
        DFILE*              file;

        file = F_Open(path, "rb");
        ptr = iExt->SongBuffer(len = F_Length(file));
        F_Read(ptr, len, file);
        F_Close(file);

        // Clear the path so the caller knows it's in the buffer.
        strcpy(path, "");

        return true; // Got it!
    }

    lump = W_CheckNumForName(def->lumpName);
    if(lump < 0)
        return false; // No such lump.

    if(Mus_IsMUSLump(lump))
        return false; // It's MUS!

    // Take a copy. Might be a big one (since it could be an MP3), so
    // use the standard memory allocation routines.
    ptr = iExt->SongBuffer(len = W_LumpLength(lump));
    W_ReadLump(lump, ptr);

    return true;
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
    char                path[300];
    int                 order[3], i, songID = def - defs.music;

    // We will not restart the currently playing song.
    if(!musAvail || songID == currentSong)
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
        switch(order[i])
        {
        case MUSP_CD:
            if(Mus_GetCD(def))
                return iCD->Play(Mus_GetCD(def), looped);
            break;

        case MUSP_EXT:
            if(Mus_GetExt(def, path))
            {
                if(path[0])
                {
                    if(verbose)
                        Con_Printf("Mus_Start: %s\n", path);

                    return iExt->PlayFile(path, looped);
                }
                else
                {
                    return iExt->PlayBuffer(looped);
                }
            }
            break;

        case MUSP_MUS:
            if(Mus_GetMUS(def))
                return iMus->Play(looped);
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
    char                buf[300];

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

            Mus_Stop();
            if(Mus_IsMUSLump(i) && iMus)
            {
                ptr = iMus->SongBuffer(len = W_LumpLength(i));
                W_ReadLump(i, ptr);

                return iMus->Play(true);
            }
            else if(!Mus_IsMUSLump(i) && iExt)
            {
                ptr = iExt->SongBuffer(len = W_LumpLength(i));
                W_ReadLump(i, ptr);

                return iExt->PlayBuffer(true);
            }
        }
        else if(!stricmp(argv[1], "file"))
        {
            Mus_Stop();
            M_TranslatePath(argv[2], buf);
            if(iExt)
                return iExt->PlayFile(buf, true);
        }
        else if(!stricmp(argv[1], "cd"))
        {
            Mus_Stop();
            if(iCD)
                return iCD->Play(atoi(argv[2]), true);
        }
        break;
    }

    return true;
}

D_CMD(PlayExt)
{
    char                buf[300];

    Mus_Stop();
    M_TranslatePath(argv[1], buf);

    if(iExt)
        return iExt->PlayFile(buf, true);

    return true;
}

D_CMD(StopMusic)
{
    Mus_Stop();
    return true;
}
