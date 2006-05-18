/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * s_mus.c: Music Subsystem
 */

// HEADER FILES ------------------------------------------------------------

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
    musinterface_generic_t **ip;
    const char *name;
} interface_info_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     mus_preference = MUSP_EXT;

#ifdef UNIX
// On Unix, all sound and music interfaces are loaded dynamically.
musdriver_t musd_loaded;
musinterface_mus_t musd_loaded_imus;
musinterface_ext_t musd_loaded_iext;
musinterface_cd_t musd_loaded_icd;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean mus_avail = false;

static int current_song = -1;

// The interfaces.
static musinterface_mus_t *imus;
static musinterface_ext_t *iext;
static musinterface_cd_t *icd;

// The interface list. Used to access the common features of all the
// interfaces conveniently.
static interface_info_t interfaces[] = {
    {(musinterface_generic_t **) & imus, "Mus"},
    {(musinterface_generic_t **) & iext, "Ext"},
    {(musinterface_generic_t **) & icd, "CD"}
};

// CODE --------------------------------------------------------------------

/*
 * Initialize the Mus module and choose the interfaces to use. Returns
 * true if no errors occur.
 */
boolean Mus_Init(void)
{
    int     i;

    if(isDedicated || mus_avail || ArgExists("-nomusic"))
        return true;

#ifdef WIN32
    // The Win driver is always initialized.
    if(musd_win.Init())
    {
        // Use Win's Mus interface.
        imus = &musd_win_imus;
    }
    else
    {
        Con_Message("Mus_Init: Failed to initialize Win driver.\n");
    }

    // Can we use FMOD?
    if(!ArgExists("-nofmod") && musd_fmod.Init())
    {
        // FMOD has been successfully initialized.
        // We get the CD and Ext interfaces.
        iext = &musd_fmod_iext;
        icd = &musd_fmod_icd;
    }
    else
    {
        // FMOD is either disabled or the init failed.
        // Must rely on Windows, then, without an Ext interface.
        icd = &musd_win_icd;
    }
#endif
#ifdef UNIX
    // The available interfaces have already been loaded.
    if(musd_loaded.Init())
    {
        imus = (musd_loaded_imus.gen.Init ? &musd_loaded_imus : 0);
        iext = (musd_loaded_iext.gen.Init ? &musd_loaded_iext : 0);
        icd = (musd_loaded_icd.gen.Init ? &musd_loaded_icd : 0);
    }
#endif

    // Initialize the chosen interfaces.
    for(i = 0; i < NUM_INTERFACES; i++)
        if(*interfaces[i].ip && !(*interfaces[i].ip)->Init())
        {
            Con_Message("Mus_Init: Failed to initialize %s interface.\n",
                        interfaces[i].name);

            *interfaces[i].ip = NULL;
        }

    // Print a list of the chosen interfaces.
    if(verbose)
    {
        char    buf[40];

        Con_Printf("Mus_Init: Interfaces:");
        for(i = 0; i < NUM_INTERFACES; i++)
            if(*interfaces[i].ip)
            {
                if(!(*interfaces[i].ip)->Get(MUSIP_ID, buf))
                    strcpy(buf, "?");
                Con_Printf(" %s", buf);
            }

        Con_Printf("\n");
    }

    current_song = -1;
    mus_avail = true;
    return true;
}

void Mus_Shutdown(void)
{
    if(!mus_avail)
        return;
    mus_avail = false;

    // Shut down the drivers. They shut down their interfaces automatically.
#ifdef WIN32
    musd_fmod.Shutdown();
    musd_win.Shutdown();
#endif
#ifdef UNIX
    musd_loaded.Shutdown();
#endif

    // No more interfaces.
    imus = 0;
    iext = 0;
    icd = 0;
}

/*
 * Called on each frame by S_StartFrame.
 */
void Mus_StartFrame(void)
{
    int     i;

    if(!mus_avail)
        return;

    // Update all interfaces.
    for(i = 0; i < NUM_INTERFACES; i++)
        if(*interfaces[i].ip)
            (*interfaces[i].ip)->Update();
}

/*
 * Set the general music volume. Affects all music played by all
 * interfaces.
 */
void Mus_SetVolume(float vol)
{
    int     i;

    if(!mus_avail)
        return;

    // Set volume of all available interfaces.
    for(i = 0; i < NUM_INTERFACES; i++)
        if(*interfaces[i].ip)
            (*interfaces[i].ip)->Set(MUSIP_VOLUME, vol);
}

/*
 * Pauses or resumes the music.
 */
void Mus_Pause(boolean do_pause)
{
    int     i;

    if(!mus_avail)
        return;

    // Pause all interfaces.
    for(i = 0; i < NUM_INTERFACES; i++)
        if(*interfaces[i].ip)
            (*interfaces[i].ip)->Pause(do_pause);
}

void Mus_Stop(void)
{
    int     i;

    if(!mus_avail)
        return;

    current_song = -1;

    // Stop all interfaces.
    for(i = 0; i < NUM_INTERFACES; i++)
        if(*interfaces[i].ip)
            (*interfaces[i].ip)->Stop();
}

/*
 * @return: boolean         (true) if the specified lump contains a MUS song.
 */
boolean Mus_IsMUSLump(int lump)
{
    char    buf[8];

    W_ReadLumpSection(lump, buf, 0, 4);

    // ASCII "MUS" and CTRL-Z (hex 4d 55 53 1a)
    return !strncmp(buf, "MUS\x01a", 4);
}

/*
 * The lump may contain non-MUS data.
 *
 * @return: int             (true) if successful.
 */
int Mus_GetMUS(ded_music_t * def)
{
    int     len, lumpnum;
    void   *ptr;

    if(!mus_avail || !imus)
        return false;

    lumpnum = W_CheckNumForName(def->lumpname);
    if(lumpnum < 0)
        return false;           // No such lump.

    // Is this MUS data or what?
    if(!Mus_IsMUSLump(lumpnum))
        return false;

    ptr = imus->SongBuffer(len = W_LumpLength(lumpnum));
    W_ReadLump(lumpnum, ptr);
    return true;
}

/*
 * Load a song file.
 * Songs can be either in external files or non-MUS lumps.
 *
 * @return: int             (true) if an external file of that name exists.
 */
int Mus_GetExt(ded_music_t * def, char *path)
{
    char    buf[300];
    int     lumpnum, len;
    void   *ptr;

    if(!mus_avail || !iext)
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
                // it ourselves. Otherwise FMOD might not be able to load
                // the song.
                DFILE  *file = F_Open(buf, "rb");

                ptr = iext->SongBuffer(len = F_Length(file));
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
    if(R_FindResource(RC_MUSIC, def->lumpname, NULL, path))
    {
        // We must read the song into a buffer, because the path may
        // be a virtual file and FMOD doesn't know anything about those.
        DFILE  *file = F_Open(path, "rb");

        ptr = iext->SongBuffer(len = F_Length(file));
        F_Read(ptr, len, file);
        F_Close(file);
        // Clear the path so the caller knows it's in the buffer.
        strcpy(path, "");
        return true;            // Got it!
    }

    lumpnum = W_CheckNumForName(def->lumpname);
    if(lumpnum < 0)
        return false;           // No such lump.

    if(Mus_IsMUSLump(lumpnum))
        return false;           // It's MUS!

    // Take a copy. Might be a big one (since it could be an MP3), so
    // use the standard memory allocation routines.
    ptr = iext->SongBuffer(len = W_LumpLength(lumpnum));
    W_ReadLump(lumpnum, ptr);
    return true;
}

/*
 * @return: int         The track number if successful else zero.
 */
int Mus_GetCD(ded_music_t * def)
{
    if(!mus_avail || !icd)
        return 0;
    if(def->cdtrack)
        return def->cdtrack;
    if(!strnicmp(def->path.path, "cd:", 3))
        return atoi(def->path.path + 3);
    return 0;
}

/*
 * Start playing a song. The chosen interface depends on what's available
 * and what kind of resources have been associated with the song. Returns
 * true if the song is successfully played. Any previously playing song
 * is stopped.
 */
int Mus_Start(ded_music_t * def, boolean looped)
{
    char    path[300];
    int     order[3], i, song_id = def - defs.music;

    // We will not restart the currently playing song.
    if(!mus_avail || song_id == current_song)
        return false;

    // Stop the currently playing song.
    Mus_Stop();

    // This is the song we're playing now.
    current_song = song_id;

    // Choose the order in which to try to start the song.
    order[0] = mus_preference;
    switch (mus_preference)
    {
    case MUSP_CD:
        order[1] = MUSP_EXT;
        order[2] = MUSP_MUS;
        break;

    case MUSP_EXT:
        order[1] = MUSP_MUS;
        order[2] = MUSP_CD;
        break;

    default:                    // MUSP_MUS
        order[1] = MUSP_EXT;
        order[2] = MUSP_CD;
        break;
    }

    // Try to start the song.
    for(i = 0; i < 3; i++)
    {
        switch (order[i])
        {
        case MUSP_CD:
            if(Mus_GetCD(def))
                return icd->Play(Mus_GetCD(def), looped);
            break;

        case MUSP_EXT:
            if(Mus_GetExt(def, path))
            {
                if(path[0])
                {
                    if(verbose)
                        Con_Printf("Mus_Start: %s\n", path);
                    return iext->PlayFile(path, looped);
                }
                else
                    return iext->PlayBuffer(looped);
            }
            break;

        case MUSP_MUS:
            if(Mus_GetMUS(def))
                return imus->Play(looped);
            break;
        }
    }

    // The song was not started.
    return false;
}

/*
 * CCmd: Play a music track.
 */
D_CMD(PlayMusic)
{
    int     i, len;
    void   *ptr;
    char    buf[300];

    if(!mus_avail)
    {
        Con_Printf("The Mus module is not available.\n");
        return false;
    }

    switch (argc)
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
                return false;   // No such lump.
            Mus_Stop();
            if(Mus_IsMUSLump(i) && imus)
            {
                ptr = imus->SongBuffer(len = W_LumpLength(i));
                W_ReadLump(i, ptr);
                return imus->Play(true);
            }
            else if(!Mus_IsMUSLump(i) && iext)
            {
                ptr = iext->SongBuffer(len = W_LumpLength(i));
                W_ReadLump(i, ptr);
                return iext->PlayBuffer(true);
            }
        }
        else if(!stricmp(argv[1], "file"))
        {
            Mus_Stop();
            M_TranslatePath(argv[2], buf);
            if(iext)
                return iext->PlayFile(buf, true);
        }
        else if(!stricmp(argv[1], "cd"))
        {
            Mus_Stop();
            if(icd)
                return icd->Play(atoi(argv[2]), true);
        }
        break;
    }
    return true;
}

D_CMD(PlayExt)
{
    char    buf[300];

    if(argc != 2)
    {
        Con_Printf("Usage: %s (filename)\n", argv[0]);
        return true;
    }
    Mus_Stop();
    M_TranslatePath(argv[1], buf);
    if(iext)
        return iext->PlayFile(buf, true);
    return true;
}

D_CMD(StopMusic)
{
    Mus_Stop();
    return true;
}
