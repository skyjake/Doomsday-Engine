/** @file s_mus.cpp  Music subsystem. 
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "de_platform.h"
#include "audio/s_mus.h"

#include "clientapp.h"
#include "dd_main.h"  // isDedicated

#include "audio/audiodriver.h"
#include "audio/audiodriver_music.h"
#include "audio/m_mus2midi.h"

#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <doomsday/defs/music.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>

using namespace de;

D_CMD(PlayMusic);
D_CMD(PauseMusic);
D_CMD(StopMusic);

static void Mus_UpdateSoundFont();

static dint musPreference  = MUSP_EXT;
static char *soundFontPath = (char *) "";

static dd_bool musAvail;
static dd_bool musicPaused;
static String currentSong;

void Mus_Register()
{
    // Variables:
    C_VAR_INT     ("music-volume",    &musVolume,     0, 0, 255);
    C_VAR_INT     ("music-source",    &musPreference, 0, 0, 2);
    C_VAR_CHARPTR2("music-soundfont", &soundFontPath, 0, 0, 0, Mus_UpdateSoundFont);

    // Commands:
    C_CMD_FLAGS   ("playmusic",  nullptr, PlayMusic,  CMDF_NO_DEDICATED);
    C_CMD_FLAGS   ("pausemusic", nullptr, PauseMusic, CMDF_NO_DEDICATED);
    C_CMD_FLAGS   ("stopmusic",  "",      StopMusic,  CMDF_NO_DEDICATED);
}

bool Mus_Init()
{
    // Already been here?
    if(::musAvail) return true;

    if(::isDedicated || CommandLine_Exists("-nomusic"))
    {
        LOG_AUDIO_NOTE("Music disabled");
        return true;
    }

    LOG_AUDIO_VERBOSE("Initializing Music subsystem...");

    ::currentSong = "";

    // Initialize the available interfaces for music playback.
    dint initialized = 0;
    App_AudioSystem().forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [&initialized] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        if(iMusic->Init())
        {
            initialized += 1;
        }
        else
        {
            LOG_AUDIO_WARNING("Failed to initialize \"%s\" for music playback")
                << App_AudioSystem().interfaceName(iMusic);
        }
        return LoopContinue;
    });
    if(!initialized) return false;

    // Tell the audio driver about our soundfont config.
    Mus_UpdateSoundFont();

    ::musAvail = true;
    return true;
}

void Mus_Shutdown()
{
    if(!::musAvail) return;
    ::musAvail = false;

    // Shutdown interfaces.
    App_AudioSystem().forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        if(iMusic->Shutdown) iMusic->Shutdown();
        return LoopContinue;
    });
}

void Mus_StartFrame()
{
    if(!::musAvail) return;

    // Update all interfaces.
    App_AudioSystem().forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        iMusic->Update();
        return LoopContinue;
    });
}

void Mus_SetVolume(dfloat vol)
{
    if(!::musAvail) return;

    // Set volume of all available interfaces.
    App_AudioSystem().forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [&vol] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        iMusic->Set(MUSIP_VOLUME, vol);
        return LoopContinue;
    });
}

void Mus_Pause(bool doPause)
{
    if(!::musAvail) return;

    // Pause all interfaces.
    App_AudioSystem().forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [&doPause] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        iMusic->Pause(doPause);
        return LoopContinue;
    });
}

void Mus_Stop()
{
    if(!::musAvail) return;

    currentSong = "";

    // Stop all interfaces.
    App_AudioSystem().forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        iMusic->Stop();
        return LoopContinue;
    });
}

/**
 * @return: @c true, if the specified lump contains a MUS song.
 */
static bool Mus_IsMUSLump(lumpnum_t lumpNum)
{
    try
    {
        char buf[4];
        App_FileSystem().lump(lumpNum).read((uint8_t *)buf, 0, 4);

        // ASCII "MUS" and CTRL-Z (hex 4d 55 53 1a)
        return !strncmp(buf, "MUS\x01a", 4);
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore error.
    return false;
}

/**
 * Check for the existence of an "external" music file.
 * Songs can be either in external files or non-MUS lumps.
 *
 * @return  Non-zero if an external file of that name exists.
 */
static dint Mus_GetExt(Record const *rec, ddstring_t *retPath)
{
    LOG_AS("Mus_GetExt");

    if(!rec) return false;
    if(!::musAvail || !App_AudioSystem().musicIsAvailable()) return false;

    defn::Music musicDef(*rec);

    de::Uri songUri(musicDef.gets("path"), RC_NULL);
    if(!songUri.path().isEmpty())
    {
        // All external music files are specified relative to the base path.
        String fullPath = App_BasePath() / songUri.path();

        if(F_Access(fullPath.toUtf8().constData()))
        {
            if(retPath) Str_Set(retPath, fullPath.toUtf8().constData());
            return true;
        }

        LOG_AUDIO_WARNING("Music file \"%s\" not found (id '%s')")
            << songUri << musicDef.gets("id");
    }

    // Try the resource locator?
    String const lumpName = musicDef.gets("lumpName");
    if(!lumpName.isEmpty())
    {
        try
        {
            String foundPath = App_FileSystem().findPath(de::Uri(lumpName, RC_MUSIC), RLF_DEFAULT,
                                                         App_ResourceClass(RC_MUSIC));
            foundPath = App_BasePath() / foundPath; // Ensure the path is absolute.

            // Does the caller want to know the matched path?
            if(retPath)
            {
                Str_Set(retPath, foundPath.toUtf8().constData());
            }
            return true;
        }
        catch(FS1::NotFoundError const &)
        {}  // Ignore this error.
    }
    return false;
}

/**
 * @return  The track number if successful else zero.
 */
static int Mus_GetCD(Record const *rec)
{
    if(!musAvail || !App_AudioSystem().cd() || !rec)
        return 0;

    defn::Music musicDef(*rec);

    int cdTrack = musicDef.geti("cdTrack");
    if(cdTrack) return cdTrack;

    String path = musicDef.gets("path");
    if(!path.compareWithoutCase("cd"))
    {
        bool ok;
        cdTrack = path.toInt(&ok);
        if(ok) return cdTrack;
    }

    return 0;
}

int Mus_StartLump(lumpnum_t lumpNum, bool looped, bool canPlayMUS)
{
    if(!App_AudioSystem().musicIsAvailable() || lumpNum < 0) return 0;

    if(Mus_IsMUSLump(lumpNum))
    {
        // Lump is in DOOM's MUS format. We must first convert it to MIDI.
        if(!canPlayMUS) return -1;

        AutoStr *srcFile = AudioDriver_Music_ComposeTempBufferFilename(".mid");

        // Read the lump, convert to MIDI and output to a temp file in the
        // working directory. Use a filename with the .mid extension so that
        // any player which relies on the it for format recognition works as
        // expected.

        File1 &lump  = App_FileSystem().lump(lumpNum);
        uint8_t *buf = (uint8_t *) M_Malloc(lump.size());

        lump.read(buf, 0, lump.size());
        M_Mus2Midi((void *)buf, lump.size(), Str_Text(srcFile));

        M_Free(buf);

        return AudioDriver_Music_PlayNativeFile(Str_Text(srcFile), looped);
    }
    else
    {
        return AudioDriver_Music_PlayLump(lumpNum, looped);
    }
}

int Mus_Start(Record const *rec, bool looped)
{
    if(!musAvail || !rec) return false;

    String songID = rec->gets("id");

    LOG_AS("Mus_Start");
    LOG_AUDIO_VERBOSE("Starting ID:%s looped:%b, currentSong ID:%s") << songID << looped << currentSong;

    // We will not restart the currently playing song.
    if(songID == currentSong && AudioDriver_Music_IsPlaying())
    {
        return false;
    }

    // Stop the currently playing song.
    Mus_Stop();

    AudioDriver_Music_SwitchBufferFilenames();

    // This is the song we're playing now.
    currentSong = songID;

    // Choose the order in which to try to start the song.
    int order[3];
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
    ddstring_t path;
    for(int i = 0; i < 3; ++i)
    {
        bool canPlayMUS = true;

        switch(order[i])
        {
        case MUSP_CD:
            if(Mus_GetCD(rec))
            {
                if(AudioDriver_Music_PlayCDTrack(Mus_GetCD(rec), looped))
                    return true;
            }
            break;

        case MUSP_EXT:
            Str_Init(&path);
            if(Mus_GetExt(rec, &path))
            {
                LOG_AUDIO_VERBOSE("Attempting to play song '%s' (file \"%s\")")
                        << rec->gets("id") << NativePath(Str_Text(&path)).pretty();

                // Its an external file.
                if(AudioDriver_Music_PlayFile(Str_Text(&path), looped))
                    return true;
            }

            // Next, try non-MUS lumps.
            canPlayMUS = false;

            // Note: Intentionally falls through to MUSP_MUS.

        case MUSP_MUS:
            if(App_AudioSystem().musicIsAvailable())
            {
                lumpnum_t const lumpNum = App_FileSystem().lumpNumForName(rec->gets("lumpName"));
                if(Mus_StartLump(lumpNum, looped, canPlayMUS) == 1)
                    return true;
            }
            break;

        default: DENG2_ASSERT(!"Mus_Start: Invalid value for order[i]"); break;
        }
    }

    // No song was started.
    return false;
}

static void Mus_UpdateSoundFont()
{
    de::NativePath path(soundFontPath);

#ifdef MACOSX
    // On OS X we can try to use the basic DLS soundfont that's part of CoreAudio.
    if(path.isEmpty())
    {
        path = "/System/Library/Components/CoreAudio.component/Contents/Resources/gs_instruments.dls";
    }
#endif

    AudioDriver_Music_Set(AUDIOP_SOUNDFONT_FILENAME,
                          path.expand().toString().toLatin1().constData());
}

/**
 * CCmd: Play a music track.
 */
D_CMD(PlayMusic)
{
    DENG2_UNUSED(src);

    LOG_AS("playmusic (Cmd)");

    if(!musAvail)
    {
        LOGDEV_SCR_ERROR("Music subsystem is not available");
        return false;
    }

    switch(argc)
    {
    default:
        LOG_SCR_NOTE("Usage:\n  %s (music-def)") << argv[0];
        LOG_SCR_MSG("  %s lump (lumpname)") << argv[0];
        LOG_SCR_MSG("  %s file (filename)") << argv[0];
        LOG_SCR_MSG("  %s cd (track)") << argv[0];
        break;

    case 2: {
        dint musIdx = ::defs.getMusicNum(argv[1]);
        if(musIdx < 0)
        {
            LOG_RES_WARNING("Music '%s' not defined") << argv[1];
            return false;
        }

        Mus_Start(&::defs.musics[musIdx], true);
        break; }

    case 3:
        if(!stricmp(argv[1], "lump"))
        {
            lumpnum_t lump = App_FileSystem().lumpNumForName(argv[2]);
            if(lump < 0) return false;  // No such lump.

            Mus_Stop();
            return AudioDriver_Music_PlayLump(lump, true);
        }
        else if(!stricmp(argv[1], "file"))
        {
            Mus_Stop();
            return AudioDriver_Music_PlayFile(argv[2], true);
        }
        else
        {   
            // Perhaps a CD track?
            if(!stricmp(argv[1], "cd"))
            {
                if(!App_AudioSystem().cd())
                {
                    LOG_AUDIO_WARNING("No CD audio interface available");
                    return false;
                }

                Mus_Stop();
                return AudioDriver_Music_PlayCDTrack(atoi(argv[2]), true);
            }
        }
        break;
    }

    return true;
}

D_CMD(StopMusic)
{
    DENG2_UNUSED3(src, argc, argv);

    Mus_Stop();
    return true;
}

D_CMD(PauseMusic)
{
    DENG2_UNUSED3(src, argc, argv);

    musicPaused = !musicPaused;
    Mus_Pause(musicPaused);
    return true;
}

