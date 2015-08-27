/** @file system.cpp  Audio subsystem module.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au> *
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

#include "dd_share.h"      // SF_* flags
#include "dd_main.h"       // ::isDedicated
#include "def_main.h"      // ::defs

#ifdef __SERVER__
#  include "server/sv_sound.h"
#endif

#include "api_audiod_sfx.h"
#include "audio/samplecache.h"
#ifdef __CLIENT__
#  include "audio/channel.h"
#  include "audio/mus.h"
#endif

#include "api_map.h"
#include "world/p_players.h"
#include "world/thinkers.h"
#include "Sector"
#include "SectorCluster"

#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#ifdef __CLIENT__
#  include <doomsday/defs/music.h>
#  include <doomsday/filesys/fs_main.h>
#  include <doomsday/filesys/fs_util.h>
#endif
#include <de/App>
#include <de/timer.h>
#ifdef __CLIENT__
#  include <de/memory.h>
#endif
#include <QMultiHash>
#include <QtAlgorithms>

using namespace de;

#ifdef __CLIENT__
#  ifdef MACOSX
/// Built-in QuickTime audio interface implemented by MusicPlayer.m
DENG_EXTERN_C audiointerface_music_t audiodQuickTimeMusic;
#  endif
#endif

dint sfxBits = 8;
dint sfxRate = 11025;

namespace audio {

static audio::System *theAudioSystem;

static duint const PURGEINTERVAL = 2000;  ///< 2 seconds
#ifdef __CLIENT__
static dint const CHANNEL_COUNT_DEFAULT  = 16;
static dint const CHANNEL_COUNT_MAX      = 256;
static dint const CHANNEL_2DCOUNT        = 4;
static String const MUSIC_BUFFEREDFILE   ("dd-buffered-song");

/// @todo should be a cvars:
static bool sfxNoRndPitch;
#endif  // __CLIENT__
static dint sfxDistMin = 256;  ///< No distance attenuation this close.
static dint sfxDistMax = 2025;

#ifdef __CLIENT__
// Console variables:
static dint sfxVolume = 255 * 2/3;
static dint sfx16Bit;
static dint sfxSampleRate = 11025;
static dint sfx3D;
#endif  // __CLIENT__
static byte sfxOneSoundPerEmitter;  //< @c false= Traditional Doomsday behavior: allow sounds to overlap.
#ifdef __CLIENT__
static dfloat sfxReverbStrength = 0.5f;

static dint musVolume = 255 * 2/3;
static char *musMidiFontPath = (char *) "";
// When multiple sources are available this setting determines which to use (mus < ext < cd).
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
 * Usually the display player.
 */
static mobj_t *getListenerMobj()
{
    return DD_Player(::displayPlayer)->publicData().mo;
}
#endif

DENG2_PIMPL(System)
#ifdef __CLIENT__
, DENG2_OBSERVES(App, GameUnload)
#endif
{
#ifdef __CLIENT__
    Driver drivers[AUDIODRIVER_COUNT];

    Driver &driverById(audiodriverid_t id)
    {
        DENG2_ASSERT(VALID_AUDIODRIVER_IDENTIFIER(id));
        return drivers[id];
    }

    /**
     * Chooses the default audio driver based on configuration options.
     */
    audiodriverid_t chooseDriver()
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
    bool initDriver(audiodriverid_t driverId)
    {
        try
        {
            Driver &driver = driverById(driverId);

            switch(driverId)
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
        catch(Driver::LoadError const &er)
        {
            LOG_AUDIO_WARNING("Failed initializing driver \"%s\":\n")
                << Driver_GetName(driverId)
                << er.asText();
        }
        return false;
    }

    audiodriverid_t initDriverIfNeeded(String const &identifier)
    {
        audiodriverid_t id  = identifierToDriverId(identifier);
        Driver &driver = driverById(id);
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

        audiodriverid_t defaultDriverId = chooseDriver();

        bool ok = initDriver(defaultDriverId);

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
        for(Driver &driver : drivers)
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
    struct PlaybackInterface
    {
        audiointerfacetype_t type;
        union {
            void                   *any;
            audiointerface_sfx_t   *sfx;
            audiointerface_music_t *music;
            audiointerface_cd_t    *cd;
        } i;
    };
    QList<PlaybackInterface> activeInterfaces;

    /**
     * Choose the SFX, Music, and CD audio interfaces to use.
     *
     * @param defaultDriverId  Default audio driver to use unless overridden.
     */
    void selectInterfaces(audiodriverid_t defaultDriverId)
    {
        Driver &defaultDriver = driverById(defaultDriverId);

        // The default driver goes on the bottom of the stack.
        if(defaultDriver.hasSfx())
        {
            PlaybackInterface ifs; zap(ifs);
            ifs.type  = AUDIO_ISFX;
            ifs.i.any = &defaultDriver.iSfx();
            activeInterfaces << ifs;  // a copy is made
        }

        if(defaultDriver.hasMusic())
        {
            PlaybackInterface ifs; zap(ifs);
            ifs.type  = AUDIO_IMUSIC;
            ifs.i.any = &defaultDriver.iMusic();
            activeInterfaces << ifs;  // a copy is made
        }
#ifdef MACOSX
        else if(defaultDriverId != AUDIOD_DUMMY)
        {
            // On the Mac, use the built-in QuickTime interface as the fallback for music.
            PlaybackInterface ifs; zap(ifs);
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
            Driver &fluidSynth = driverById(AUDIOD_FLUIDSYNTH);
            if(fluidSynth.isInitialized())
            {
                PlaybackInterface ifs; zap(ifs);
                ifs.type  = AUDIO_IMUSIC;
                ifs.i.any = &fluidSynth.iMusic();
                activeInterfaces << ifs;  // a copy is made
            }
        }
#endif

        if(defaultDriver.hasCd())
        {
            PlaybackInterface ifs; zap(ifs);
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
                Driver &driver = driverById(initDriverIfNeeded(cmdLine.at(++p)));
                if(!driver.hasSfx())
                    throw Error("selectInterfaces", "Audio driver \"" + driver.name() + "\" does not provide a SFX interface");

                PlaybackInterface ifs; zap(ifs);
                ifs.type  = AUDIO_ISFX;
                ifs.i.any = &driver.iSfx();
                activeInterfaces << ifs;  // a copy is made
                continue;
            }

            // Check for Music override.
            if(cmdLine.matches("-imusic", cmdLine.at(p)))
            {
                Driver &driver = driverById(initDriverIfNeeded(cmdLine.at(++p)));
                if(!driver.hasMusic())
                    throw Error("selectInterfaces", "Audio driver \"" + driver.name() + "\" does not provide a Music interface");

                PlaybackInterface ifs; zap(ifs);
                ifs.type  = AUDIO_IMUSIC;
                ifs.i.any = &driver.iMusic();
                activeInterfaces << ifs;  // a copy is made
                continue;
            }

            // Check for CD override.
            if(cmdLine.matches("-icd", cmdLine.at(p)))
            {
                Driver &driver = driverById(initDriverIfNeeded(cmdLine.at(++p)));
                if(!driver.hasCd())
                    throw Error("selectInterfaces", "Audio driver \"" + driver.name() + "\" does not provide a CD interface");

                PlaybackInterface ifs; zap(ifs);
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
                PlaybackInterface const &ifs = activeInterfaces[i];
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

    /**
     * Find the Base interface of the audio driver to which @a anyAudioInterface
     * belongs.
     *
     * @param anyAudioInterface  Pointer to a SFX, Music, or CD interface.
     *
     * @return Audio interface, or @c nullptr if the none of the loaded drivers match.
     */
    audiodriver_t &getBaseInterface(void *anyAudioInterface) const
    {
        if(anyAudioInterface)
        {
            for(Driver const &driver : drivers)
            {
                if((void *)&driver.iSfx()   == anyAudioInterface ||
                   (void *)&driver.iMusic() == anyAudioInterface ||
                   (void *)&driver.iCd()    == anyAudioInterface)
                {
                    return driver.iBase();
                }
            }
        }
        throw Error("audio::System::getBaseInterface", "Unknown playback interface");
    }

    audiointerfacetype_t interfaceType(void *anyAudioInterface) const
    {
        if(anyAudioInterface)
        {
            for(Driver const &driver : drivers)
            {
                if((void *)&driver.iSfx()   == anyAudioInterface) return AUDIO_ISFX;
                if((void *)&driver.iMusic() == anyAudioInterface) return AUDIO_IMUSIC;
                if((void *)&driver.iCd()    == anyAudioInterface) return AUDIO_ICD;
            }
        }
        return AUDIO_INONE;
    }

    String interfaceName(void *anyAudioInterface) const
    {
        if(anyAudioInterface)
        {
            for(Driver const &driver : drivers)
            {
                String const name = driver.interfaceName(anyAudioInterface);
                if(!name.isEmpty()) return name;
            }
        }
        return "(invalid)";
    }
#endif  // __CLIENT__

    /**
     * LogicSounds are used to track currently playing sounds on a logical
     * level (irrespective of whether playback is available, or if the sounds
     * are actually audible to anyone).
     *
     * @todo The premise behind this functionality is fundamentally flawed in
     * that it assumes that the same samples are used by both the Client and
     * the Server, and that the later schedules remote playback of the former
     * (determined by examining sample lengths on Server side).
     *
     * Furthermore, the Server should not be dictating 'oneSoundPerEmitter'
     * policy so that Clients can be configured independently.
     */
    struct LogicSound
    {
        mobj_t *emitter  = nullptr;
        duint endTime    = 0;
        bool isRepeating = false;

        bool inline isPlaying(duint nowTime) const {
            return (isRepeating || endTime > nowTime);
        }
    };
    typedef QMultiHash<dint /*key: soundId*/, LogicSound *> LogicSoundHash;
    typedef QMutableHashIterator<dint /*key: soundId*/, LogicSound *> MutableLogicSoundHashIterator;

#if __CLIENT__
    bool musAvail = false;              ///< @c true if at least one driver is initialized for music playback.
    bool musNeedBufFileSwitch = false;  ///< @c true= choose a new file name for the buffered playback file when asked. */
    String musCurrentSong;
    bool musPaused = false;

    bool sfxAvail = false;              ///< @c true if a sound driver is initialized for sound effect playback.
    mobj_t *sfxListener = nullptr;
    SectorCluster *sfxListenerCluster = nullptr;
#endif
    LogicSoundHash sfxLogicHash;
    duint sfxLogicLastPurge = 0;
    bool sfxLogicOneSoundPerEmitter = false;  ///< set at the start of the frame

    SampleCache sampleCache;
#ifdef __CLIENT__
    std::unique_ptr<Channels> channels;
#endif

    Instance(Public *i) : Base(i)
    {
        theAudioSystem = thisPublic;

#ifdef __CLIENT__
        App::app().audienceForGameUnload() += this;
#endif
    }

    ~Instance()
    {
        sfxClearLogical();
#ifdef __CLIENT__
        App::app().audienceForGameUnload() -= this;
#endif

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
        return MUSIC_BUFFEREDFILE + String::number(currentBufFile) + ext;
    }

    void setMusicProperty(dint prop, void const *ptr)
    {
        forAllInterfaces(AUDIO_IMUSIC, [this, &prop, &ptr] (void *ifs)
        {
            audiodriver_t &iBase = getBaseInterface(ifs);
            if(iBase.Set) iBase.Set(prop, ptr);
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
                    auto *buf = (duint8 *)M_Malloc(len);
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

        if(!App_FileSystem().nameIndex().hasLump(lumpNum))
            return 0;

        File1 &lump = App_FileSystem().lump(lumpNum);
        if(M_MusRecognize(lump))
        {
            // Lump is in DOOM's MUS format.
            if(!canPlayMUS) return -1;

            // Read the lump, convert to MIDI and output to a temp file in the
            // working directory. Use a filename with the .mid extension so that
            // any player which relies on the it for format recognition works as
            // expected.
            String const srcFile = composeMusicBufferFilename(".mid");
            M_Mus2Midi(lump, srcFile.toUtf8().constData());

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

        LOG_AUDIO_VERBOSE("Initializing music playback...");

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
                    << interfaceName(iMusic);
            }
            return LoopContinue;
        });

        // Remember whether an interface for music playback initialized successfully.
        musAvail = initialized >= 1;
        if(musAvail)
        {
            // Tell audio drivers about our soundfont config.
            self.updateMusicMidiFont();
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

    void updateMusicVolumeIfChanged()
    {
        if(!musAvail) return;

        static dint oldMusVolume = -1;
        if(musVolume != oldMusVolume)
        {
            oldMusVolume = musVolume;

            // Set volume of all available interfaces.
            dfloat newVolume = musVolume / 255.0f;
            forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [&newVolume] (void *ifs)
            {
                auto *iMusic = (audiointerface_music_generic_t *) ifs;
                iMusic->Set(MUSIP_VOLUME, newVolume);
                return LoopContinue;
            });
        }
    }

    /**
     * Perform initialization for sound effect playback.
     */
    void initSfx()
    {
        // Already initialized?
        if(sfxAvail) return;

        // Check if sound has been disabled with a command line option.
        if(App::commandLine().has("-nosfx"))
        {
            LOG_AUDIO_NOTE("Sound effects disabled");
            return;
        }

        LOG_AUDIO_VERBOSE("Initializing sound effect playback...");
        // No available interface?
        if(!self.sfx()) return;

        // This is based on the scientific calculations that if the DOOM marine
        // is 56 units tall, 60 is about two meters.
        //// @todo Derive from the viewheight.
        self.sfx()->Listener(SFXLP_UNITS_PER_METER, 30);
        self.sfx()->Listener(SFXLP_DOPPLER, 1.5f);

        // (Re)Init the sample cache.
        sampleCache.clear();

        // Initialize reverb effects to off.
        sfxListenerNoReverb();

        // The drivers are working; prepare playback channels and start the sound
        // channel refresh thread (if needed).
        initChannels();
        channels->initRefresh();

        // The Sfx module is now available.
        sfxAvail = true;
    }

    /**
     * Perform deinitialization for sound effect playback.
     */
    void deinitSfx()
    {
        // Not initialized?
        if(!sfxAvail) return;

        sfxAvail = false;

        // Clear the sample cache.
        sampleCache.clear();

        // Destroy channels (and stop the refresh thread if running).
        channels.reset();
    }

    /**
     * Destroys and then recreates the sound Channels according to the current mode.
     */
    void initChannels()
    {
        dint numChannels = CHANNEL_COUNT_DEFAULT;
        // The -sfxchan option can be used to change the number of channels.
        if(CommandLine_CheckWith("-sfxchan", 1))
        {
            numChannels = de::clamp(1, String(CommandLine_Next()).toInt(), CHANNEL_COUNT_MAX);
            LOG_AUDIO_NOTE("Initialized %i sound effect channels") << numChannels;
        }

        // Change the primary buffer format to match the channel format.
        dfloat parm[2] = { dfloat(sfxBits), dfloat(sfxRate) };
        self.sfx()->Listenerv(SFXLP_PRIMARY_FORMAT, parm);

        // Replace the entire channel set (we'll reconfigure).
        channels.reset(new Channels);
        dint num2D = sfx3D ? CHANNEL_2DCOUNT: numChannels;  // The rest will be 3D.
        for(dint i = 0; i < numChannels; ++i)
        {
            auto *ch = new Channel;
            ch->setBuffer(self.sfx()->Create(num2D-- > 0 ? 0 : SFXBF_3D, sfxBits, sfxRate));
            if(!ch->hasBuffer())
            {
                LOG_AUDIO_WARNING("Failed creating (sample) buffer for Channel #%i") << i;
            }
            channels->add(*ch);
        }
    }

    void getChannelPriorities(dfloat *prios) const
    {
        if(!prios) return;

        dint idx = 0;
        channels->forAll([&prios, &idx] (Channel &ch)
        {
            prios[idx++] = ch.priority();
            return LoopContinue;
        });
    }

    /**
     * Used by the high-level sound interface to play sounds on the local system.
     *
     * @param sample   Sample to play (must be stored persistently! No copy is made).
     * @param volume   Volume at which the sample should be played.
     * @param freq     Relative and modifies the sample's rate.
     * @param emitter  If @c nullptr, @a fixedpos is checked for a position. If both
     *                 @a emitter and @a fixedpos are @c nullptr, then the sound is
     *                 played as centered 2D.
     * @param origin   Fixed position where the sound if emitted, or @c nullptr.
     * @param flags    Additional flags (@ref soundFlags).
     *
     * @return  @c true, if a sound is started.
     */
    bool playSound(sfxsample_t &sample, dfloat volume, dfloat freq, mobj_t *emitter,
        coord_t const *origin, dint flags)
    {
        if(!sfxAvail) return false;

        bool const play3D = sfx3D && (emitter || origin);

        if(sample.soundId < 1 || sample.soundId >= ::defs.sounds.size()) return false;
        if(volume <= 0 || !sample.size) return false;

        if(emitter && sfxOneSoundPerEmitter)
        {
            // Stop any other sounds from the same emitter.
            if(self.stopSoundWithLowerPriority(0, emitter, ::defs.sounds[sample.soundId].priority) < 0)
            {
                // Something with a higher priority is playing, can't start now.
                LOG_AUDIO_MSG("Not playing sound id:%i (prio:%i) because overridden (emitter id:%i)")
                    << sample.soundId
                    << ::defs.sounds[sample.soundId].priority
                    << emitter->thinker.id;
                return false;
            }
        }

        // Calculate the new sound's priority.
        dint const nowTime  = Timer_Ticks();
        dfloat const myPrio = self.rateSoundPriority(emitter, origin, volume, nowTime);

        bool haveChannelPrios = false;
        dfloat channelPrios[256/*MAX_CHANNEL_COUNT*/];
        dfloat lowPrio = 0;

        // Ensure there aren't already too many channels playing this sample.
        sfxinfo_t *info = &::runtimeDefs.sounds[sample.soundId];
        if(info->channels > 0)
        {
            // The decision to stop channels is based on priorities.
            getChannelPriorities(channelPrios);
            haveChannelPrios = true;

            dint count = channels->countPlaying(sample.soundId);
            while(count >= info->channels)
            {
                // Stop the lowest priority sound of the playing instances, again
                // noting sounds that are more important than us.
                dint idx = 0;
                Channel *selCh = nullptr;
                channels->forAll([&sample, &myPrio, &channelPrios,
                                  &selCh, &lowPrio, &idx] (Channel &ch)
                {
                    dfloat const chPriority = channelPrios[idx++];

                    if(ch.hasBuffer())
                    {
                        sfxbuffer_t &sbuf = ch.buffer();
                        if((sbuf.flags & SFXBF_PLAYING))
                        {
                            DENG2_ASSERT(sbuf.sample != nullptr);

                            if(sbuf.sample->soundId == sample.soundId &&
                               (myPrio >= chPriority && (!selCh || chPriority <= lowPrio)))
                            {
                                selCh   = &ch;
                                lowPrio = chPriority;
                            }
                        }
                    }

                    return LoopContinue;
                });

                if(!selCh)
                {
                    // The new sound can't be played because we were unable to stop
                    // enough channels to accommodate the limitation.
                    LOG_AUDIO_XVERBOSE("Not playing sound id:%i because all channels are busy")
                        << sample.soundId;
                    return false;
                }

                // Stop this one.
                count--;
                selCh->stop();
            }
        }

        // Hit count tells how many times the cached sound has been used.
        sampleCache.hit(sample.soundId);

        /*
         * Pick a channel for the sound. We will do our best to play the sound,
         * cancelling existing ones if need be. The ideal choice is a free channel
         * that is already loaded with the sample, in the correct format and mode.
         */
        channels->allowRefresh(false);

        // First look through the stopped channels. At this stage we're very picky:
        // only the perfect choice will be good enough.
        Channel *selCh = channels->tryFindVacant(play3D, sample.bytesPer,
                                                 sample.rate, sample.soundId);

        if(!selCh)
        {
            // Perhaps there is a vacant channel (with any sample, but preferably one
            // with no sample already loaded).
            selCh = channels->tryFindVacant(play3D, sample.bytesPer, sample.rate, 0);
        }

        if(!selCh)
        {
            // Try any non-playing channel in the correct format.
            selCh = channels->tryFindVacant(play3D, sample.bytesPer, sample.rate, -1);
        }

        if(!selCh)
        {
            // A perfect channel could not be found.
            // We must use a channel with the wrong format or decide which one of the
            // playing ones gets stopped.

            if(!haveChannelPrios)
            {
                getChannelPriorities(channelPrios);
            }

            // All channels with a priority less than or equal to ours can be stopped.
            Channel *prioCh = nullptr;
            dint idx = 0;
            channels->forAll([&play3D, &myPrio, &channelPrios,
                              &selCh, &prioCh, &lowPrio, &idx] (Channel &ch)
            {
                dfloat const chPriority = channelPrios[idx++];

                if(ch.hasBuffer())
                {
                    // Sample buffer must be configured for the right mode.
                    sfxbuffer_t &sbuf = ch.buffer();
                    if(play3D == ((sbuf.flags & SFXBF_3D) != 0))
                    {
                        if(!(sbuf.flags & SFXBF_PLAYING))
                        {
                            // This channel is not playing, we'll take it!
                            selCh = &ch;
                            return LoopAbort;
                        }

                        // Are we more important than this sound?
                        // We want to choose the lowest priority sound.
                        if(myPrio >= chPriority && (!prioCh || chPriority <= lowPrio))
                        {
                            prioCh  = &ch;
                            lowPrio = chPriority;
                        }
                    }
                }

                return LoopContinue;
            });

            // If a good low-priority channel was found, use it.
            if(prioCh)
            {
                selCh = prioCh;
                selCh->stop();
            }
        }

        if(!selCh)
        {
            // A suitable channel was not found.
            channels->allowRefresh(true);
            LOG_AUDIO_XVERBOSE("Failed to find suitable channel for sample id:%i") << sample.soundId;
            return false;
        }

        DENG2_ASSERT(selCh->hasBuffer());
        // The sample buffer may need to be reformatted.

        if(selCh->buffer().rate  != sample.rate ||
           selCh->buffer().bytes != sample.bytesPer)
        {
            // Create a new sample buffer with the correct format.
            self.sfx()->Destroy(&selCh->buffer());
            selCh->setBuffer(self.sfx()->Create(play3D ? SFXBF_3D : 0, sample.bytesPer * 8, sample.rate));
        }
        sfxbuffer_t &sbuf = selCh->buffer();

        // Configure buffer flags.
        sbuf.flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);
        if(flags & SF_REPEAT)    sbuf.flags |= SFXBF_REPEAT;
        if(flags & SF_DONT_STOP) sbuf.flags |= SFXBF_DONT_STOP;

        // Init the channel information.
        selCh->setFlags(selCh->flags() & ~(SFXCF_NO_ORIGIN | SFXCF_NO_ATTENUATION | SFXCF_NO_UPDATE));
        selCh->setVolume(volume);
        selCh->setFrequency(freq);

        if(!emitter && !origin)
        {
            selCh->setFlags(selCh->flags() | SFXCF_NO_ORIGIN);
            selCh->setEmitter(nullptr);
        }
        else
        {
            selCh->setEmitter(emitter);
            if(origin)
            {
                selCh->setFixedOrigin(Vector3d(origin));
            }
        }

        if(flags & SF_NO_ATTENUATION)
        {
            // The sound can be heard from any distance.
            selCh->setFlags(selCh->flags() | SFXCF_NO_ATTENUATION);
        }

        /**
         * Load in the sample. Must load prior to setting properties, because the driver
         * might actually create the real buffer only upon loading.
         *
         * @note The sample is not reloaded if a sample with the same ID is already loaded
         * on the channel.
         */
        if(!sbuf.sample || sbuf.sample->soundId != sample.soundId)
        {
            self.sfx()->Load(&sbuf, &sample);
        }

        // Update channel properties.
        selCh->updatePriority();

        // 3D sounds need a few extra properties set up.
        if(play3D)
        {
            // Init the buffer's min/max distances.
            // This is only done once, when the sound is started (i.e., here).
            dfloat const minDist = (selCh->flags() & SFXCF_NO_ATTENUATION) ? 10000 : sfxDistMin;
            dfloat const maxDist = (selCh->flags() & SFXCF_NO_ATTENUATION) ? 20000 : sfxDistMax;

            self.sfx()->Set(&sbuf, SFXBP_MIN_DISTANCE, minDist);
            self.sfx()->Set(&sbuf, SFXBP_MAX_DISTANCE, maxDist);
        }

        // This'll commit all the deferred properties.
        self.sfx()->Listener(SFXLP_UPDATE, 0);

        // Start playing.
        self.sfx()->Play(&sbuf);

        channels->allowRefresh();

        // Take note of the start time.
        selCh->setStartTime(nowTime);

        // Sound successfully started.
        return true;
    }

#endif  // __CLIENT__

    void sfxClearLogical()
    {
        qDeleteAll(sfxLogicHash);
        sfxLogicHash.clear();
    }

    /**
     * Maybe remove stopped sounds from the LSM.
     */
    void sfxPurgeLogical()
    {
        // Too soon?
        duint const nowTime = Timer_RealMilliseconds();
        if(nowTime - sfxLogicLastPurge < PURGEINTERVAL) return;

        // Peform the purge now.
        LOGDEV_AUDIO_XVERBOSE("purging logic sound hash...");
        sfxLogicLastPurge = nowTime;

        // Check all sounds in the hash.
        MutableLogicSoundHashIterator it(sfxLogicHash);
        while(it.hasNext())
        {
            it.next();
            LogicSound &lsound = *it.value();
            if(!lsound.isRepeating && lsound.endTime < nowTime)
            {
                // This has stopped.
                delete &lsound;
                it.remove();
            }
        }
    }

    /**
     * The sound is removed from the list of playing sounds. Called whenever a sound is
     * stopped, regardless of whether it was actually playing on the local system.
     *
     * @note If @a soundId == 0 and @a emitter == nullptr then stop everything.
     *
     * @return  Number of sounds stopped.
     */
    dint sfxStopLogical(dint soundId, mobj_t *emitter)
    {
        dint numStopped = 0;
        MutableLogicSoundHashIterator it(sfxLogicHash);
        while(it.hasNext())
        {
            it.next();

            LogicSound const &lsound = *it.value();
            if(soundId)
            {
                if(it.key() != soundId) continue;
            }
            else if(emitter)
            {
                if(lsound.emitter != emitter) continue;
            }

            delete &lsound;
            it.remove();
            numStopped += 1;
        }
        return numStopped;
    }

    /**
     * The sound is entered into the list of playing sounds. Called when a 'world class'
     * sound is started, regardless of whether it's actually started on the local system.
     *
     * @todo Why does the Server cache sound samples and/or care to know the length of
     * the samples? It is entirely possible that the Client is using a different set of
     * samples so using this information on server side (for scheduling of remote playback
     * events?) is not logical. -ds
     */
    void sfxStartLogical(dint soundIdAndFlags, mobj_t *emitter)
    {
        dint const soundId = (soundIdAndFlags & ~DDSF_FLAG_MASK);

        // Cache the sound sample associated with @a soundId (if necessary)
        // so that we can determine it's length.
        if(sfxsample_t *sample = sampleCache.cache(soundId))
        {
            bool const isRepeating = (soundIdAndFlags & DDSF_REPEAT) || Def_SoundIsRepeating(soundId);

            duint length = (1000 * sample->numSamples) / sample->rate;
            if(isRepeating && length > 1)
            {
                length = 1;
            }

            // Ignore zero length sounds.
            /// @todo Shouldn't we still stop others though? -ds
            if(!length) return;

            // Only one sound per emitter?
            if(emitter && sfxLogicOneSoundPerEmitter)
            {
                // Stop all other sounds.
                sfxStopLogical(0, emitter);
            }

            auto *ls = new Instance::LogicSound;
            ls->emitter     = emitter;
            ls->isRepeating = isRepeating;
            ls->endTime     = Timer_RealMilliseconds() + length;
            sfxLogicHash.insert(soundId, ls);
        }
    }

    /**
     * @param sectorEmitter  Sector in which to stop sounds.
     * @param soundId        Unique identifier of the sound to be stopped.
     *                       If @c 0, ID not checked.
     * @param flags          @ref soundStopFlags
     */
    void stopSectorSounds(ddmobj_base_t *sectorEmitter, dint soundId, dint flags)
    {
        if(!sectorEmitter || !flags) return;

        // Are we stopping with this sector's emitter?
        if(flags & SSF_SECTOR)
        {
            self.stopSound(soundId, (mobj_t *)sectorEmitter);
        }

        // Are we stopping with linked emitters?
        if(!(flags & SSF_SECTOR_LINKED_SURFACES)) return;

        // Process the rest of the emitter chain.
        ddmobj_base_t *base = sectorEmitter;
        while((base = (ddmobj_base_t *)base->thinker.next))
        {
            // Stop sounds from this emitter.
            self.stopSound(soundId, (mobj_t *)base);
        }
    }

#ifdef __CLIENT__

    /**
     * Returns the 3D position of the sound effect listener, in map space.
     */
    Vector3d getSfxListenerOrigin() const
    {
        if(sfxListener)
        {
            auto origin = Vector3d(sfxListener->origin);
            origin.z += sfxListener->height - 5;  /// @todo Make it exactly eye-level! (viewheight).
            return origin;
        }
        return Vector3d();
    }

    void sfxListenerNoReverb()
    {
        if(!sfxAvail) return;

        sfxListenerCluster = nullptr;

        dfloat rev[4] = { 0, 0, 0, 0 };
        self.sfx()->Listenerv(SFXLP_REVERB, rev);
        self.sfx()->Listener(SFXLP_UPDATE, 0);
    }

    void updateSfxListener()
    {
        if(!sfxAvail || !sfx3D) return;

        // No volume means no sound.
        if(!sfxVolume) return;

        // Update the listener mobj.
        sfxListener = getListenerMobj();
        if(sfxListener)
        {
            {
                // Origin. At eye-level.
                auto const origin = Vector4f(getSfxListenerOrigin().toVector3f(), 0);
                dfloat vec[4];
                origin.decompose(vec);
                self.sfx()->Listenerv(SFXLP_POSITION, vec);
            }
            {
                // Orientation. (0,0) will produce front=(1,0,0) and up=(0,0,1).
                dfloat vec[2] = {
                    sfxListener->angle / (dfloat) ANGLE_MAX * 360,
                    (sfxListener->dPlayer ? LOOKDIR2DEG(sfxListener->dPlayer->lookDir) : 0)
                };
                self.sfx()->Listenerv(SFXLP_ORIENTATION, vec);
            }
            {
                // Velocity. The unit is world distance units per second
                auto const velocity = Vector4f(Vector3d(sfxListener->mom).toVector3f(), 0) * TICSPERSEC;
                dfloat vec[4];
                velocity.decompose(vec);
                self.sfx()->Listenerv(SFXLP_VELOCITY, vec);
            }

            // Reverb effects. Has the current sector cluster changed?
            SectorCluster *newCluster = Mobj_ClusterPtr(*sfxListener);
            if(newCluster && (!sfxListenerCluster || sfxListenerCluster != newCluster))
            {
                sfxListenerCluster = newCluster;

                // It may be necessary to recalculate the reverb properties...
                AudioEnvironmentFactors const &envFactors = sfxListenerCluster->reverb();
                dfloat vec[NUM_REVERB_DATA];
                for(dint i = 0; i < NUM_REVERB_DATA; ++i)
                {
                    vec[i] = envFactors[i];
                }
                vec[SRD_VOLUME] *= sfxReverbStrength;
                self.sfx()->Listenerv(SFXLP_REVERB, vec);
            }
        }

        // Update all listener properties.
        self.sfx()->Listener(SFXLP_UPDATE, 0);
    }

    void updateSfx3DModeIfChanged()
    {
        static dint old3DMode = false;

        if(old3DMode == sfx3D) return;  // No change.

        LOG_AUDIO_VERBOSE("Switching to %s mode...") << (old3DMode ? "2D" : "3D");

        // Re-create the sound Channels.
        initChannels();

        if(old3DMode)
        {
            // Going 2D - ensure reverb is disabled.
            sfxListenerNoReverb();
        }
        old3DMode = sfx3D;
    }

    void updateSfxSampleRateIfChanged()
    {
        static dint old16Bit = false;
        static dint oldRate  = 11025;

        // Ensure the rate is valid.
        if(sfxSampleRate != 11025 && sfxSampleRate != 22050 && sfxSampleRate != 44100)
        {
            LOG_AUDIO_WARNING("\"sound-rate\" corrected to 11025 from invalid value (%i)") << sfxSampleRate;
            sfxSampleRate = 11025;
        }

        // Do we need to change the sample format?
        if(old16Bit != sfx16Bit || oldRate != sfxSampleRate)
        {
            dint const newBits = sfx16Bit ? 16 : 8;
            dint const newRate = sfxSampleRate;
            if(::sfxBits != newBits || ::sfxRate != newRate)
            {
                LOG_AUDIO_VERBOSE("Switching sound rate to %iHz (%i-bit)..") << newRate << newBits;

                // Set the new buffer format.
                ::sfxBits = newBits;
                ::sfxRate = newRate;
                initChannels();

                // The cache just became useless, clear it.
                sampleCache.clear();
            }
            old16Bit = sfx16Bit;
            oldRate  = sfxSampleRate;
        }
    }

    void aboutToUnloadGame(game::Game const &)
    {
        self.reset();
    }

#endif  // __CLIENT__
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

#ifdef __CLIENT__

String System::description() const
{
#define TABBED(A, B)  _E(Ta) "  " _E(l) A _E(.) " " _E(Tb) << (B) << "\n"

    String str;
    QTextStream os(&str);

    os << _E(b) "Audio configuration:\n" _E(.);

    os << TABBED("Sound volume:", sfxVolume);

    os << TABBED("Music volume:", musVolume);
    String const midiFontPath(musMidiFontPath);
    os << TABBED("Music sound font:", midiFontPath.isEmpty() ? "None" : midiFontPath);
    os << TABBED("Music source preference:", musicSourceAsText(musSourcePreference));

    // Include an active playback interface itemization.
    for(dint i = d->activeInterfaces.count(); i--> 0; )
    {
        Instance::PlaybackInterface const &ifs = d->activeInterfaces[i];

        if(ifs.type == AUDIO_IMUSIC || ifs.type == AUDIO_ICD)
        {
            os << _E(Ta) _E(l) "  " << (ifs.type == AUDIO_IMUSIC ? "Music" : "CD") << ": "
               << _E(.) _E(Tb) << d->interfaceName(ifs.i.any) << "\n";
        }
        else if(ifs.type == AUDIO_ISFX)
        {
            os << _E(Ta) _E(l) << "  SFX: " << _E(.) _E(Tb)
               << d->interfaceName(ifs.i.sfx) << "\n";
        }
    }

    return str.rightStrip();

#undef TABBED
}

void System::reset()
{
    LOG_AS("audio::System");
    LOG_AUDIO_VERBOSE("Reseting...");

    if(d->sfxAvail)
    {
        d->sfxListenerCluster = nullptr;

        // Stop all channels.
        d->channels->forAll([] (Channel &ch)
        {
            ch.stop();
            return LoopContinue;
        });

        // Clear the sample cache.
        d->sampleCache.clear();
    }

    stopMusic();
}
#endif

/**
 * @todo Do this in timeChanged()
 */
void System::startFrame()
{
    LOG_AS("audio::System");

#ifdef __CLIENT__
    d->updateMusicVolumeIfChanged();

    if(sfxIsAvailable())
    {
        // Update all channels (freq, 2D:pan,volume, 3D:position,velocity).

        // Update the active interface.
        d->getBaseInterface(sfx()).Event(SFXEV_BEGIN);

        // Have there been changes to the cvar settings?
        d->updateSfx3DModeIfChanged();
        d->updateSfxSampleRateIfChanged();

        // Should we purge the cache (to conserve memory)?
        d->sampleCache.maybeRunPurge();
    }

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

    d->sfxLogicOneSoundPerEmitter = sfxOneSoundPerEmitter;
    d->sfxPurgeLogical();
}

#ifdef __CLIENT__
void System::endFrame()
{
    LOG_AS("audio::System");

    if(sfxIsAvailable())
    {
        if(!BusyMode_Active())
        {
            // Update channel and listener properties.

            // If no listener is available - no 3D positioning is done.
            d->sfxListener = getListenerMobj();

            // Update channels.
            d->channels->forAll([] (Channel &ch)
            {
                if(ch.hasBuffer() && (ch.buffer().flags & SFXBF_PLAYING))
                {
                    ch.updatePriority();
                }
                return LoopContinue;
            });

            // Update listener.
            d->updateSfxListener();
        }

        // Update the active interface.
        d->getBaseInterface(sfx()).Event(SFXEV_END);
    }
}

void System::initPlayback()
{
    LOG_AS("audio::System");

    CommandLine &cmdLine = App::commandLine();
    if(cmdLine.has("-nosound") || cmdLine.has("-noaudio"))
        return;

    LOG_AUDIO_VERBOSE("Initializing for playback...");

    // Disable random pitch changes?
    sfxNoRndPitch = cmdLine.has("-norndpitch");

    // Try to load the audio driver plugin(s).
    if(d->loadDrivers())
    {
        // Init for sound effects.
        try
        {
            d->initSfx();
        }
        catch(Error const &er)
        {
            LOG_AUDIO_NOTE("Failed initializing playback for sound effects:\n")
                << er.asText();
        }

        // Init for music.
        try
        {
            d->initMusic();
        }
        catch(Error const &er)
        {
            LOG_AUDIO_NOTE("Failed initializing playback for music:\n")
                << er.asText();
        }
    }
    else
    {
        LOG_AUDIO_NOTE("Music and sound effects are disabled");
    }

    // Print a summary of the active configuration to the log.
    LOG_AUDIO_MSG("%s") << description();
}

void System::deinitPlayback()
{
    LOG_AS("audio::System");

    d->deinitSfx();
    d->deinitMusic();

    d->unloadDrivers();
}

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

dint System::musicVolume() const
{
    return musVolume;
}

bool System::musicIsAvailable() const
{
    return d->musAvail;
}

bool System::musicIsPlaying() const
{
    //LOG_AS("audio::System");
    return d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_t *) ifs;
        return iMusic->gen.Get(MUSIP_PLAYING, nullptr);
    });
}

void System::stopMusic()
{
    if(!d->musAvail) return;

    LOG_AS("audio::System");
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

    LOG_AS("audio::System");
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
    if(::isDedicated) return true;

    if(!d->musAvail) return false;

    LOG_AS("audio::System");
    LOG_AUDIO_MSG("Playing song \"%s\"%s") << definition.gets("id") << (looped ? " looped" : "");
    //LOG_AUDIO_VERBOSE("Current song '%s'") << d->musCurrentSong;

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
            if(d->playMusicFile(App_ResourceSystem().tryFindMusicFile(definition), looped))
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

        default: DENG2_ASSERT(!"Invalid value for order[i]"); break;
        }
    }

    // No song was started.
    return false;
}

dint System::playMusicLump(lumpnum_t lumpNum, bool looped)
{
    stopMusic();
    LOG_AS("audio::System");
    LOG_AUDIO_MSG("Playing music lump #%i%s...")
        << lumpNum << (looped ? " looped" : "");
    return d->playMusicLump(lumpNum, looped);
}

dint System::playMusicFile(String const &filePath, bool looped)
{
    stopMusic();
    LOG_AS("audio::System");
    LOG_AUDIO_MSG("Playing music file \"%s\"%s...")
        << NativePath(filePath).pretty() << (looped ? " looped" : "");
    return d->playMusicFile(filePath, looped);
}

dint System::playMusicCDTrack(dint cdTrack, bool looped)
{
    stopMusic();
    LOG_AS("audio::System");
    LOG_AUDIO_MSG("Playing music CD track #%i%s...")
        << cdTrack << (looped ? " looped" : "");
    return d->playMusicCDTrack(cdTrack, looped);
}

void System::updateMusicMidiFont()
{
    LOG_AS("audio::System");

    NativePath path(musMidiFontPath);
#ifdef MACOSX
    // On OS X we can try to use the basic DLS soundfont that's part of CoreAudio.
    if(path.isEmpty())
    {
        path = "/System/Library/Components/CoreAudio.component/Contents/Resources/gs_instruments.dls";
    }
#endif

    d->setMusicProperty(AUDIOP_SOUNDFONT_FILENAME, path.expand().toString().toLatin1().constData());
}

dint System::soundVolume() const
{
    return sfxVolume;
}

#endif  // __CLIENT__

Rangei System::soundVolumeAttenuationRange() const
{
    return Rangei(sfxDistMin, sfxDistMax);
}

#ifdef __CLIENT__

bool System::sfxIsAvailable() const
{
    return d->sfxAvail;
}

bool System::mustUpsampleToSfxRate() const
{
    dint anyRateAccepted = 0;
    if(sfx()->Getv)
    {
        sfx()->Getv(SFXIP_ANY_SAMPLE_RATE_ACCEPTED, &anyRateAccepted);
    }
    return (anyRateAccepted ? false : true);
}

mobj_t *System::sfxListener()
{
    return d->sfxListener;
}

#endif  // __CLIENT__

bool System::soundIsPlaying(dint soundId, mobj_t *emitter) const
{
    //LOG_AS("audio::System");

    // Use the logic sound hash to determine whether the referenced sound is being
    // played currently. We don't care whether its audible or not.
    duint const nowTime = Timer_RealMilliseconds();
    if(soundId)
    {
        auto it = d->sfxLogicHash.constFind(soundId);
        while(it != d->sfxLogicHash.constEnd() && it.key() == soundId)
        {
            Instance::LogicSound const &lsound = *it.value();
            if(lsound.emitter == emitter && lsound.isPlaying(nowTime))
                return true;

            ++it;
        }
    }
    else if(emitter)
    {
        // Check if the emitter is playing any sound.
        auto it = d->sfxLogicHash.constBegin();
        while(it != d->sfxLogicHash.constEnd())
        {
            Instance::LogicSound const &lsound = *it.value();
            if(lsound.emitter == emitter && lsound.isPlaying(nowTime))
                return true;

            ++it;
        }
    }
    return false;

#if 0
    // Use the sound channels to determine whether the referenced sound is actually
    // being played currently.
    if(!d->sfxAvail) return false;

    return d->channels->forAll([&id, &emitter] (Channel &ch)
    {
        if(ch.hasSample())
        {
            sfxbuffer_t &sbuf = ch.sample();

            if(!(sbuf.flags & SFXBF_PLAYING) ||
               ch.emitter() != emitter ||
               id && sbuf.sample->id != id)
            {
                return LoopContinue;
            }

            // Once playing, repeating sounds don't stop.
            if(sbuf.flags & SFXBF_REPEAT)
                return LoopAbort;

            // Check time. The flag is updated after a slight delay (only at refresh).
            if(Sys_GetTime() - ch->startTime() < sbuf.sample->numsamples / (dfloat) sbuf.freq * TICSPERSEC)
            {
                return LoopAbort;
            }
        }
        return LoopContinue;
    });
#endif
}

#ifdef __CLIENT__

void System::stopSoundGroup(dint group, mobj_t *emitter)
{
    if(!d->sfxAvail) return;
    LOG_AS("audio::System");
    d->channels->forAll([this, &group, &emitter] (Channel &ch)
    {
        if(ch.hasBuffer())
        {
            sfxbuffer_t &sbuf = ch.buffer();
            if((sbuf.flags & SFXBF_PLAYING) &&
               (sbuf.sample->group == group && (!emitter || ch.emitter() == emitter)))
            {
                // This channel must stop.
                sfx()->Stop(&sbuf);
            }
        }
        return LoopContinue;
    });
}

dint System::stopSoundWithLowerPriority(dint id, mobj_t *emitter, dint defPriority)
{
    if(!d->sfxAvail) return false;

    LOG_AS("audio::System");
    dint stopCount = 0;
    d->channels->forAll([this, &id, &emitter, &defPriority, &stopCount] (Channel &ch)
    {
        if(!ch.hasBuffer()) return LoopContinue;
        sfxbuffer_t &sbuf = ch.buffer();

        if(!(sbuf.flags & SFXBF_PLAYING) || (id && sbuf.sample->soundId != id) ||
           (emitter && ch.emitter() != emitter))
        {
            return LoopContinue;
        }

        // Can it be stopped?
        if(sbuf.flags & SFXBF_DONT_STOP)
        {
            // The emitter might get destroyed...
            ch.setEmitter(nullptr);
            ch.setFlags(ch.flags() | (SFXCF_NO_UPDATE | SFXCF_NO_ORIGIN));
            return LoopContinue;
        }

        // Check the priority.
        if(defPriority >= 0)
        {
            dint oldPrio = ::defs.sounds[sbuf.sample->soundId].priority;
            if(oldPrio < defPriority)  // Old is more important.
            {
                stopCount = -1;
                return LoopAbort;
            }
        }

        // This channel must be stopped!
        /// @todo should observe. -ds
        sfx()->Stop(&sbuf);
        ++stopCount;
        return LoopContinue;
    });

    return stopCount;
}

#endif  // __CLIENT__

void System::stopSound(dint soundId, mobj_t *emitter, dint flags)
{
    LOG_AS("audio::System");

    // Are we performing any special stop behaviors?
    if(emitter && flags)
    {
        if(emitter->thinker.id)
        {
            // Emitter is a real Mobj.
            d->stopSectorSounds(&Mobj_Sector(emitter)->soundEmitter(), soundId, flags);
            return;
        }

        // The head of the chain is the sector. Find it.
        while(emitter->thinker.prev)
        {
            emitter = (mobj_t *)emitter->thinker.prev;
        }
        d->stopSectorSounds((ddmobj_base_t *)emitter, soundId, flags);
        return;
    }

    // No special stop behavior.

#ifdef __CLIENT__
    stopSoundWithLowerPriority(soundId, emitter, -1);
#endif

    // Notify the LSM.
    if(d->sfxStopLogical(soundId, emitter))
    {
#ifdef __SERVER__
        // In netgames, the server is responsible for telling clients
        // when to stop sounds. The LSM will tell us if a sound was
        // stopped somewhere in the world.
        Sv_StopSound(soundId, emitter);
#endif
    }
}

#ifdef __CLIENT__
bool System::playSound(dint soundIdAndFlags, mobj_t *emitter, coord_t const *origin,
    dfloat volume)
{
    LOG_AS("audio::System");

    // A dedicated server never starts any local sounds (only logical sounds in the LSM).
    if(::isDedicated) return false;

    // Sounds cannot be started while in busy mode...
    if(DoomsdayApp::app().busyMode().isActive())
        return false;

    dint const soundId = (soundIdAndFlags & ~DDSF_FLAG_MASK);
    if(soundId <= 0 || soundId >= ::defs.sounds.size())
        return false;

    // Skip if sounds won't be heard.
    if(sfxVolume <= 0 || volume <= 0)
        return false;

    if(volume > 1)
    {
        LOGDEV_AUDIO_WARNING("Volume is too high (%f > 1)") << volume;
    }

    dfloat freq = 1;
    // This is the sound we're going to play.
    sfxinfo_t *info = Def_GetSoundInfo(soundId, &freq, &volume);
    if(!info) return false;  // Hmm? This ID is not defined.

    bool const isRepeating = (soundIdAndFlags & DDSF_REPEAT) || Def_SoundIsRepeating(soundId);

    // Check the distance (if applicable).
    if(!(info->flags & SF_NO_ATTENUATION) && !(soundIdAndFlags & DDSF_NO_ATTENUATION))
    {
        // If origin is too far, don't even think about playing the sound.
        coord_t const *point = (emitter ? emitter->origin : origin);
        if(Mobj_ApproxPointDistance(getListenerMobj(), point) > sfxDistMax)
            return false;
    }

    // Load the sample.
    sfxsample_t *sample = d->sampleCache.cache(soundId);
    if(!sample)
    {
        if(d->sfxAvail)
        {
            LOG_AUDIO_VERBOSE("Caching of sound %i failed") << soundId;
        }
        return false;
    }

    // Random frequency alteration? (Multipliers chosen to match original
    // sound code.)
    if(!sfxNoRndPitch)
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
        stopSoundGroup(info->group, (info->flags & SF_GLOBAL_EXCLUDE) ? nullptr : emitter);
    }

    // Let's play it.
    dint flags = 0;
    flags |= (((info->flags & SF_NO_ATTENUATION) || (soundIdAndFlags & DDSF_NO_ATTENUATION)) ? SF_NO_ATTENUATION : 0);
    flags |= (isRepeating ? SF_REPEAT : 0);
    flags |= ((info->flags & SF_DONT_STOP) ? SF_DONT_STOP : 0);
    return d->playSound(*sample, volume, freq, emitter, origin, flags);
}

dfloat System::rateSoundPriority(mobj_t *emitter, coord_t const *point, dfloat volume,
    dint startTic)
{
    // In five seconds all priority of a sound is gone.
    dfloat const timeoff  = 1000 * (Timer_Ticks() - startTic) / (5.0f * TICSPERSEC);

    if(!d->sfxListener || (!emitter && !point))
    {
        // The sound does not have an origin.
        return 1000 * volume - timeoff;
    }

    // The sound has an origin, base the points on distance.
    coord_t const *origin;
    if(emitter)
    {
        origin = emitter->origin;
    }
    else
    {
        // No emitter mobj, use the fixed source position.
        origin = point;
    }

    return 1000 * volume - Mobj_ApproxPointDistance(d->sfxListener, origin) / 2 - timeoff;
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

audiodriverid_t System::toDriverId(Driver const *driver) const
{
    if(driver && driver >= &d->drivers[0] && driver <= &d->drivers[AUDIODRIVER_COUNT])
    {
        return audiodriverid_t( driver - d->drivers );
    }
    return AUDIOD_INVALID;
}
#endif

#ifdef __CLIENT__
SampleCache const &System::sampleCache() const
{
    return d->sampleCache;
}

bool System::hasChannels()
{
    return bool( d->channels );
}

Channels &System::channels() const
{
    DENG2_ASSERT(d->channels.get() != nullptr);
    return *d->channels;
}

void System::requestSfxListenerUpdate()
{
    // Request a listener reverb update at the end of the frame.
    d->sfxListenerCluster = nullptr;
}

#endif  // __CLIENT__

void System::clearLogical()
{
    d->sfxClearLogical();
}

void System::startLogical(dint soundIdAndFlags, mobj_t *emitter)
{
    d->sfxStartLogical(soundIdAndFlags, emitter);
}

void System::aboutToUnloadMap()
{
    LOG_AS("audio::System");
    LOG_AUDIO_VERBOSE("Cleaning for map unload...");

    d->sfxClearLogical();

#ifdef __CLIENT__
    // Mobjs are about to be destroyed so stop all sound channels using one as an emitter.
    d->channels->forAll([] (Channel &ch)
    {
        if(ch.emitter())
        {
            ch.setEmitter(nullptr);
            ch.stop();
        }
        return LoopContinue;
    });

    // Sectors, too, for that matter.
    d->sfxListenerCluster = nullptr;
#endif
}

#ifdef __CLIENT__
void System::worldMapChanged()
{
    // Update who is listening now.
    d->sfxListener = getListenerMobj();
}
#endif

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
    System &audioSys = App_AudioSystem();

    if(!audioSys.musicIsAvailable())
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
            return audioSys.playMusic(*definition, looped);
        }
        LOG_RES_WARNING("Music '%s' not defined") << argv[1];
        return false;
    }

    if(argc == 3)
    {
        // Play a file referenced directly.
        if(!qstricmp(argv[1], "lump"))
        {
            return audioSys.playMusicLump(App_FileSystem().lumpNumForName(argv[2]), looped);
        }
        else if(!qstricmp(argv[1], "file"))
        {
            return audioSys.playMusicFile(argv[2], looped);
        }
        else if(!qstricmp(argv[1], "cd"))
        {
            if(!audioSys.cd())
            {
                LOG_AUDIO_WARNING("No CD audio interface available");
                return false;
            }
            return audioSys.playMusicCDTrack(String(argv[2]).toInt(), looped);
        }
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

static void sfxReverbStrengthChanged()
{
    App_AudioSystem().requestSfxListenerUpdate();
}

static void musicMidiFontChanged()
{
    App_AudioSystem().updateMusicMidiFont();
}
#endif

void System::consoleRegister()  // static
{
    // Sound effects:
#ifdef __CLIENT__
    C_VAR_INT     ("sound-16bit",         &sfx16Bit,              0, 0, 1);
    C_VAR_INT     ("sound-3d",            &sfx3D,                 0, 0, 1);
#endif
    C_VAR_BYTE    ("sound-overlap-stop",  &sfxOneSoundPerEmitter, 0, 0, 1);
#ifdef __CLIENT__
    C_VAR_INT     ("sound-rate",          &sfxSampleRate,         0, 11025, 44100);
    C_VAR_FLOAT2  ("sound-reverb-volume", &sfxReverbStrength,     0, 0, 1.5f, sfxReverbStrengthChanged);
    C_VAR_INT     ("sound-volume",        &sfxVolume,             0, 0, 255);

    C_CMD_FLAGS("playsound",  nullptr, PlaySound,  CMDF_NO_DEDICATED);

    // Music:
    C_VAR_CHARPTR2("music-soundfont",     &musMidiFontPath,       0, 0, 0, musicMidiFontChanged);
    C_VAR_INT     ("music-source",        &musSourcePreference,   0, 0, 2);
    C_VAR_INT     ("music-volume",        &musVolume,             0, 0, 255);

    C_CMD_FLAGS("pausemusic", nullptr, PauseMusic, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("playmusic",  nullptr, PlayMusic,  CMDF_NO_DEDICATED);
    C_CMD_FLAGS("stopmusic",  "",      StopMusic,  CMDF_NO_DEDICATED);

    // Debug:
    C_VAR_INT     ("sound-info",          &showSoundInfo,         0, 0, 1);
#endif
}

}  // namespace audio

using namespace audio;

// Music: ---------------------------------------------------------------------------

void S_PauseMusic(dd_bool paused)
{
#ifdef __CLIENT__
    App_AudioSystem().pauseMusic(paused);
#else
    DENG2_UNUSED(paused);
#endif
}

void S_StopMusic()
{
#ifdef __CLIENT__
    App_AudioSystem().stopMusic();
#endif
}

dd_bool S_StartMusicNum(dint musicId, dd_bool looped)
{
#ifdef __CLIENT__
    if(musicId >= 0 && musicId < ::defs.musics.size())
    {
        return App_AudioSystem().playMusic(::defs.musics[musicId], looped);
    }
    return false;
#else
    DENG2_UNUSED2(musicId, looped);
    return false;
#endif
}

dd_bool S_StartMusic(char const *musicId, dd_bool looped)
{
    dint idx = ::defs.getMusicNum(musicId);
    if(idx < 0)
    {
        if(musicId && !String(musicId).isEmpty())
        {
            LOG_AS("S_StartMusic");
            LOG_AUDIO_WARNING("Music \"%s\" not defined, cannot start playback") << musicId;
        }
        return false;
    }
    return S_StartMusicNum(idx, looped);
}

// Sound Effects: -------------------------------------------------------------------

dd_bool S_SoundIsPlaying(dint soundId, mobj_t *emitter)
{
    return (dd_bool) App_AudioSystem().soundIsPlaying(soundId, emitter);
}

void S_StopSound(dint soundId, mobj_t *emitter)
{
    App_AudioSystem().stopSound(soundId, emitter);
}

void S_StopSound2(dint soundId, mobj_t *emitter, dint flags)
{
    App_AudioSystem().stopSound(soundId, emitter, flags);
}

dint S_LocalSoundAtVolumeFrom(dint soundIdAndFlags, mobj_t *emitter, coord_t *origin,
    dfloat volume)
{
#ifdef __CLIENT__
    return App_AudioSystem().playSound(soundIdAndFlags, emitter, origin, volume);
#else
    DENG2_UNUSED4(soundIdAndFlags, emitter, origin, volume);
    return false;
#endif
}

dint S_LocalSoundAtVolume(dint soundIdAndFlags, mobj_t *emitter, dfloat volume)
{
    return S_LocalSoundAtVolumeFrom(soundIdAndFlags, emitter, nullptr, volume);
}

dint S_LocalSound(dint soundIdAndFlags, mobj_t *emitter)
{
    return S_LocalSoundAtVolumeFrom(soundIdAndFlags, emitter, nullptr, 1/*max volume*/);
}

dint S_LocalSoundFrom(dint soundIdAndFlags, coord_t *origin)
{
    return S_LocalSoundAtVolumeFrom(soundIdAndFlags, nullptr, origin, 1/*max volume*/);
}

dint S_StartSound(dint soundIdAndFlags, mobj_t *emitter)
{
#ifdef __SERVER__
    // The sound is audible to everybody.
    Sv_Sound(soundIdAndFlags, emitter, SVSF_TO_ALL);
#endif
    App_AudioSystem().startLogical(soundIdAndFlags, emitter);

    return S_LocalSound(soundIdAndFlags, emitter);
}

dint S_StartSoundEx(dint soundIdAndFlags, mobj_t *emitter)
{
#ifdef __SERVER__
    Sv_Sound(soundIdAndFlags, emitter, SVSF_TO_ALL | SVSF_EXCLUDE_ORIGIN);
#endif
    App_AudioSystem().startLogical(soundIdAndFlags, emitter);

    return S_LocalSound(soundIdAndFlags, emitter);
}

dint S_StartSoundAtVolume(dint soundIdAndFlags, mobj_t *emitter, dfloat volume)
{
#ifdef __SERVER__
    Sv_SoundAtVolume(soundIdAndFlags, emitter, volume, SVSF_TO_ALL);
#endif
    App_AudioSystem().startLogical(soundIdAndFlags, emitter);

    // The sound is audible to everybody.
    return S_LocalSoundAtVolume(soundIdAndFlags, emitter, volume);
}

dint S_ConsoleSound(dint soundIdAndFlags, mobj_t *emitter, dint targetConsole)
{
#ifdef __SERVER__
    Sv_Sound(soundIdAndFlags, emitter, targetConsole);
#endif

    // If it's for us, we can hear it.
    if(targetConsole == consolePlayer)
    {
        S_LocalSound(soundIdAndFlags, emitter);
    }

    return true;
}

DENG_DECLARE_API(S) =
{
    { DE_API_SOUND },
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
    S_SoundIsPlaying,
    S_StartMusic,
    S_StartMusicNum,
    S_StopMusic,
    S_PauseMusic
};
