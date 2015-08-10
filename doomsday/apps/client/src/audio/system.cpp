/** @file system.cpp  Audio subsystem module.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au> *
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#define DENG_NO_API_MACROS_SOUND

#include "audio/system.h"

#ifdef __CLIENT__
#  include "dd_main.h"  // isDedicated
#endif

#ifdef __SERVER__
#  include "server/sv_sound.h"
#endif

#include "audio/s_cache.h"
#include "audio/s_sfx.h"
#ifdef __CLIENT__
#  include "audio/m_mus2midi.h"
#endif

#include "api_map.h"
#include "world/p_players.h"
#include "world/sector.h"

#include <doomsday/audio/logical.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#ifdef __CLIENT__
#  include <doomsday/defs/music.h>
#  include <doomsday/filesys/fs_main.h>
#  include <doomsday/filesys/fs_util.h>
#endif
#include <de/App>
#include <de/memory.h>

using namespace de;

dint soundMinDist = 256;  // No distance attenuation this close.
dint soundMaxDist = 2025;

// Setting these variables is enough to adjust the volumes.
// S_StartFrame() will call the actual routines to change the volume
// when there are changes.
dint sfxVolume = 255 * 2/3;
dint musVolume = 255 * 2/3;

dint sfxBits = 8;
dint sfxRate = 11025;

byte sfxOneSoundPerEmitter;  // Traditional Doomsday behavior: allows sounds to overlap.

bool noRndPitch;

#ifdef __CLIENT__
#  ifdef MACOSX
/// Built-in QuickTime audio interface implemented by MusicPlayer.m
DENG_EXTERN_C audiointerface_music_t audiodQuickTimeMusic;
#  endif
#endif

namespace audio {

static audio::System *theAudioSystem = nullptr;

#ifdef __CLIENT__
static char const *BUFFERED_MUSIC_FILE = (char *)"dd-buffered-song";

static char *musSoundFontPath = (char *) "";

/**
 * If multiple sources are available, this setting is used to determine which one to
 * use (mus < ext < cd).
 */
static audio::System::MusicSource musSourcePreference = audio::System::MUSP_EXT;

static audiodriverid_t identifierToDriverId(String name)
{
    static String const driverIdentifier[AUDIODRIVER_COUNT] = {
        "dummy",
        "sdlmixer",
        "openal",
        "fmod",
        "fluidsynth",
        "dsound",
        "winmm"
    };

    name = name.toLower();

    for(dint i = 0; i < AUDIODRIVER_COUNT; ++i)
    {
        if(name == driverIdentifier[i]) return audiodriverid_t( i );
    }

    LOG_AUDIO_ERROR("'%s' is not a valid audio driver name") << name;
    return AUDIOD_INVALID;
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
 *
 * @todo Move to ResourceSystem.
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
#endif

DENG2_PIMPL(System)
, DENG2_OBSERVES(App, GameUnload)
{
#ifdef __CLIENT__
    AudioDriver drivers[AUDIODRIVER_COUNT];

    AudioDriver &driverById(audiodriverid_t id)
    {
        DENG2_ASSERT(VALID_AUDIODRIVER_IDENTIFIER(id));
        return drivers[id];
    }

    /**
     * Chooses the default audio driver based on configuration options.
     */
    audiodriverid_t chooseAudioDriver()
    {
        CommandLine &cmdLine = App::commandLine();

        // No audio output?
        if(::isDedicated)
            return AUDIOD_DUMMY;
    
        if(cmdLine.has("-dummy"))
            return AUDIOD_DUMMY;

        if(cmdLine.has("-fmod"))
            return AUDIOD_FMOD;

        if(cmdLine.has("-oal") || cmdLine.has("-openal"))
            return AUDIOD_OPENAL;

#ifdef WIN32
        if(cmdLine.has("-dsound"))
            return AUDIOD_DSOUND;

        if(cmdLine.has("-winmm"))
            return AUDIOD_WINMM;
#endif

#ifndef DENG_DISABLE_SDLMIXER
        if(cmdLine.has("-sdlmixer"))
            return AUDIOD_SDL_MIXER;
#endif

        // The default audio driver.
        return AUDIOD_FMOD;
    }

    /**
     * Initializes the audio driver interfaces.
     *
     * @return  @c true iff successful.
     */
    bool initDriver(audiodriverid_t id)
    {
        try
        {
            AudioDriver &driver = driverById(id);

            switch(id)
            {
            case AUDIOD_DUMMY:      driver.load("dummy");       break;
#ifndef DENG_DISABLE_SDLMIXER
            case AUDIOD_SDL_MIXER:  driver.load("sdlmixer");    break;
#endif
            case AUDIOD_OPENAL:     driver.load("openal");      break;
            case AUDIOD_FMOD:       driver.load("fmod");        break;
            case AUDIOD_FLUIDSYNTH: driver.load("fluidsynth");  break;
#ifdef WIN32
            case AUDIOD_DSOUND:     driver.load("directsound"); break;
            case AUDIOD_WINMM:      driver.load("winmm");       break;
#endif

            default: return false;
            }

            // All loaded drivers are automatically initialized so they are ready for use.
            driver.initialize();
            return driver.isInitialized();
        }
        catch(AudioDriver::LoadError const &er)
        {
            LOG_AUDIO_WARNING("") << er.asText();
        }
        return false;
    }

    audiodriverid_t initDriverIfNeeded(String const &identifier)
    {
        audiodriverid_t id  = identifierToDriverId(identifier);
        AudioDriver &driver = driverById(id);
        if(!driver.isInitialized())
        {
            initDriver(id);
        }
        return id;
    }

    bool loadDrivers()
    {
        activeInterfaces.clear();

        if(App::commandLine().has("-nosound"))
            return false;

        audiodriverid_t defaultDriverId = chooseAudioDriver();

        bool ok = initDriver(defaultDriverId);
        if(!ok)
        {
            LOG_AUDIO_WARNING("Failed initializing audio driver \"%s\"")
                << AudioDriver_GetName(defaultDriverId);
        }

        // Fallback option for the default driver.
#ifndef DENG_DISABLE_SDLMIXER
        if(!ok)
        {
            defaultDriverId = AUDIOD_SDL_MIXER;
            ok = initDriver(defaultDriverId);
        }
#endif

        if(ok)
        {
            // Choose the interfaces to use.
            selectInterfaces(defaultDriverId);
        }
        return ok;
    }

    void unloadDrivers()
    {
        // Deinitialize all loaded drivers. (Note: reverse order)
        for(dint i = AUDIODRIVER_COUNT; i--> 0; )
        {
            drivers[i].deinitialize();
        }

        // Unload the plugins after everything has been shut down.
        for(AudioDriver &driver : drivers)
        {
            driver.unload();
        }

        // No more interfaces available.
        activeInterfaces.clear();
    }

    /**
     * The active/loaded interfaces.
     *
     * @todo The audio interface could also declare which audio formats it is capable
     * of playing (e.g., MIDI only, CD tracks only).
     */
    struct AudioInterface
    {
        audiointerfacetype_t type;
        union {
            void                   *any;
            audiointerface_sfx_t   *sfx;
            audiointerface_music_t *music;
            audiointerface_cd_t    *cd;
        } i;
    };
    QList<AudioInterface> activeInterfaces;

    /**
     * Choose the SFX, Music, and CD audio interfaces to use.
     *
     * @param defaultDriverId  Default audio driver to use unless overridden.
     */
    void selectInterfaces(audiodriverid_t defaultDriverId)
    {
        AudioDriver &defaultDriver = driverById(defaultDriverId);

        // The default driver goes on the bottom of the stack.
        if(defaultDriver.hasSfx())
        {
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_ISFX;
            ifs.i.any = &defaultDriver.iSfx();
            activeInterfaces << ifs;  // a copy is made
        }

        if(defaultDriver.hasMusic())
        {
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_IMUSIC;
            ifs.i.any = &defaultDriver.iMusic();
            activeInterfaces << ifs;  // a copy is made
        }
#ifdef MACOSX
        else if(defaultDriverId != AUDIOD_DUMMY)
        {
            // On the Mac, use the built-in QuickTime interface as the fallback for music.
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_IMUSIC;
            ifs.i.any = &::audiodQuickTimeMusic;
            activeInterfaces << ifs;  // a copy is made
        }
#endif

#ifndef WIN32
        // At the moment, dsFMOD supports streaming samples so we can
        // automatically load dsFluidSynth for MIDI music.
        if(defaultDriverId == AUDIOD_FMOD)
        {
            initDriverIfNeeded("fluidsynth");
            AudioDriver &fluidSynth = driverById(AUDIOD_FLUIDSYNTH);
            if(fluidSynth.isInitialized())
            {
                AudioInterface ifs; zap(ifs);
                ifs.type  = AUDIO_IMUSIC;
                ifs.i.any = &fluidSynth.iMusic();
                activeInterfaces << ifs;  // a copy is made
            }
        }
#endif

        if(defaultDriver.hasCd())
        {
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_ICD;
            ifs.i.any = &defaultDriver.iCd();
            activeInterfaces << ifs;  // a copy is made
        }

        CommandLine &cmdLine = App::commandLine();
        for(dint p = 1; p < cmdLine.count() - 1; ++p)
        {
            if(!cmdLine.isOption(p)) continue;

            // Check for SFX override.
            if(cmdLine.matches("-isfx", cmdLine.at(p)))
            {
                AudioDriver &driver = driverById(initDriverIfNeeded(cmdLine.at(++p)));
                if(!driver.hasSfx())
                    throw Error("selectInterfaces", "Audio driver '" + driver.name() + "' does not provide an SFX interface");

                AudioInterface ifs; zap(ifs);
                ifs.type  = AUDIO_ISFX;
                ifs.i.any = &driver.iSfx();
                activeInterfaces << ifs;  // a copy is made
                continue;
            }

            // Check for Music override.
            if(cmdLine.matches("-imusic", cmdLine.at(p)))
            {
                AudioDriver &driver = driverById(initDriverIfNeeded(cmdLine.at(++p)));
                if(!driver.hasMusic())
                    throw Error("selectInterfaces", "Audio driver '" + driver.name() + "' does not provide a Music interface");

                AudioInterface ifs; zap(ifs);
                ifs.type  = AUDIO_IMUSIC;
                ifs.i.any = &driver.iMusic();
                activeInterfaces << ifs;  // a copy is made
                continue;
            }

            // Check for CD override.
            if(cmdLine.matches("-icd", cmdLine.at(p)))
            {
                AudioDriver &driver = driverById(initDriverIfNeeded(cmdLine.at(++p)));
                if(!driver.hasCd())
                    throw Error("selectInterfaces", "Audio driver '" + driver.name() + "' does not provide a CD interface");

                AudioInterface ifs; zap(ifs);
                ifs.type  = AUDIO_ICD;
                ifs.i.any = &driver.iCd();
                activeInterfaces << ifs;  // a copy is made
                continue;
            }
        }

        // Let the music driver(s) know of the primary sfx interface, in case they
        // want to play audio through it.
        setMusicProperty(AUDIOP_SFX_INTERFACE, self.sfx());
    }

    /**
     * Iterate through the active interfaces of a given type, in descending priority
     * order: the most important interface is visited first.
     *
     * @param type  Type of interface to process.
     * @param func  Callback to make for each interface.
     */
    LoopResult forAllInterfaces(audiointerfacetype_t type, std::function<LoopResult (void *)> func) const
    {
        if(type != AUDIO_INONE)
        {
            for(dint i = activeInterfaces.count(); i--> 0; )
            {
                AudioInterface const &ifs = activeInterfaces[i];
                if(ifs.type == type ||
                   (type == AUDIO_IMUSIC_OR_ICD && (ifs.type == AUDIO_IMUSIC ||
                                                    ifs.type == AUDIO_ICD)))
                {
                    if(auto result = func(ifs.i.any))
                        return result;
                }
            }
        }
        return LoopContinue;
    }

    bool musAvail = false;              ///< @c true if at least one driver is initialized for music playback.
    bool musNeedBufFileSwitch = false;  ///< @c true= choose a new file name for the buffered playback file when asked. */
    String musCurrentSong;
    bool musPaused = false;

#endif  // __CLIENT__

    Instance(Public *i) : Base(i)
    {
        theAudioSystem = thisPublic;

        App::app().audienceForGameUnload() += this;
    }
    ~Instance()
    {
        App::app().audienceForGameUnload() -= this;

        theAudioSystem = nullptr;
    }

#ifdef __CLIENT__
    String composeMusicBufferFilename(String const &ext = "")
    {
        // Switch the name of the buffered song file?
        static dint currentBufFile = 0;
        if(musNeedBufFileSwitch)
        {
            currentBufFile ^= 1;
            musNeedBufFileSwitch = false;
        }

        // Compose the name.
        return BUFFERED_MUSIC_FILE + String::number(currentBufFile) + ext;
    }

    void setMusicProperty(dint prop, void const *ptr)
    {
        forAllInterfaces(AUDIO_IMUSIC, [this, &prop, &ptr] (void *ifs)
        {
            auto *iMusic = (audiointerface_music_t *) ifs;
            if(audiodriver_t *base = self.interface(iMusic))
            {
                if(base->Set) base->Set(prop, ptr);
            }
            return LoopContinue;
        });

        if(prop == AUDIOP_SOUNDFONT_FILENAME)
        {
            auto *fn = (char const *) ptr;
            if(!fn || !fn[0]) return; // No path.

            if(F_FileExists(fn))
            {
                LOG_AUDIO_MSG("Current soundfont set to: \"%s\"") << fn;
            }
            else
            {
                LOG_AUDIO_WARNING("Soundfont \"%s\" not found") << fn;
            }
        }
    }

    dint playMusicFile(String const &virtualOrNativePath, bool looped = false)
    {
        DENG2_ASSERT(musAvail);
        LOG_AS("audio::System");

        if(virtualOrNativePath.isEmpty())
            return 0;

        // Relative paths are relative to the native working directory.
        String const path  = (NativePath::workPath() / NativePath(virtualOrNativePath).expand()).withSeparators('/');
        LOG_AUDIO_VERBOSE("Attempting to play music file \"%s\"")
            << NativePath(virtualOrNativePath).pretty();

        try
        {
            std::unique_ptr<FileHandle> hndl(&App_FileSystem().openFile(path, "rb"));

            auto didPlay = forAllInterfaces(AUDIO_IMUSIC, [this, &hndl, &looped] (void *ifs)
            {
                auto *iMusic = (audiointerface_music_t *) ifs;

                // Does this interface offer buffered playback?
                if(iMusic->Play && iMusic->SongBuffer)
                {
                    // Buffer the data using the driver's own facility.
                    dsize const len = hndl->length();
                    hndl->read((duint8 *) iMusic->SongBuffer(len), len);

                    return iMusic->Play(looped);
                }
                // Does this interface offer playback from a native file?
                else if(iMusic->PlayFile)
                {
                    // Write the data to disk and play from there.
                    String const bufPath = composeMusicBufferFilename();

                    dsize len = hndl->length();
                    duint8 *buf = (duint8 *)M_Malloc(len);
                    hndl->read(buf, len);
                    F_Dump(buf, len, bufPath.toUtf8().constData());
                    M_Free(buf); buf = nullptr;

                    return iMusic->PlayFile(bufPath.toUtf8().constData(), looped);
                }
                return 0;  // Continue.
            });

            App_FileSystem().releaseFile(hndl->file());
            return didPlay;
        }
        catch(FS1::NotFoundError const &)
        {}  // Ignore this error.
        return 0;  // Continue.
    }

    /**
     * @return  @c  1= if music was started. 0, if attempted to start but failed.
     *          @c -1= if it was MUS data and @a canPlayMUS says we can't play it.
     */
    dint playMusicLump(lumpnum_t lumpNum, bool looped = false, bool canPlayMUS = true)
    {
        DENG2_ASSERT(musAvail);
        LOG_AS("audio::System");

        if(!App_FileSystem().nameIndex().hasLump(lumpNum))
            return 0;

        File1 &lump = App_FileSystem().lump(lumpNum);
        if(recognizeMus(lump))
        {
            // Lump is in DOOM's MUS format. We must first convert it to MIDI.
            if(!canPlayMUS) return -1;

            String const srcFile = composeMusicBufferFilename(".mid");

            // Read the lump, convert to MIDI and output to a temp file in the working directory.
            // Use a filename with the .mid extension so that any player which relies on the it
            // for format recognition works as expected.
            duint8 *buf = (duint8 *) M_Malloc(lump.size());
            lump.read(buf, 0, lump.size());
            M_Mus2Midi((void *)buf, lump.size(), srcFile.toUtf8().constData());
            M_Free(buf); buf = nullptr;

            return forAllInterfaces(AUDIO_IMUSIC, [&srcFile, &looped] (void *ifs)
            {
                auto *iMusic = (audiointerface_music_t *) ifs;
                if(iMusic->PlayFile)
                {
                    return iMusic->PlayFile(srcFile.toUtf8().constData(), looped);
                }
                return 0;  // Continue.
            });
        }

        return forAllInterfaces(AUDIO_IMUSIC, [this, &lump, &looped] (void *ifs)
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
                String const fileName = composeMusicBufferFilename();
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

    dint playMusicCDTrack(dint track, bool looped)
    {
        LOG_AS("audio::System");

        // Assume track 0 is not valid.
        if(track == 0) return 0;

        return forAllInterfaces(AUDIO_ICD, [&track, &looped] (void *ifs)
        {
            auto *iCD = (audiointerface_cd_t *) ifs;
            if(iCD->Play)
            {
                return iCD->Play(track, looped);
            }
            return 0;  // Continue.
        });
    }

    /**
     * Perform initialization for music playback.
     */
    void initMusic()
    {
        // Already been here?
        if(musAvail) return;

        LOG_AUDIO_VERBOSE("Initializing Music subsystem...");

        musAvail       = false;
        musCurrentSong = "";
        musPaused      = false;

        CommandLine &cmdLine = App::commandLine();
        if(::isDedicated || cmdLine.has("-nomusic"))
        {
            LOG_AUDIO_NOTE("Music disabled");
            return;
        }

        // Initialize interfaces for music playback.
        dint initialized = 0;
        forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [this, &initialized] (void *ifs)
        {
            auto *iMusic = (audiointerface_music_generic_t *) ifs;
            if(iMusic->Init())
            {
                initialized += 1;
            }
            else
            {
                LOG_AUDIO_WARNING("Failed to initialize \"%s\" for music playback")
                    << self.interfaceName(iMusic);
            }
            return LoopContinue;
        });

        // Remember whether an interface for music playback initialized successfully.
        musAvail = initialized >= 1;
        if(musAvail)
        {
            // Tell audio drivers about our soundfont config.
            self.updateMusicSoundFont();
        }
    }

    /**
     * Perform deinitialize for music playback.
     */
    void deinitMusic()
    {
        // Already been here?
        if(!musAvail) return;
        musAvail = false;

        // Shutdown interfaces.
        forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
        {
            auto *iMusic = (audiointerface_music_generic_t *) ifs;
            if(iMusic->Shutdown) iMusic->Shutdown();
            return LoopContinue;
        });
    }
#endif

    void aboutToUnloadGame(game::Game const &)
    {
        self.reset();
    }
};

System::System() : d(new Instance(this))
{}

audio::System &System::get()
{
    DENG2_ASSERT(theAudioSystem);
    return *theAudioSystem;
}

void System::timeChanged(Clock const &)
{
    // Nothing to do.
}

String System::description() const
{
#define TABBED(A, B)  _E(Ta) "  " _E(l) A _E(.) " " _E(Tb) << B << "\n"

    String str;
    QTextStream os(&str);

    os << _E(b) "Audio configuration:\n" _E(.);

    os << TABBED("Music volume:",  musVolume);
#ifdef __CLIENT__
    os << TABBED("Music sound font:", musSoundFontPath);
    os << TABBED("Music source preference:", musicSourceAsText(musSourcePreference));

    // Include an active playback interface itemization.
    for(dint i = d->activeInterfaces.count(); i--> 0; )
    {
        Instance::AudioInterface const &ifs = d->activeInterfaces[i];

        if(ifs.type == AUDIO_IMUSIC || ifs.type == AUDIO_ICD)
        {
            os << _E(Ta) _E(l) "  " << (ifs.type == AUDIO_IMUSIC ? "Music" : "CD") << ": "
               << _E(.) _E(Tb) << interfaceName(ifs.i.any) << "\n";
        }
        else if(ifs.type == AUDIO_ISFX)
        {
            os << _E(Ta) _E(l) << "  SFX: " << _E(.) _E(Tb)
               << interfaceName(ifs.i.sfx) << "\n";
        }
    }
#endif

    return str.rightStrip();

#undef TABBED
}

void System::reset()
{
#ifdef __CLIENT__
    Sfx_Reset();
#endif
    _api_S.StopMusic();
}

/**
 * @todo Do this in audio::System::timeChanged()
 */
void System::startFrame()
{
#ifdef __CLIENT__
    static dint oldMusVolume = -1;

    if(::musVolume != oldMusVolume)
    {
        oldMusVolume = ::musVolume;
        setMusicVolume(::musVolume / 255.0f);
    }

    // Update all channels (freq, 2D:pan,volume, 3D:position,velocity).
    Sfx_StartFrame();

    if(d->musAvail)
    {
        // Update all interfaces.
        d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
        {
            auto *iMusic = (audiointerface_music_generic_t *) ifs;
            iMusic->Update();
            return LoopContinue;
        });
    }
#endif

    // Remove stopped sounds from the LSM.
    Sfx_Logical_SetOneSoundPerEmitter(::sfxOneSoundPerEmitter);
    Sfx_PurgeLogical();
}

void System::endFrame()
{
#ifdef __CLIENT__
    Sfx_EndFrame();
#endif
}

void System::initPlayback()
{
    LOG_AS("audio::System");

    Sfx_Logical_SetSampleLengthCallback(Sfx_GetSoundLength);

    CommandLine &cmdLine = App::commandLine();
    if(cmdLine.has("-nosound") || cmdLine.has("-noaudio"))
        return;

    // Disable random pitch changes?
    ::noRndPitch = cmdLine.has("-norndpitch");

#ifdef __CLIENT__
    LOG_AUDIO_VERBOSE("Initializing for playback...");

    // Try to load the audio driver plugin(s).
    if(d->loadDrivers())
    {
        if(!Sfx_Init())
        {
            LOG_AUDIO_NOTE("Errors during audio subsystem initialization");
        }

        d->initMusic();
    }
    else
    {
        LOG_AUDIO_NOTE("Music and sound effects are disabled");
    }

    // Print a summary of the active configuration to the log.
    LOG_AUDIO_MSG("%s") << description();
#endif
}

void System::deinitPlayback()
{
#ifdef __CLIENT__
    Sfx_Shutdown();
    d->deinitMusic();

    d->unloadDrivers();
#endif
}

#ifdef __CLIENT__
String System::musicSourceAsText(MusicSource source)  // static 
{
    static char const *sourceNames[3] = {
        /* MUSP_MUS */ "MUS lumps",
        /* MUSP_EXT */ "External files",
        /* MUSP_CD */  "CD",
    };
    if(source >= MUSP_MUS && source <= MUSP_CD)
        return sourceNames[dint( source )];
    return "(invalid)";
}

bool System::musicIsAvailable() const
{
    return d->musAvail;
}

void System::setMusicVolume(dfloat newVolume)
{
    if(!d->musAvail) return;

    // Set volume of all available interfaces.
    d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [&newVolume] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        iMusic->Set(MUSIP_VOLUME, newVolume);
        return LoopContinue;
    });
}

bool System::musicIsPlaying() const
{
    return d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_t *) ifs;
        return iMusic->gen.Get(MUSIP_PLAYING, nullptr);
    });
}

void System::stopMusic()
{
    if(!d->musAvail) return;

    d->musCurrentSong = "";

    // Stop all interfaces.
    d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        iMusic->Stop();
        return LoopContinue;
    });
}

void System::pauseMusic(bool doPause)
{
    if(!d->musAvail) return;

    d->musPaused = !d->musPaused;

    // Pause playback on all interfaces.
    d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [&doPause] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        iMusic->Pause(doPause);
        return LoopContinue;
    });
}

bool System::musicIsPaused() const
{
    return d->musPaused;
}

dint System::playMusic(Record const &definition, bool looped)
{
    if(!d->musAvail) return false;

    LOG_AS("audio::System::playMusic");
    LOG_AUDIO_VERBOSE("Starting ID:%s looped:%b, currentSong ID:%s")
        << definition.gets("id") << looped << d->musCurrentSong;

    // We will not restart the currently playing song.
    if(definition.gets("id") == d->musCurrentSong && musicIsPlaying())
        return false;

    // Stop the currently playing song.
    stopMusic();

    // Switch to an unused file buffer if asked.
    d->musNeedBufFileSwitch = true;

    // This is the song we're playing now.
    d->musCurrentSong = definition.gets("id");

    // Determine the music source, order preferences.
    dint source[3];
    source[0] = musSourcePreference;
    switch(musSourcePreference)
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
            if(cd())
            {
                if(d->playMusicCDTrack(defn::Music(definition).cdTrack(), looped))
                    return true;
            }
            break;

        case MUSP_EXT:
            if(d->playMusicFile(tryFindMusicFile(definition), looped))
                return true;

            // Next, try non-MUS lumps.
            canPlayMUS = false;

            // Note: Intentionally falls through to MUSP_MUS.

        case MUSP_MUS:
            if(d->playMusicLump(App_FileSystem().lumpNumForName(definition.gets("lumpName")),
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

dint System::playMusicLump(lumpnum_t lumpNum, bool looped)
{
    stopMusic();
    return d->playMusicLump(lumpNum, looped);
}

dint System::playMusicFile(String const &filePath, bool looped)
{
    stopMusic();
    return d->playMusicFile(filePath, looped);
}

dint System::playMusicCDTrack(dint cdTrack, bool looped)
{
    stopMusic();
    return d->playMusicCDTrack(cdTrack, looped);
}

void System::updateMusicSoundFont()
{
    NativePath path(musSoundFontPath);

#ifdef MACOSX
    // On OS X we can try to use the basic DLS soundfont that's part of CoreAudio.
    if(path.isEmpty())
    {
        path = "/System/Library/Components/CoreAudio.component/Contents/Resources/gs_instruments.dls";
    }
#endif

    d->setMusicProperty(AUDIOP_SOUNDFONT_FILENAME, path.expand().toString().toLatin1().constData());
}

audiointerface_sfx_generic_t *System::sfx() const
{
    // The primary interface is the first one.
    audiointerface_sfx_generic_t *found = nullptr;
    d->forAllInterfaces(AUDIO_ISFX, [&found] (void *ifs)
    {
        found = (audiointerface_sfx_generic_t *)ifs;
        return LoopAbort;
    });
    return found;
}

audiointerface_cd_t *System::cd() const
{
    // The primary interface is the first one.
    audiointerface_cd_t *found = nullptr;
    d->forAllInterfaces(AUDIO_ICD, [&found] (void *ifs)
    {
        found = (audiointerface_cd_t *)ifs;
        return LoopAbort;
    });
    return found;
}

audiodriver_t *System::interface(void *anyAudioInterface) const
{
    if(anyAudioInterface)
    {
        for(AudioDriver const &driver : d->drivers)
        {
            if((void *)&driver.iSfx()   == anyAudioInterface ||
               (void *)&driver.iMusic() == anyAudioInterface ||
               (void *)&driver.iCd()    == anyAudioInterface)
            {
                return &driver.iBase();
            }
        }
    }
    return nullptr;
}

audiointerfacetype_t System::interfaceType(void *anyAudioInterface) const
{
    if(anyAudioInterface)
    {
        for(AudioDriver const &driver : d->drivers)
        {
            if((void *)&driver.iSfx()   == anyAudioInterface) return AUDIO_ISFX;
            if((void *)&driver.iMusic() == anyAudioInterface) return AUDIO_IMUSIC;
            if((void *)&driver.iCd()    == anyAudioInterface) return AUDIO_ICD;
        }
    }
    return AUDIO_INONE;
}

String System::interfaceName(void *anyAudioInterface) const
{
    if(anyAudioInterface)
    {
        for(AudioDriver const &driver : d->drivers)
        {
            String const name = driver.interfaceName(anyAudioInterface);
            if(!name.isEmpty()) return name;
        }
    }
    return "(invalid)";
}

audiodriverid_t System::toDriverId(AudioDriver const *driver) const
{
    if(driver && driver >= &d->drivers[0] && driver <= &d->drivers[AUDIODRIVER_COUNT])
    {
        return audiodriverid_t( driver - d->drivers );
    }
    return AUDIOD_INVALID;
}
#endif

void System::aboutToUnloadMap()
{
    // Stop everything in the LSM.    
    Sfx_InitLogical();

#ifdef __CLIENT__
    Sfx_MapChange();
#endif
}

void System::worldMapChanged()
{
#ifdef __CLIENT__
    // Update who is listening now.
    Sfx_SetListener(S_GetListenerMobj());
#endif
}

/**
 * Console command for playing a (local) sound effect.
 */
D_CMD(PlaySound)
{
    DENG2_UNUSED(src);

    if(argc < 2)
    {
        LOG_SCR_NOTE("Usage: %s (id) (volume) at (x) (y) (z)") << argv[0];
        LOG_SCR_MSG("(volume) must be in 0..1, but may be omitted");
        LOG_SCR_MSG("'at (x) (y) (z)' may also be omitted");
        LOG_SCR_MSG("The sound is always played locally");
        return true;
    }
    dint p = 0;

    // The sound ID is always first.
    dint const id = ::defs.getSoundNum(argv[1]);

    // The second argument may be a volume.
    dfloat volume = 1;
    if(argc >= 3 && String(argv[2]).compareWithoutCase("at"))
    {
        volume = String(argv[2]).toFloat();
        p = 3;
    }
    else
    {
        p = 2;
    }

    bool useFixedPos = false;
    coord_t fixedPos[3];
    if(argc >= p + 4 && !String(argv[p]).compareWithoutCase("at"))
    {
        useFixedPos = true;
        fixedPos[0] = String(argv[p + 1]).toDouble();
        fixedPos[1] = String(argv[p + 2]).toDouble();
        fixedPos[2] = String(argv[p + 3]).toDouble();
    }

    // Check that the volume is valid.
    volume = de::clamp(0.f, volume, 1.f);
    if(de::fequal(volume, 0)) return true;

    if(useFixedPos)
    {
        _api_S.LocalSoundAtVolumeFrom(id, nullptr, fixedPos, volume);
    }
    else
    {
        _api_S.LocalSoundAtVolume(id, nullptr, volume);
    }

    return true;
}

#ifdef __CLIENT__

/**
 * CCmd: Play a music track.
 */
D_CMD(PlayMusic)
{
    DENG2_UNUSED(src);

    LOG_AS("playmusic (Cmd)");

    if(!App_AudioSystem().musicIsAvailable())
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
            return Mus_StartLump(App_FileSystem().lumpNumForName(argv[2]), looped);
        }
        else if(!qstricmp(argv[1], "file"))
        {
            return Mus_StartFile(argv[2], looped);
        }
        else if(!qstricmp(argv[1], "cd"))
        {
            if(!App_AudioSystem().cd())
            {
                LOG_AUDIO_WARNING("No CD audio interface available");
                return false;
            }
            return Mus_StartCDTrack(String(argv[2]).toInt(), looped);
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

    App_AudioSystem().stopMusic();
    return true;
}

D_CMD(PauseMusic)
{
    DENG2_UNUSED3(src, argc, argv);

    App_AudioSystem().pauseMusic(!App_AudioSystem().musicIsPaused());
    return true;
}

static void reverbVolumeChanged()
{
    Sfx_UpdateReverb();
}

static void musicSoundFontChanged()
{
    App_AudioSystem().updateMusicSoundFont();
}
#endif

void System::consoleRegister()  // static
{
    C_VAR_BYTE  ("sound-overlap-stop",  &sfxOneSoundPerEmitter, 0, 0, 1);

#ifdef __CLIENT__
    // Sound effects:
    C_VAR_INT     ("sound-volume",        &sfxVolume,             0, 0, 255);
    C_VAR_INT     ("sound-rate",          &sfxSampleRate,         0, 11025, 44100);
    C_VAR_INT     ("sound-16bit",         &sfx16Bit,              0, 0, 1);
    C_VAR_INT     ("sound-3d",            &sfx3D,                 0, 0, 1);
    C_VAR_FLOAT2  ("sound-reverb-volume", &sfxReverbStrength,     0, 0, 1.5f, reverbVolumeChanged);

    C_CMD_FLAGS("playsound",  nullptr, PlaySound,  CMDF_NO_DEDICATED);

    // Music:
    C_VAR_CHARPTR2("music-soundfont",     &musSoundFontPath,      0, 0, 0, musicSoundFontChanged);
    C_VAR_INT     ("music-source",        &musSourcePreference,   0, 0, 2);
    C_VAR_INT     ("music-volume",        &musVolume,             0, 0, 255);

    C_CMD_FLAGS("playmusic",  nullptr, PlayMusic,  CMDF_NO_DEDICATED);
    C_CMD_FLAGS("pausemusic", nullptr, PauseMusic, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("stopmusic",  "",      StopMusic,  CMDF_NO_DEDICATED);

    // Debug:
    C_VAR_INT     ("sound-info",          &showSoundInfo,         0, 0, 1);
#endif
}

}  // namespace audio

// Sound Effects: ---------------------------------------------------------------

mobj_t *S_GetListenerMobj()
{
    return DD_Player(::displayPlayer)->publicData().mo;
}

sfxinfo_t *S_GetSoundInfo(dint soundID, dfloat *freq, dfloat *volume)
{
    if(soundID <= 0 || soundID >= ::defs.sounds.size())
        return nullptr;

    dfloat dummy = 0;
    if(!freq)   freq   = &dummy;
    if(!volume) volume = &dummy;

    // Traverse all links when getting the definition. (But only up to 10, which is
    // certainly enough and prevents endless recursion.) Update the sound id at the
    // same time. The links were checked in Def_Read() so there cannot be any bogus
    // ones.
    sfxinfo_t *info = &::runtimeDefs.sounds[soundID];

    for(dint i = 0; info->link && i < 10;
        info     = info->link,
        *freq    = (info->linkPitch > 0    ? info->linkPitch  / 128.0f : *freq),
        *volume += (info->linkVolume != -1 ? info->linkVolume / 127.0f : 0),
        soundID  = ::runtimeDefs.sounds.indexOf(info),
       ++i)
    {}

    DENG2_ASSERT(soundID < ::defs.sounds.size());

    return info;
}

dd_bool S_IsRepeating(dint idFlags)
{
    if(idFlags & DDSF_REPEAT)
        return true;

    if(sfxinfo_t *info = S_GetSoundInfo(idFlags & ~DDSF_FLAG_MASK, nullptr, nullptr))
    {
        return (info->flags & SF_REPEAT) != 0;
    }
    return false;
}

dint S_LocalSoundAtVolumeFrom(dint soundIdAndFlags, mobj_t *origin, coord_t *point, dfloat volume)
{
#ifdef __CLIENT__
    // A dedicated server never starts any local sounds (only logical sounds in the LSM).
    if(::isDedicated || DoomsdayApp::app().busyMode().isActive())
        return false;

    dint soundId = (soundIdAndFlags & ~DDSF_FLAG_MASK);
    if(soundId <= 0 || soundId >= ::defs.sounds.size())
        return false;

    if(::sfxVolume <= 0 || volume <= 0)
        return false; // This won't play...

    LOG_AS("S_LocalSoundAtVolumeFrom");

    if(volume > 1)
    {
        LOGDEV_AUDIO_WARNING("Volume is too high (%f > 1)") << volume;
    }

    dfloat freq = 1;
    // This is the sound we're going to play.
    sfxinfo_t *info = S_GetSoundInfo(soundId, &freq, &volume);
    if(!info) return false;  // Hmm? This ID is not defined.

    bool const isRepeating = S_IsRepeating(soundIdAndFlags);

    // Check the distance (if applicable).
    if(!(info->flags & SF_NO_ATTENUATION) && !(soundIdAndFlags & DDSF_NO_ATTENUATION))
    {
        // If origin is too far, don't even think about playing the sound.
        coord_t *fixPoint = (origin ? origin->origin : point);

        if(Mobj_ApproxPointDistance(S_GetListenerMobj(), fixPoint) > soundMaxDist)
            return false;
    }

    // Load the sample.
    sfxsample_t *sample = Sfx_Cache(soundId);
    if(!sample)
    {
        if(sfxAvail)
        {
            LOG_AUDIO_VERBOSE("S_LocalSoundAtVolumeFrom: Caching of sound %i failed")
                    << soundId;
        }
        return false;
    }

    // Random frequency alteration? (Multipliers chosen to match original
    // sound code.)
    if(!::noRndPitch)
    {
        if(info->flags & SF_RANDOM_SHIFT)
        {
            freq += (RNG_RandFloat() - RNG_RandFloat()) * (7.0f / 255);
        }
        if(info->flags & SF_RANDOM_SHIFT2)
        {
            freq += (RNG_RandFloat() - RNG_RandFloat()) * (15.0f / 255);
        }
    }

    // If the sound has an exclusion group, either all or the same emitter's
    // iterations of this sound will stop.
    if(info->group)
    {
        mobj_t *emitter = ((info->flags & SF_GLOBAL_EXCLUDE) ? nullptr : origin);
        Sfx_StopSoundGroup(info->group, emitter);
    }

    // Let's play it.
    dint flags = 0;
    flags |= (((info->flags & SF_NO_ATTENUATION) || (soundIdAndFlags & DDSF_NO_ATTENUATION)) ? SF_NO_ATTENUATION : 0);
    flags |= (isRepeating ? SF_REPEAT : 0);
    flags |= ((info->flags & SF_DONT_STOP) ? SF_DONT_STOP : 0);
    return Sfx_StartSound(sample, volume, freq, origin, point, flags);

#else
    DENG2_UNUSED4(soundIdAndFlags, origin, point, volume);
    return false;
#endif
}

dint S_LocalSoundAtVolume(dint soundID, mobj_t *origin, dfloat volume)
{
    return S_LocalSoundAtVolumeFrom(soundID, origin, nullptr, volume);
}

dint S_LocalSound(dint soundID, mobj_t *origin)
{
    // Play local sound at max volume.
    return S_LocalSoundAtVolumeFrom(soundID, origin, nullptr, 1);
}

dint S_LocalSoundFrom(dint soundID, coord_t *fixedPos)
{
    return S_LocalSoundAtVolumeFrom(soundID, nullptr, fixedPos, 1);
}

dint S_StartSound(dint soundID, mobj_t *origin)
{
#ifdef __SERVER__
    // The sound is audible to everybody.
    Sv_Sound(soundID, origin, SVSF_TO_ALL);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    return S_LocalSound(soundID, origin);
}

dint S_StartSoundEx(dint soundID, mobj_t *origin)
{
#ifdef __SERVER__
    Sv_Sound(soundID, origin, SVSF_TO_ALL | SVSF_EXCLUDE_ORIGIN);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    return S_LocalSound(soundID, origin);
}

dint S_StartSoundAtVolume(dint soundID, mobj_t *origin, dfloat volume)
{
#ifdef __SERVER__
    Sv_SoundAtVolume(soundID, origin, volume, SVSF_TO_ALL);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    // The sound is audible to everybody.
    return S_LocalSoundAtVolume(soundID, origin, volume);
}

dint S_ConsoleSound(dint soundID, mobj_t *origin, dint targetConsole)
{
#ifdef __SERVER__
    Sv_Sound(soundID, origin, targetConsole);
#endif

    // If it's for us, we can hear it.
    if(targetConsole == consolePlayer)
    {
        S_LocalSound(soundID, origin);
    }

    return true;
}

/**
 * @param sectorEmitter  Sector in which to stop sounds.
 * @param soundID        Unique identifier of the sound to be stopped.
 *                       If @c 0, ID not checked.
 * @param flags          @ref soundStopFlags
 */
static void stopSectorSounds(ddmobj_base_t *sectorEmitter, dint soundID, dint flags)
{
    if(!sectorEmitter || !flags) return;

    // Are we stopping with this sector's emitter?
    if(flags & SSF_SECTOR)
    {
        _api_S.StopSound(soundID, (mobj_t *)sectorEmitter);
    }

    // Are we stopping with linked emitters?
    if(!(flags & SSF_SECTOR_LINKED_SURFACES)) return;

    // Process the rest of the emitter chain.
    ddmobj_base_t *base = sectorEmitter;
    while((base = (ddmobj_base_t *)base->thinker.next))
    {
        // Stop sounds from this emitter.
        _api_S.StopSound(soundID, (mobj_t *)base);
    }
}

void S_StopSound(dint soundID, mobj_t *emitter)
{
#ifdef __CLIENT__
    // No special stop behavior.
    // Sfx provides a routine for this.
    Sfx_StopSound(soundID, emitter);
#endif

    // Notify the LSM.
    if(Sfx_StopLogical(soundID, emitter))
    {
#ifdef __SERVER__
        // In netgames, the server is responsible for telling clients
        // when to stop sounds. The LSM will tell us if a sound was
        // stopped somewhere in the world.
        Sv_StopSound(soundID, emitter);
#endif
    }
}

void S_StopSound2(dint soundID, mobj_t *emitter, dint flags)
{
    // Are we performing any special stop behaviors?
    if(emitter && flags)
    {
        if(emitter->thinker.id)
        {
            // Emitter is a real Mobj.
            stopSectorSounds(&Mobj_Sector(emitter)->soundEmitter(), soundID, flags);
            return;
        }

        // The head of the chain is the sector. Find it.
        while(emitter->thinker.prev)
        {
            emitter = (mobj_t *)emitter->thinker.prev;
        }
        stopSectorSounds((ddmobj_base_t *)emitter, soundID, flags);
        return;
    }

    // A regular stop.
    S_StopSound(soundID, emitter);
}

dint S_IsPlaying(dint soundID, mobj_t *emitter)
{
    // The Logical Sound Manager (under Sfx) provides a routine for this.
    return Sfx_IsPlaying(soundID, emitter);
}

// Music: -----------------------------------------------------------------------

void Mus_SetVolume(dfloat newVolume)
{
#ifdef __CLIENT__
    App_AudioSystem().setMusicVolume(newVolume);
#endif
}

bool Mus_IsPlaying()
{
#ifdef __CLIENT__
    return App_AudioSystem().musicIsPlaying();
#else
    return false;
#endif
}

void S_StopMusic()
{
#ifdef __CLIENT__
    App_AudioSystem().stopMusic();
#endif
}

void S_PauseMusic(dd_bool paused)
{
#ifdef __CLIENT__
    App_AudioSystem().pauseMusic(paused);
#else
    DENG2_UNUSED(paused);
#endif
}

dint Mus_Start(Record const &definition, bool looped)
{
#ifdef __CLIENT__
    return App_AudioSystem().playMusic(definition, looped);
#else
    DENG2_UNUSED2(definition, looped);
    return 0;
#endif
}

dint Mus_StartLump(lumpnum_t lumpNum, bool looped)
{
#ifdef __CLIENT__
    return App_AudioSystem().playMusicLump(lumpNum, looped);
#else
    DENG2_UNUSED2(lumpNum, looped);
    return 0;
#endif
}

dint Mus_StartFile(char const *filePath, bool looped)
{
#ifdef __CLIENT__
    return App_AudioSystem().playMusicFile(filePath, looped);
#else
    DENG2_UNUSED2(filePath, looped);
    return 0;
#endif
}

dint Mus_StartCDTrack(dint cdTrack, bool looped)
{
#ifdef __CLIENT__
    return App_AudioSystem().playMusicCDTrack(cdTrack, looped);
#else
    DENG2_UNUSED2(cdTrack, looped);
    return 0;
#endif
}

dint S_StartMusicNum(dint id, dd_bool looped)
{
#ifdef __CLIENT__

    // Don't play music if the volume is at zero.
    if(::isDedicated) return true;

    if(id < 0 || id >= ::defs.musics.size()) return false;

    Record const &def = ::defs.musics[id];
    LOG_AUDIO_MSG("Starting music '%s'") << def.gets("id");

    return Mus_Start(def, looped);

#else
    DENG2_UNUSED2(id, looped);
    return false;
#endif
}

dint S_StartMusic(char const *musicID, dd_bool looped)
{
    LOG_AS("S_StartMusic");
    dint idx = ::defs.getMusicNum(musicID);
    if(idx < 0)
    {
        if(musicID && !String(musicID).isEmpty())
        {
            LOG_AUDIO_WARNING("Song \"%s\" not defined, cannot start playback") << musicID;
        }
        return false;
    }
    return S_StartMusicNum(idx, looped);
}

DENG_DECLARE_API(S) =
{
    { DE_API_SOUND },
    //S_MapChange,
    S_LocalSoundAtVolumeFrom,
    S_LocalSoundAtVolume,
    S_LocalSound,
    S_LocalSoundFrom,
    S_StartSound,
    S_StartSoundEx,
    S_StartSoundAtVolume,
    S_ConsoleSound,
    S_StopSound,
    S_StopSound2,
    S_IsPlaying,
    S_StartMusic,
    S_StartMusicNum,
    S_StopMusic,
    S_PauseMusic
};
