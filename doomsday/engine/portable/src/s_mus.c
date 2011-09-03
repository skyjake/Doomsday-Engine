/**\file s_mus.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
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
 * Music Subsystem.
 */

// HEADER FILES ------------------------------------------------------------

#if WIN32
# include <math.h> // for sqrt()
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
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
    const char* name;
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
    unsigned int i;

    if(musAvail)
        return true; // Already initialized.

    if(isDedicated || ArgExists("-nomusic"))
    {
        Con_Message("Music disabled.\n");
        return true;
    }

    VERBOSE( Con_Message("Initializing Music subsystem...\n") )

    // Use the external music playback facilities, if available.
    if(audioDriver == &audiod_dummy)
    {
        iMusic = 0;
        iCD  = 0;
    }
    else
    {
#ifdef MACOSX
        // On the Mac, just use QuickTime for the music and be done with it.
        iMusic = &audiodQuickTimeMusic;
        iCD = (audiodExternalICD.gen.Init  ? &audiodExternalICD  : 0);
#else
        if(audioDriver == &audiod_sdlmixer)
        {
            iMusic = (audiointerface_music_t*) &audiod_sdlmixer_music;
            iCD  = 0;
        }
        else
        {
            iMusic = (audiodExternalIMusic.gen.Init ? &audiodExternalIMusic : 0);
            iCD  = (audiodExternalICD.gen.Init  ? &audiodExternalICD  : 0);
        }
#endif
    }

    // Initialize the chosen interfaces.
    for(i = 0; i < NUM_INTERFACES; ++i)
    {
        if(*interfaces[i].ip && !(*interfaces[i].ip)->Init())
        {
            Con_Message("Warning:Mus_Init: Failed to initialize %s interface.\n",
                        interfaces[i].name);
            *interfaces[i].ip = NULL;
        }
    }

    // Print a list of the chosen interfaces.
    if(verbose >= 2)
    {
        char buf[40];
        Con_Printf("Music configuration:\n");
        for(i = 0; i < NUM_INTERFACES; ++i)
        {
            if(*interfaces[i].ip && !(*interfaces[i].ip)->Get(MUSIP_ID, buf))
                strcpy(buf, "?");
            Con_Printf("  %s: %s\n", interfaces[i].name, buf);
        }
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
 * @return: @c true, if the specified lump contains a MUS song.
 */
boolean Mus_IsMUSLump(lumpnum_t lumpNum)
{
    char buf[4];
    int lumpIdx;
    abstractfile_t* fsObject = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    if(!fsObject) false;
    
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
    if(!musAvail || !iMusic)
        return false;

    // All external music files are specified relative to the base path.
    if(def->path && !Str_IsEmpty(Uri_Path(def->path)))
    {
        ddstring_t fullPath, *path;

        Str_Init(&fullPath);
        F_FixSlashes(&fullPath, Uri_Path(def->path));
        F_PrependBasePath(&fullPath, &fullPath);
        if(F_Access(Str_Text(&fullPath)))
        {
            if(NULL != retPath)
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
 * @return:             The track number if successful else zero.
 */
int Mus_GetCD(ded_music_t* def)
{
    if(!musAvail || !iCD || !def)
        return 0;

    if(def->cdTrack)
        return def->cdTrack;

    if(def->path && !stricmp(Str_Text(Uri_Scheme(def->path)), "cd"))
        return atoi(Str_Text(Uri_Path(def->path)));

    return 0;
}

/// @return  Composed music file name. Must be released with Str_Delete()
static ddstring_t* composeBufferedMusicFilename(int id, const char* ext)
{
#define BUFFERED_MUSIC_FILE      "dd-buffered-song"

    if(ext && ext[0])
    {
        return Str_Appendf(Str_New(), "%s%i%s", BUFFERED_MUSIC_FILE, id, ext);
    }

    return Str_Appendf(Str_New(), "%s%i", BUFFERED_MUSIC_FILE, id);

#undef BUFFERED_MUSIC_FILE
}

int Mus_Start(ded_music_t* def, boolean looped)
{
    static int currentBufFile = 0;

    int i, order[3], songID;
    ddstring_t path;

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
        boolean canPlayMUS = true;

        switch(order[i])
        {
        case MUSP_CD:
            if(Mus_GetCD(def))
                return iCD->Play(Mus_GetCD(def), looped);
            break;

        case MUSP_EXT:
            Str_Init(&path);
            if(Mus_GetExt(def, &path))
            {   // Its an external file.
                // The song may be in a virtual file, so we must buffer
                // it ourselves.
                abstractfile_t* file = F_Open(Str_Text(&path), "rb");
                size_t len = F_Length(AbstractFile_Handle(file));

                if(!iMusic->Play)
                {   // Music interface does not offer buffer playback.
                    // Write to disk and play from there.
                    ddstring_t* fileName = composeBufferedMusicFilename(currentBufFile ^= 1, NULL);

                    { FILE* outFile = fopen(Str_Text(fileName), "wb");
                    if(NULL != outFile)
                    {
                        int result;
                        uint8_t* buf = (uint8_t*)malloc(len);
                        if(NULL == buf)
                        {
                            Con_Message("Warning:Mus_Start: Failed on allocation of %lu bytes for "
                                "temporary song write buffer.\n", (unsigned long) len);
                            Str_Delete(fileName);
                            return false;
                        }

                        // Write the song into the buffer file.
                        F_Read(AbstractFile_Handle(file), buf, len);
                        fwrite(buf, 1, len, outFile);
                        fclose(outFile);
                        F_Delete(file);
                        free(buf);

                        // Music maestro, if you please!
                        result = iMusic->PlayFile(Str_Text(fileName), looped);
                        Str_Delete(fileName);
                        return result;
                    }}

                    Con_Message("Mus_Start: Failed opening \"%s\" for writing (%s).\n",
                        Str_Text(fileName), strerror(errno));
                    F_Delete(file);
                    Str_Delete(fileName);
                    return false;
                }
                else
                {   // Music interface offers buffered playback. Use it.
                    void* ptr;

                    VERBOSE( Con_Message("Mus_GetExt: Opened song '%s' (file \"%s\" %lu bytes).\n",
                        def->id, F_PrettyPath(Str_Text(&path)), (unsigned long) len) )

                    ptr = iMusic->SongBuffer(len);
                    F_Read(AbstractFile_Handle(file), (uint8_t*)ptr, len);
                    F_Delete(file);

                    return iMusic->Play(looped);
                }
            }

            // Next, try non-MUS lumps.
            canPlayMUS = false;
            // Fall through.

        case MUSP_MUS:
            if(iMusic)
            {
                lumpnum_t absoluteLumpNum;
                if(def->lumpName && -1 != (absoluteLumpNum = F_CheckLumpNumForName(def->lumpName, true)))
                {
                    ddstring_t* fileName = NULL;
                    abstractfile_t* fsObject;
                    size_t lumpLength;
                    int lumpIdx;

                    if(Mus_IsMUSLump(absoluteLumpNum))
                    {   // Lump is in DOOM's MUS format.
                        
                        uint8_t* buf;

                        if(!canPlayMUS)
                            break;

                        fileName = composeBufferedMusicFilename(currentBufFile ^= 1, ".mid");

                        // Read the lump, convert to MIDI and output to a temp file in the
                        // working directory. Use a filename with the .mid extension so that
                        // any player which relies on the it for format recognition works as
                        // expected.

                        lumpLength = F_LumpLength(absoluteLumpNum);
                        buf = (uint8_t*) malloc(lumpLength);
                        if(NULL == buf)
                        {
                            Con_Message("Warning:Mus_Start: Failed on allocation of %lu bytes for "
                                "temporary MUS to MIDI conversion buffer.\n", (unsigned long) lumpLength);
                            Str_Delete(fileName);
                            return false;
                        }
                        fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
                        F_ReadLumpSection(fsObject, lumpIdx, buf, 0, lumpLength);

                        M_Mus2Midi((void*)buf, lumpLength, Str_Text(fileName));
                        free(buf);
                    }
                    else if(!iMusic->Play)
                    {   // Music interface does not offer buffer playback.
                        // Write this lump to disk and play from there.
                        fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
                        fileName = composeBufferedMusicFilename(currentBufFile ^= 1, 0);
                        if(!F_DumpLump(fsObject, lumpIdx, Str_Text(fileName)))
                        {
                            Str_Delete(fileName);
                            return false;
                        }
                    }

                    if(NULL != fileName)
                    {
                        int result = iMusic->PlayFile(Str_Text(fileName), looped);
                        Str_Delete(fileName);
                        return result;
                    }

                    fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
                    lumpLength = F_LumpLength(absoluteLumpNum);
                    F_ReadLumpSection(fsObject, lumpIdx, (uint8_t*)iMusic->SongBuffer(lumpLength), 0, lumpLength);
                    return iMusic->Play(looped);
                }
            }
            break;

        default:
            Con_Error("Mus_Start: Invalid value order[i] = %i.", order[i]);
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
            lumpnum_t lumpNum = F_CheckLumpNumForName(argv[2], true);
            abstractfile_t* fsObject;
            size_t lumpLength;
            uint8_t* ptr;
            int lumpIdx;

            if(0 > lumpNum)
                return false; // No such lump.

            if(NULL == iMusic)
            {
                Con_Printf("No music interface available.\n");
                return false;
            }

            Mus_Stop();
            lumpLength = F_LumpLength(lumpNum);
            ptr = (uint8_t*) iMusic->SongBuffer(lumpLength);

            fsObject = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
            F_ReadLumpSection(fsObject, lumpIdx, ptr, 0, lumpLength);

            return iMusic->Play(true);
        }
        else if(!stricmp(argv[1], "file"))
        {
            ddstring_t path;
            int result = 0;

            if(NULL == iMusic)
            {
                Con_Printf("No music interface available.\n");
                return false;
            }

            // Compose the file path.
            Str_Init(&path);
            Str_Set(&path, argv[2]);
            F_FixSlashes(&path, &path);
            F_ExpandBasePath(&path, &path);

            Mus_Stop();
            result = iMusic->PlayFile(Str_Text(&path), true);
            Str_Free(&path);
            return result;
        }
        else
        {   // Perhaps a CD track?
            if(!stricmp(argv[1], "cd"))
            {
                if(NULL == iCD)
                {
                    Con_Printf("No CDAudio interface available.\n");
                    return false;
                }

                Mus_Stop();
                return iCD->Play(atoi(argv[2]), true);
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

