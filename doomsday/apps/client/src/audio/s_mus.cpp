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

#include "audio/m_mus2midi.h"

#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <doomsday/defs/music.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <de/memory.h>

using namespace de;

static char const *BUFFERED_MUSIC_FILE = (char *)"dd-buffered-song";

static dint musPreference = MUSP_EXT;

static bool musAvail;
static String currentSong;
static bool musicPaused;

static bool needBufFileSwitch;  ///< @c true= choose a new file name for the buffered playback file when asked. */

static String composeBufferFilename(String const &ext = "")
{
    // Switch the name of the buffered song file?
    static dint currentBufFile = 0;
    if(::needBufFileSwitch)
    {
        currentBufFile ^= 1;
        ::needBufFileSwitch = false;
    }

    // Compose the name.
    return BUFFERED_MUSIC_FILE + String::number(currentBufFile) + ext;
}

/**
 * Returns @c true if the given @a file appears to contain MUS format music.
 */
static bool recognizeMus(de::File1 &file)
{
    char buf[4];
    file.read((uint8_t *)buf, 0, 4);

    // ASCII "MUS" and CTRL-Z (hex 4d 55 53 1a)
    return !qstrncmp(buf, "MUS\x01a", 4);
}

/**
 * Attempt to locate a music file referenced in the given music @a definition. Songs can be
 * either in external files or non-MUS lumps.
 *
 * @note Lump based music is presently handled separately!!
 *
 * @param definition  Music definition to find the music file for.
 *
 * @return  Absolute path to the music if found; otherwise a zero-length string.
 */
static String tryFindMusicFile(Record const &definition)
{
    LOG_AS("tryFindMusicFile");

    defn::Music const music(definition);

    de::Uri songUri(music.gets("path"), RC_NULL);
    if(!songUri.path().isEmpty())
    {
        // All external music files are specified relative to the base path.
        String fullPath = App_BasePath() / songUri.path();
        if(F_Access(fullPath.toUtf8().constData()))
        {
            return fullPath;
        }

        LOG_AUDIO_WARNING("Music file \"%s\" not found (id '%s')")
            << songUri << music.gets("id");
    }

    // Try the resource locator.
    String const lumpName = music.gets("lumpName");
    if(!lumpName.isEmpty())
    {
        try
        {
            String const foundPath = App_FileSystem().findPath(de::Uri(lumpName, RC_MUSIC), RLF_DEFAULT,
                                                               App_ResourceClass(RC_MUSIC));
            return App_BasePath() / foundPath;  // Ensure the path is absolute.
        }
        catch(FS1::NotFoundError const &)
        {}  // Ignore this error.
    }
    return "";  // None found.
}

static dint playFile(String const &virtualOrNativePath, bool looped)
{
    DENG2_ASSERT(::musAvail && App_AudioSystem().musicIsAvailable());

    return App_AudioSystem().forAllInterfaces(AUDIO_IMUSIC, [&virtualOrNativePath, &looped] (void *ifs)
    {
        try
        {
            auto *iMusic = (audiointerface_music_t *) ifs;

            // Relative paths are relative to the native working directory.
            String path  = (NativePath::workPath() / NativePath(virtualOrNativePath).expand()).withSeparators('/');
            std::unique_ptr<FileHandle> hndl(&App_FileSystem().openFile(path, "rb"));
            dsize const len = hndl->length();

            // Does this interface offer buffered playback?
            if(iMusic->Play && iMusic->SongBuffer)
            {
                // Buffer the data using the driver's own facility.
                hndl->read((duint8 *) iMusic->SongBuffer(len), len);
                App_FileSystem().releaseFile(hndl->file());

                return iMusic->Play(looped);
            }
            // Does this interface offer playback from a native file?
            else if(iMusic->PlayFile)
            {
                // Write the data to disk and play from there.
                String const fileName = composeBufferFilename();

                duint8 *buf = (duint8 *)M_Malloc(len);
                hndl->read(buf, len);
                F_Dump(buf, len, fileName.toUtf8().constData());
                M_Free(buf); buf = nullptr;

                App_FileSystem().releaseFile(hndl->file());

                // Music maestro, if you please!
                return iMusic->PlayFile(fileName.toUtf8().constData(), looped);
            }
            else
            {
                App_FileSystem().releaseFile(hndl->file());
            }
        }
        catch(FS1::NotFoundError const &)
        {}  // Ignore this error.
        return 0;  // Continue
    });
}

/**
 * @return  @c  1= if music was started. 0, if attempted to start but failed.
 *          @c -1= if it was MUS data and @a canPlayMUS says we can't play it.
 */
static dint playLump(lumpnum_t lumpNum, bool looped = false, bool canPlayMUS = true)
{
    DENG2_ASSERT(::musAvail && App_AudioSystem().musicIsAvailable());

    if(!App_FileSystem().nameIndex().hasLump(lumpNum))
        return 0;

    File1 &lump = App_FileSystem().lump(lumpNum);
    if(recognizeMus(lump))
    {
        // Lump is in DOOM's MUS format. We must first convert it to MIDI.
        if(!canPlayMUS) return -1;

        String const srcFile = composeBufferFilename(".mid");

        // Read the lump, convert to MIDI and output to a temp file in the working directory.
        // Use a filename with the .mid extension so that any player which relies on the it
        // for format recognition works as expected.
        duint8 *buf = (duint8 *) M_Malloc(lump.size());
        lump.read(buf, 0, lump.size());
        M_Mus2Midi((void *)buf, lump.size(), srcFile.toUtf8().constData());
        M_Free(buf); buf = nullptr;

        return App_AudioSystem().forAllInterfaces(AUDIO_IMUSIC, [&srcFile, &looped] (void *ifs)
        {
            auto *iMusic = (audiointerface_music_t *) ifs;
            if(iMusic->PlayFile)
            {
                return iMusic->PlayFile(srcFile.toUtf8().constData(), looped);
            }
            return 0;  // Continue.
        });
    }

    return App_AudioSystem().forAllInterfaces(AUDIO_IMUSIC, [&lump, &looped] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_t *) ifs;

        // Does this interface offer buffered playback?
        if(iMusic->Play && iMusic->SongBuffer)
        {
            // Buffer the data using the driver's own facility.
            std::unique_ptr<FileHandle> hndl(&App_FileSystem().openLump(lump));
            dsize const length  = hndl->length();
            hndl->read((duint8 *) iMusic->SongBuffer(length), length);
            App_FileSystem().releaseFile(hndl->file());

            return iMusic->Play(looped);
        }
        // Does this interface offer playback from a native file?
        else if(iMusic->PlayFile)
        {
            // Write the data to disk and play from there.
            String const fileName = composeBufferFilename();
            if(!F_DumpFile(lump, fileName.toUtf8().constData()))
            {
                // Failed to write the lump...
                return 0;
            }

            return iMusic->PlayFile(fileName.toUtf8().constData(), looped);
        }

        return 0;  // Continue.
    });
}

static dint playCDTrack(dint track, bool looped)
{
    return App_AudioSystem().forAllInterfaces(AUDIO_ICD, [&track, &looped] (void *ifs)
    {
        auto *iCD = (audiointerface_cd_t *) ifs;
        if(iCD->Play)
        {
            return iCD->Play(track, looped);
        }
        return 0;  // Continue.
    });
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
    App_AudioSystem().updateSoundFont();

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

    ::currentSong = "";

    // Stop all interfaces.
    App_AudioSystem().forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        iMusic->Stop();
        return LoopContinue;
    });
}

dint Mus_Start(Record const &definition, bool looped)
{
    if(!::musAvail) return false;

    LOG_AS("Mus_Start");
    LOG_AUDIO_VERBOSE("Starting ID:%s looped:%b, currentSong ID:%s")
        << definition.gets("id") << looped << currentSong;

    // We will not restart the currently playing song.
    if(definition.gets("id") == currentSong && Mus_IsPlaying())
        return false;

    // Stop the currently playing song.
    Mus_Stop();

    // Switch to an unused file buffer if asked.
    ::needBufFileSwitch = true;

    // This is the song we're playing now.
    ::currentSong = definition.gets("id");

    // Determine the music source, order preferences.
    dint source[3];
    source[0] = ::musPreference;
    switch(::musPreference)
    {
    case MUSP_CD:
        source[1] = MUSP_EXT;
        source[2] = MUSP_MUS;
        break;

    case MUSP_EXT:
        source[1] = MUSP_MUS;
        source[2] = MUSP_CD;
        break;

    default: // MUSP_MUS
        source[1] = MUSP_EXT;
        source[2] = MUSP_CD;
        break;
    }

    // Try to start the song.
    for(dint i = 0; i < 3; ++i)
    {
        bool canPlayMUS = true;

        switch(source[i])
        {
        case MUSP_CD:
            if(App_AudioSystem().cd())
            {
                if(dint cdTrack = defn::Music(definition).cdTrack())
                {
                    if(playCDTrack(cdTrack, looped))
                        return true;
                }
            }
            break;

        case MUSP_EXT: {
            String filePath = tryFindMusicFile(definition);
            if(!filePath.isEmpty())
            {
                LOG_AUDIO_VERBOSE("Attempting to play song '%s' (file \"%s\")")
                    << definition.gets("id") << NativePath(filePath).pretty();

                // Its an external file.
                if(playFile(filePath, looped))
                    return true;
            }

            // Next, try non-MUS lumps.
            canPlayMUS = false;

            }  // Note: Intentionally falls through to MUSP_MUS.

        case MUSP_MUS:
            if(playLump(App_FileSystem().lumpNumForName(definition.gets("lumpName")),
                        looped, canPlayMUS) == 1)
            {
                return true;
            }
            break;

        default: DENG2_ASSERT(!"Mus_Start: Invalid value for order[i]"); break;
        }
    }

    // No song was started.
    return false;
}

bool Mus_IsPlaying()
{
    return App_AudioSystem().forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_t *) ifs;
        return iMusic->gen.Get(MUSIP_PLAYING, nullptr);
    });
}

/**
 * CCmd: Play a music track.
 */
D_CMD(PlayMusic)
{
    DENG2_UNUSED(src);

    LOG_AS("playmusic (Cmd)");

    if(!::musAvail || !App_AudioSystem().musicIsAvailable())
    {
        LOGDEV_SCR_ERROR("Music subsystem is not available");
        return false;
    }

    bool const looped = true;

    if(argc == 2)
    {
        // Play a file associated with the referenced music definition.
        if(Record const *definition = ::defs.musics.tryFind("id", argv[1]))
        {
            return Mus_Start(*definition, looped);
        }
        LOG_RES_WARNING("Music '%s' not defined") << argv[1];
        return false;
    }

    if(argc == 3)
    {
        // Play a file referenced directly.
        if(!qstricmp(argv[1], "lump"))
        {
            Mus_Stop();
            return playLump(App_FileSystem().lumpNumForName(argv[2]), looped);
        }
        else if(!qstricmp(argv[1], "file"))
        {
            Mus_Stop();
            return playFile(argv[2], looped);
        }
        else if(!qstricmp(argv[1], "cd"))
        {
            if(!App_AudioSystem().cd())
            {
                LOG_AUDIO_WARNING("No CD audio interface available");
                return false;
            }

            Mus_Stop();
            return playCDTrack(String(argv[2]).toInt(), looped);
        }
        return false;
    }

    LOG_SCR_NOTE("Usage:\n  %s (music-def)") << argv[0];
    LOG_SCR_MSG("  %s lump (lumpname)") << argv[0];
    LOG_SCR_MSG("  %s file (filename)") << argv[0];
    LOG_SCR_MSG("  %s cd (track)") << argv[0];
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

    ::musicPaused = !::musicPaused;
    Mus_Pause(::musicPaused);
    return true;
}

void Mus_ConsoleRegister()
{
    // Variables:
    C_VAR_INT     ("music-volume",    &::musVolume,     0, 0, 255);
    C_VAR_INT     ("music-source",    &::musPreference, 0, 0, 2);

    // Commands:
    C_CMD_FLAGS   ("playmusic",  nullptr, PlayMusic,  CMDF_NO_DEDICATED);
    C_CMD_FLAGS   ("pausemusic", nullptr, PauseMusic, CMDF_NO_DEDICATED);
    C_CMD_FLAGS   ("stopmusic",  "",      StopMusic,  CMDF_NO_DEDICATED);
}
