/** @file audiosystem.cpp  Audio subsystem.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "audio/audiosystem.h"

#include "dd_share.h"      // SF_* flags
#include "dd_main.h"       // ::isDedicated
#include "def_main.h"      // ::defs
#include "api_map.h"
#include "world/p_players.h"
#include "world/thinkers.h"
#include "audio/s_cache.h"
#include "Sector"
#include "Subsector"

#ifdef __CLIENT__
#  include "sys_system.h"  // Sys_Sleep()
#  include "audio/m_mus2midi.h"
#  include "audio/sfxchannel.h"
#  include "audio/sys_audiod_dummy.h"
#  include "world/audioenvironment.h"
#  include "client/clientsubsector.h"
#  include <doomsday/defs/music.h>
#  include <doomsday/filesys/fs_main.h>
#  include <doomsday/filesys/fs_util.h>
#endif

#ifdef __SERVER__
#  include "server/sv_sound.h"
#endif

#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <de/App>
#include <de/Binder>
#include <de/Config>
#include <de/CommandLine>
#include <de/FileSystem>
#include <de/LogBuffer>
#include <de/NativeFile>
#include <de/ScriptSystem>
#include <de/timer.h>
#include <de/c_wrapper.h>
#include <de/concurrency.h>
#include <de/memory.h>

#include <QMultiHash>
#include <QtAlgorithms>

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

#ifdef __CLIENT__
#  if defined(MACOSX) && defined(MACOS_HAVE_QTKIT)
/// Built-in QuickTime audio interface implemented by MusicPlayer.m
DENG_EXTERN_C audiointerface_music_t audiodQuickTimeMusic;
#  endif
#endif

static AudioSystem *theAudioSystem;

static duint const SOUND_LOGICAL_PURGEINTERVAL = 2000;  ///< 2 seconds

static byte sfxOneSoundPerEmitter;  //< @c false= Traditional Doomsday behavior: allow sounds to overlap.

#ifdef __CLIENT__
static dint const SOUND_CHANNEL_COUNT_DEFAULT  = 16;
static dint const SOUND_CHANNEL_COUNT_MAX      = 256;
static dint const SOUND_CHANNEL_2DCOUNT        = 4;
static char const *MUSIC_BUFFEREDFILE          = "/tmp/dd-buffered-song";

static thread_t refreshHandle;
static volatile bool allowRefresh, refreshing;

static bool sfxNoRndPitch;  ///< @todo should be a cvar.

// Console variables:
//static dint sfx16Bit;
//static dint sfxSampleRate = 11025;
static dint sfx3D;
static dfloat sfxReverbStrength = 0.5f;
static char *musMidiFontPath = (char *) "";
// When multiple sources are available this setting determines which to use (mus < ext < cd).
static AudioSystem::MusicSource musSourcePreference = AudioSystem::MUSP_EXT;

static String const driverIdentifier[AUDIODRIVER_COUNT] = {
    "dummy",
    "sdlmixer",
    "openal",
    "fmod",
    "fluidsynth",
    "dsound",
    "winmm"
};

static audiodriverid_t identifierToDriverId(String name)
{
    for (dint i = 0; i < AUDIODRIVER_COUNT; ++i)
    {
        if (!driverIdentifier[i].compareWithoutCase(name))
            return audiodriverid_t(i);
    }
    LOG_AUDIO_ERROR("'%s' is not a valid audio driver name") << name;
    return AUDIOD_INVALID;
}

/**
 * This is a high-priority thread that periodically checks if the channels need
 * to be updated with more data. The thread terminates when it notices that the
 * channels have been destroyed. The Sfx audio driver maintains a 250ms buffer
 * for each channel, which means the refresh must be done often enough to keep
 * them filled.
 *
 * @todo Use a real mutex, will you?
 */
static dint C_DECL sfxChannelRefreshThread(void *)
{
    // We'll continue looping until the Sfx module is shut down.
    while (App_AudioSystem().sfxIsAvailable() && App_AudioSystem().hasSfxChannels())
    {
        // The bit is swapped on each refresh (debug info).
        ::refMonitor ^= 1;

        if (allowRefresh)
        {
            // Do the refresh.
            refreshing = true;
            App_AudioSystem().sfxChannels().refreshAll();
            refreshing = false;

            // Let's take a nap.
            Sys_Sleep(200);
        }
        else
        {
            // Refreshing is not allowed, so take a shorter nap while
            // waiting for allowRefresh.
            Sys_Sleep(150);
        }
    }

    // Time to end this thread.
    return 0;
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

static Value *Function_Audio_LocalSound(Context &, const Function::ArgumentValues &args);

#endif // __CLIENT__

DENG2_PIMPL(AudioSystem)
, DENG2_OBSERVES(DoomsdayApp, GameUnload)
#ifdef __CLIENT__
, DENG2_OBSERVES(audio::SfxSampleCache, SampleRemove)
#endif
{
    Record module;
    Binder binder;

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
        CommandLine &cmdLine = CommandLine::get();

        // No audio output?
        if (::isDedicated)
            return AUDIOD_DUMMY;

        if (cmdLine.has("-dummy"))
            return AUDIOD_DUMMY;

        if (cmdLine.has("-fmod"))
            return AUDIOD_FMOD;

        if (cmdLine.has("-oal") || cmdLine.has("-openal"))
            return AUDIOD_OPENAL;

#ifdef WIN32
        if (cmdLine.has("-dsound"))
            return AUDIOD_DSOUND;

        if (cmdLine.has("-winmm"))
            return AUDIOD_WINMM;
#endif

#ifndef DENG_DISABLE_SDLMIXER
        if (cmdLine.has("-sdlmixer"))
            return AUDIOD_SDL_MIXER;

        // FMOD is preferred, but SDL_mixer is also a fallback.
        if (!AudioDriver::isAvailable(driverIdentifier[AUDIOD_FMOD]))
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
        LOG_AS("AudioSystem");
        try
        {
            String const idStr = driverIdentifier[driverId];
            if (!AudioDriver::isAvailable(idStr))
            {
                return false;
            }

            AudioDriver &driver = driverById(driverId);
            switch (driverId)
            {
            case AUDIOD_DUMMY:
            case AUDIOD_OPENAL:
            case AUDIOD_FMOD:
            case AUDIOD_FLUIDSYNTH:
                driver.load(idStr);
                break;
#ifndef DENG_DISABLE_SDLMIXER
            case AUDIOD_SDL_MIXER:
                driver.load(idStr);
                break;
#endif
#ifdef WIN32
            case AUDIOD_DSOUND:
            case AUDIOD_WINMM:
                driver.load(idStr);
                break;
#endif
            default:
                return false;
            }

            // All loaded drivers are automatically initialized so they are ready for use.
            driver.initialize();
            return driver.isInitialized();
        }
        catch (AudioDriver::LoadError const &er)
        {
            LOG_AUDIO_WARNING("Failed initializing driver \"%s\":\n")
                << AudioDriver_GetName(driverId)
                << er.asText();
        }
        return false;
    }

    audiodriverid_t initDriverIfNeeded(String const &identifier)
    {
        audiodriverid_t id  = identifierToDriverId(identifier);
        AudioDriver &driver = driverById(id);
        if (!driver.isInitialized())
        {
            if (!initDriver(id))
            {
                return AUDIOD_DUMMY;
            }
        }
        return id;
    }

    bool loadDrivers()
    {
        activeInterfaces.clear();

        // The audio drivers may use Audio.outputs to declare which outputs are available.
        module.set("outputs", new DictionaryValue);

        if (CommandLine::get().has("-nosound"))
            return false;

        audiodriverid_t defaultDriverId = chooseAudioDriver();
        initDriver(defaultDriverId);

/*#ifndef DENG_DISABLE_SDLMIXER
        // Fallback option for the default driver.
        if (!ok)
        {
            defaultDriverId = AUDIOD_SDL_MIXER;
            ok = initDriver(defaultDriverId);
        }
#endif*/

        // Choose the interfaces to use.
        selectInterfaces(defaultDriverId);

        return !activeInterfaces.isEmpty();
    }

    void unloadDrivers()
    {
        // Deinitialize all loaded drivers. (Note: reverse order)
        for (dint i = AUDIODRIVER_COUNT; i--> 0; )
        {
            drivers[i].deinitialize();
        }

        module.set("outputs", new DictionaryValue);

        // Unload the plugins after everything has been shut down.
        for (AudioDriver &driver : drivers)
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

        bool isDummy() const
        {
            switch (type)
            {
            case AUDIO_ISFX:
                return !std::memcmp(i.sfx, &audiod_dummy_sfx, sizeof(*i.sfx));

            case AUDIO_IMUSIC:
                return !std::memcmp(i.music, &audiod_dummy_music, sizeof(*i.music));

            case AUDIO_ICD:
                return !std::memcmp(i.cd, &audiod_dummy_cd, sizeof(*i.cd));

            default:
                break;
            }
            return false;
        }
    };
    QList<AudioInterface> activeInterfaces;

    bool isPrimaryInterface(audiointerfacetype_t type, void *ptr)
    {
        for (int i = activeInterfaces.size() - 1; i >= 0; --i)
        {
            auto const &intf = activeInterfaces.at(i);
            if (intf.type != type) continue;
            return intf.i.any == ptr;
        }
        return false;
    }

    void addPrimaryInterface(audiointerfacetype_t type, void *ptr)
    {
        if (!isPrimaryInterface(type, ptr)) // check if already there
        {
            AudioInterface ifs; zap(ifs);
            ifs.type  = type;
            ifs.i.any = ptr;

            if (ifs.isDummy())
            {
                // A dummy interface as the primary one removes the need to have any
                // other interfaces of the same type.
                for (int i = activeInterfaces.size() - 1; i >= 0; --i)
                {
                    if (activeInterfaces[i].type == type)
                    {
                        activeInterfaces.removeAt(i);
                    }
                }
            }

            activeInterfaces << ifs;  // a copy is made
        }
    }

    /**
     * Choose the SFX, Music, and CD audio interfaces to use.
     *
     * @param defaultDriverId  Default audio driver to use unless overridden.
     */
    void selectInterfaces(audiodriverid_t defaultDriverId)
    {
        AudioDriver &defaultDriver = driverById(defaultDriverId);

        // The default driver goes on the bottom of the stack.
        if (defaultDriver.hasSfx())
        {
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_ISFX;
            ifs.i.any = &defaultDriver.iSfx();
            activeInterfaces << ifs;  // a copy is made
        }

        if (defaultDriver.hasMusic())
        {
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_IMUSIC;
            ifs.i.any = &defaultDriver.iMusic();
            activeInterfaces << ifs;  // a copy is made
        }
#if defined(MACOSX) && defined(MACOS_HAVE_QTKIT)
        else if (defaultDriverId != AUDIOD_DUMMY)
        {
            // On the Mac, use the built-in QuickTime interface as the fallback for music.
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_IMUSIC;
            ifs.i.any = &audiodQuickTimeMusic;
            activeInterfaces << ifs;  // a copy is made
        }
#endif

        if (defaultDriver.hasCd())
        {
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_ICD;
            ifs.i.any = &defaultDriver.iCd();
            activeInterfaces << ifs;  // a copy is made
        }

        String userSfx   = Config::get("audio.soundPlugin");
        String userMusic = Config::get("audio.musicPlugin");
        String userCD    = Config::get("audio.cdPlugin");

        // Command line options may also be used to specify which plugin to use.
        CommandLine &cmdLine = CommandLine::get();
        if (auto arg = cmdLine.check("-isfx", 1))
        {
            userSfx = arg.params.at(0);
        }
        if (auto arg = cmdLine.check("-imusic", 1))
        {
            userMusic = arg.params.at(0);
        }
        if (auto arg = cmdLine.check("-icd", 1))
        {
            userCD = arg.params.at(0);
        }

        // Activate the user's preferred interfaces.
        {
            AudioDriver &driver = driverById(initDriverIfNeeded(userSfx));
            if (driver.hasSfx())
            {
                addPrimaryInterface(AUDIO_ISFX, &driver.iSfx());
            }
        }
        {
            AudioDriver &driver = driverById(initDriverIfNeeded(userMusic));
            if (driver.hasMusic())
            {
                addPrimaryInterface(AUDIO_IMUSIC, &driver.iMusic());
            }
        }
        {
            AudioDriver &driver = driverById(initDriverIfNeeded(userCD));
            if (driver.hasCd())
            {
                addPrimaryInterface(AUDIO_ICD, &driver.iCd());
            }
        }

        // Let the music driver(s) know of the primary sfx interface, in case they
        // want to play audio through it.
        setMusicProperty(AUDIOP_SFX_INTERFACE, self().sfx());
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
        if (type != AUDIO_INONE)
        {
            for (dint i = activeInterfaces.count(); i--> 0; )
            {
                AudioInterface const &ifs = activeInterfaces[i];
                if (ifs.type == type ||
                   (type == AUDIO_IMUSIC_OR_ICD && (ifs.type == AUDIO_IMUSIC ||
                                                    ifs.type == AUDIO_ICD)))
                {
                    if (auto result = func(ifs.i.any))
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
        if (anyAudioInterface)
        {
            for (AudioDriver const &driver : drivers)
            {
                if ((void *)&driver.iSfx()   == anyAudioInterface ||
                   (void *)&driver.iMusic() == anyAudioInterface ||
                   (void *)&driver.iCd()    == anyAudioInterface)
                {
                    return driver.iBase();
                }
            }
        }
        throw Error("audio::System::getBaseInterface", "Unknown audio interface");
    }

    audiointerfacetype_t interfaceType(void *anyAudioInterface) const
    {
        if (anyAudioInterface)
        {
            for (AudioDriver const &driver : drivers)
            {
                if ((void *)&driver.iSfx()   == anyAudioInterface) return AUDIO_ISFX;
                if ((void *)&driver.iMusic() == anyAudioInterface) return AUDIO_IMUSIC;
                if ((void *)&driver.iCd()    == anyAudioInterface) return AUDIO_ICD;
            }
        }
        return AUDIO_INONE;
    }

    String interfaceName(void *anyAudioInterface) const
    {
        if (anyAudioInterface)
        {
            for (AudioDriver const &driver : drivers)
            {
                String const name = driver.interfaceName(anyAudioInterface);
                if (!name.isEmpty()) return name;
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
        //dint soundId     = 0;
        mobj_t const *emitter = nullptr;
        duint endTime = 0;
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
    world::Subsector *sfxListenerSubsector = nullptr;
    std::unique_ptr<audio::SfxChannels> sfxChannels;
#endif

    audio::SfxSampleCache sfxSampleCache;      ///< @todo should be __CLIENT__ only.
    LogicSoundHash sfxLogicHash;
    duint sfxLogicLastPurge = 0;
    bool sfxLogicOneSoundPerEmitter = false;  ///< set at the start of the frame

    Impl(Public *i) : Base(i)
    {
        theAudioSystem = thisPublic;

        // Script bindings.
        {
            ScriptSystem::get().addNativeModule("Audio", module);
#if defined(__CLIENT__)
            binder.init(module)
                << DENG2_FUNC(Audio_LocalSound, "localSound", "id" << "volume");
#endif
        }

#ifdef __CLIENT__
        DoomsdayApp::app().audienceForGameUnload() += this;
        sfxSampleCache.audienceForSampleRemove() += this;
#endif
    }

    ~Impl()
    {
        sfxClearLogical();
//#ifdef __CLIENT__
//        sfxSampleCache.audienceForSampleRemove() -= this;
//        DoomsdayApp::app().audienceForGameUnload() -= this;
//#endif

        theAudioSystem = nullptr;
    }

#ifdef __CLIENT__
    String composeMusicBufferFilename(String const &ext = "")
    {
        // Switch the name of the buffered song file?
        static dint currentBufFile = 0;
        if (musNeedBufFileSwitch)
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
            if (iBase.Set) iBase.Set(prop, ptr);
            return LoopContinue;
        });

        if (prop == AUDIOP_SOUNDFONT_FILENAME)
        {
            auto *fn = (char const *) ptr;
            if (!fn || !fn[0]) return; // No path.

            if (F_FileExists(fn))
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

        if (virtualOrNativePath.isEmpty())
            return 0;

        // Relative paths are relative to the native working directory.
        String const path  = (NativePath::workPath() / NativePath(virtualOrNativePath).expand()).withSeparators('/');
        LOG_AUDIO_VERBOSE("Attempting to play music file \"%s\"")
            << NativePath(virtualOrNativePath).pretty();

        try
        {
            std::unique_ptr<FileHandle> hndl(&App_FileSystem().openFile(path, "rb"));

            auto didPlay = forAllInterfaces(AUDIO_IMUSIC, [this, &path, &hndl, &looped] (void *ifs)
            {
                auto *iMusic = (audiointerface_music_t *) ifs;

                // Does this interface offer buffered playback?
                if (iMusic->PlayFile)
                {
                    // Write the data to disk and play from there.
                    File &file = FS::rootFolder().replaceFile(
                        composeMusicBufferFilename(path.fileNameExtension()));
                    Block buf(hndl->length());
                    hndl->read(buf.data(), buf.size());
                    file << buf;
                    file.flush();
                    return iMusic->PlayFile(file.as<NativeFile>().nativePath().toUtf8(), looped);
                }
                else if (iMusic->Play && iMusic->SongBuffer)
                {
                    // Buffer the data using the driver's own facility.
                    dsize const len = hndl->length();
                    hndl->read((duint8 *) iMusic->SongBuffer(len), len);

                    return iMusic->Play(looped);
                }
                // Does this interface offer playback from a native file?
                else
                return 0;  // Continue.
            });

            App_FileSystem().releaseFile(hndl->file());
            return didPlay;
        }
        catch (FS1::NotFoundError const &)
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

        if (!App_FileSystem().nameIndex().hasLump(lumpNum))
            return 0;

        File1 &lump = App_FileSystem().lump(lumpNum);
        if (recognizeMus(lump))
        {
            // Lump is in DOOM's MUS format. We must first convert it to MIDI.
            if (!canPlayMUS) return -1;

            File &midi = FS::rootFolder().replaceFile(composeMusicBufferFilename(".mid"));

            // Read the lump, convert to MIDI and output to a temp file in the working directory.
            // Use a filename with the .mid extension so that any player which relies on the it
            // for format recognition works as expected.
            //duint8 *buf = (duint8 *) M_Malloc(lump.size());
            Block buf(lump.size());
            lump.read(buf.data(), 0, lump.size());
            midi << M_Mus2Midi(buf);
            midi.flush();
            //M_Free(buf); buf = nullptr;

            return forAllInterfaces(AUDIO_IMUSIC, [&midi, &looped] (void *ifs)
            {
                auto *iMusic = (audiointerface_music_t *) ifs;
                if (iMusic->PlayFile)
                {
                    return iMusic->PlayFile(midi.as<NativeFile>().nativePath().toUtf8(), looped);
                }
                return 0;  // Continue.
            });
        }

        return forAllInterfaces(AUDIO_IMUSIC, [this, &lump, &looped] (void *ifs)
        {
            auto *iMusic = (audiointerface_music_t *) ifs;

            // Does this interface offer buffered playback?
            if (iMusic->Play && iMusic->SongBuffer)
            {
                // Buffer the data using the driver's own facility.
                std::unique_ptr<FileHandle> hndl(&App_FileSystem().openLump(lump));
                dsize const length  = hndl->length();
                hndl->read((duint8 *) iMusic->SongBuffer(length), length);
                App_FileSystem().releaseFile(hndl->file());

                return iMusic->Play(looped);
            }
            // Does this interface offer playback from a native file?
            else if (iMusic->PlayFile)
            {
                String bufName = composeMusicBufferFilename();
                if (!F_DumpFile(lump, bufName.toUtf8()))
                {
                    // Failed to write the lump...
                    return 0;
                }
                return iMusic->PlayFile(FS::rootFolder().locate<File const>(bufName)
                                        .as<NativeFile>().nativePath().toUtf8(), looped);
            }

            return 0;  // Continue.
        });
    }

    dint playMusicCDTrack(dint track, bool looped)
    {
        // Assume track 0 is not valid.
        if (track == 0) return 0;

        return forAllInterfaces(AUDIO_ICD, [&track, &looped] (void *ifs)
        {
            auto *iCD = (audiointerface_cd_t *) ifs;
            if (iCD->Play)
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
        if (musAvail) return;

        LOG_AUDIO_VERBOSE("Initializing Music subsystem...");

        musAvail       = false;
        musCurrentSong = "";
        musPaused      = false;

        CommandLine &cmdLine = CommandLine::get();
        if (::isDedicated || cmdLine.has("-nomusic"))
        {
            LOG_AUDIO_NOTE("Music disabled");
            return;
        }

        // Initialize interfaces for music playback.
        dint initialized = 0;
        forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [this, &initialized] (void *ifs)
        {
            auto *iMusic = (audiointerface_music_generic_t *) ifs;
            if (iMusic->Init())
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
        if (musAvail)
        {
            // Tell audio drivers about our soundfont config.
            self().updateMusicMidiFont();
        }
    }

    /**
     * Perform deinitialize for music playback.
     */
    void deinitMusic()
    {
        // Already been here?
        if (!musAvail) return;
        musAvail = false;

        // Shutdown interfaces.
        forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
        {
            auto *iMusic = (audiointerface_music_generic_t *) ifs;
            if (iMusic->Shutdown) iMusic->Shutdown();
            return LoopContinue;
        });
    }

    void updateMusicVolumeIfChanged()
    {
        if (!musAvail) return;

        static dint oldMusVolume = -1;
        if (::musVolume != oldMusVolume)
        {
            oldMusVolume = ::musVolume;

            // Set volume of all available interfaces.
            dfloat newVolume = ::musVolume / 255.0f;
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
        if (sfxAvail) return;

        // Check if sound has been disabled with a command line option.
        if (CommandLine::get().has("-nosfx"))
        {
            LOG_AUDIO_NOTE("Sound effects disabled");
            return;
        }

        LOG_AUDIO_VERBOSE("Initializing sound effect playback...");
        // No available interface?
        if (!self().sfx()) return;

        // This is based on the scientific calculations that if the DOOM marine
        // is 56 units tall, 60 is about two meters.
        //// @todo Derive from the viewheight.
        self().sfx()->Listener(SFXLP_UNITS_PER_METER, 30);
        self().sfx()->Listener(SFXLP_DOPPLER, 1.5f);

        // The audio driver is working, let's create the channels.
        initSfxChannels();

        // (Re)Init the sample cache.
        sfxSampleCache.clear();

        // Initialize reverb effects to off.
        sfxListenerNoReverb();

        // Finally, start the sound channel refresh thread. It will stop on its
        // own when it notices that the rest of the sound system is going down.

        refreshing   = false;
        allowRefresh = true;

        dint disableRefresh = false;

        // Nothing to refresh?
        if (!self().sfx()) goto noRefresh;

        if (self().sfx()->Getv)
        {
            self().sfx()->Getv(SFXIP_DISABLE_CHANNEL_REFRESH, &disableRefresh);
        }

        if (!disableRefresh)
        {
            // Start the refresh thread. It will run until the Sfx module is shut down.
            refreshHandle = Sys_StartThread(sfxChannelRefreshThread, nullptr, nullptr);
            if (!refreshHandle)
            {
                throw Error("audio::System::initSfx", "Failed to start refresh thread");
            }
        }
        else
        {
    noRefresh:
            LOGDEV_AUDIO_NOTE("Audio driver does not require a refresh thread");
        }

        // The Sfx module is now available.
        sfxAvail = true;
    }

    /**
     * Perform deinitialization for sound effect playback.
     */
    void deinitSfx()
    {
        // Not initialized?
        if (!sfxAvail) return;

        // These will stop further refreshing.
        sfxAvail     = false;
        allowRefresh = false;

        if (refreshHandle)
        {
            // Wait for the sfx refresh thread to stop.
            Sys_WaitThread(refreshHandle, 2000, nullptr);
            refreshHandle = nullptr;
        }

        // Clear the sample cache.
        sfxSampleCache.clear();

        // Destroy channels.
        shutdownSfxChannels();
    }

    /**
     * The specified sample will soon no longer exist. All channel buffers
     * loaded with the sample will be reset.
     */
    void unloadSoundID(dint id)
    {
        if (!sfxAvail) return;

        self().allowSfxRefresh(false);
        sfxChannels->forAll([this, &id] (audio::SfxChannel &ch)
        {
            if (ch.hasBuffer())
            {
                sfxbuffer_t &sbuf = ch.buffer();
                if (sbuf.sample && sbuf.sample->id == id)
                {
                    // Stop and unload.
                    self().sfx()->Reset(&sbuf);
                }
            }
            return LoopContinue;
        });
        self().allowSfxRefresh(true);
    }

    /**
     * Stop all channels and destroy their buffers.
     */
    void destroySfxChannels()
    {
        self().allowSfxRefresh(false);
        sfxChannels->forAll([this] (audio::SfxChannel &ch)
        {
            ch.stop();
            if (ch.hasBuffer())
            {
                self().sfx()->Destroy(&ch.buffer());
                ch.setBuffer(nullptr);
            }
            return LoopContinue;
        });
        self().allowSfxRefresh(true);
    }

    void createSfxChannels()
    {
        if (!bool( sfxChannels )) return; // Huh?

        dint num2D = sfx3D ? SOUND_CHANNEL_2DCOUNT: sfxChannels->count();  // The rest will be 3D.
        dint bits  = sfxBits;
        dint rate  = sfxRate;

        // Change the primary buffer format to match the channel format.
        dfloat parm[2] = { dfloat(bits), dfloat(rate) };
        self().sfx()->Listenerv(SFXLP_PRIMARY_FORMAT, parm);

        // Create sample buffers for the channels.
        dint idx = 0;
        sfxChannels->forAll([this, &num2D, &bits, &rate, &idx] (audio::SfxChannel &ch)
        {
            ch.setBuffer(self().sfx()->Create(num2D-- > 0 ? 0 : SFXBF_3D, bits, rate));
            if (!ch.hasBuffer())
            {
                LOG_AUDIO_WARNING("Failed to create sample buffer for #%i") << idx;
            }
            idx += 1;
            return LoopContinue;
        });
    }

    // Create channels according to the current mode.
    void initSfxChannels()
    {
        // The -sfxchan option can be used to change the number of channels.
        if (CommandLine_CheckWith("-sfxchan", 1))
        {
            Config::get().set("audio.channels", String(CommandLine_Next()).toInt());

            //numChannels = de::clamp(1, String(CommandLine_Next()).toInt(), SOUND_CHANNEL_COUNT_MAX);
        }

        dint numChannels = Config::get().geti("audio.channels", SOUND_CHANNEL_COUNT_DEFAULT);
        numChannels = Rangei(1, SOUND_CHANNEL_COUNT_MAX).clamp(numChannels);

        LOG_AUDIO_NOTE("Initializing %i sound effect channels") << numChannels;

        // Allocate and init the channels.
        sfxChannels.reset(new audio::SfxChannels(numChannels));
        createSfxChannels();
    }

    /**
     * Frees all memory allocated for the channels.
     */
    void shutdownSfxChannels()
    {
        destroySfxChannels();
        sfxChannels.reset();
    }

    /**
     * Destroys all channels and creates them again.
     */
    void recreateSfxChannels()
    {
        destroySfxChannels();
        createSfxChannels();
    }

    void getSfxChannelPriorities(dfloat *prios) const
    {
        if (!prios) return;

        dint idx = 0;
        sfxChannels->forAll([&prios, &idx] (audio::SfxChannel &ch)
        {
            prios[idx++] = ch.priority();
            return LoopContinue;
        });
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
        if (nowTime - sfxLogicLastPurge < SOUND_LOGICAL_PURGEINTERVAL) return;

        // Peform the purge now.
//        LOGDEV_AUDIO_XVERBOSE("purging logic sound hash...", "");
        sfxLogicLastPurge = nowTime;

        // Check all sounds in the hash.
        MutableLogicSoundHashIterator it(sfxLogicHash);
        while (it.hasNext())
        {
            it.next();
            LogicSound &lsound = *it.value();
            if (!lsound.isRepeating && lsound.endTime < nowTime)
            {
                // This has stopped.
                delete &lsound;
                it.remove();
            }
        }
    }

    /**
     * The sound is removed from the list of playing sounds. Called whenever
     * a sound is stopped, regardless of whether it was actually playing on
     * the local system.
     *
     * @note If @a soundId == 0 and @a emitter == nullptr then stop everything.
     *
     * @return  Number of sounds stopped.
     */
    dint sfxStopLogical(dint soundId, mobj_t const *emitter)
    {
        dint stopCount = 0;
        MutableLogicSoundHashIterator it(sfxLogicHash);
        while (it.hasNext())
        {
            it.next();

            LogicSound const &lsound = *it.value();
            if (soundId)
            {
                if (it.key() != soundId) continue;
            }
            else if (emitter)
            {
                if (lsound.emitter != emitter) continue;
            }

            delete &lsound;
            it.remove();
            stopCount++;
        }
        return stopCount;
    }

    /**
     * The sound is entered into the list of playing sounds. Called when a
     * 'world class' sound is started, regardless of whether it's actually
     * started on the local system.
     *
     * @todo Why does the Server cache sound samples and/or care to know the
     * length of the samples? It is entirely possible that the Client is using
     * a different set of samples so using this information on server side (for
     * scheduling of remote playback events?) is not logical. -ds
     */
    void sfxStartLogical(dint soundIdAndFlags, mobj_t const *emitter)
    {
        if (soundIdAndFlags <= 0) return;

        dint const soundId = (soundIdAndFlags & ~DDSF_FLAG_MASK);

        // Cache the sound sample associated with @a soundId (if necessary)
        // so that we can determine it's length.
        if (sfxsample_t *sample = sfxSampleCache.cache(soundId))
        {
            bool const isRepeating = (soundIdAndFlags & DDSF_REPEAT) ||
                                     Def_SoundIsRepeating(soundId);

            duint length = (1000 * sample->numSamples) / sample->rate;
            if (isRepeating && length > 1)
            {
                length = 1;
            }

            // Ignore zero length sounds.
            /// @todo Shouldn't we still stop others though? -ds
            if (!length) return;

            // Only one sound per emitter?
            if (emitter && sfxLogicOneSoundPerEmitter)
            {
                // Stop all other sounds.
                sfxStopLogical(0, emitter);
            }

            auto *ls = new Impl::LogicSound;
            //ls->soundId     = soundId;
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
        if (!sectorEmitter || !flags) return;

        // Are we stopping with this sector's emitter?
        if (flags & SSF_SECTOR)
        {
            self().stopSound(soundId, (mobj_t *)sectorEmitter);
        }

        // Are we stopping with linked emitters?
        if (!(flags & SSF_SECTOR_LINKED_SURFACES)) return;

        // Process the rest of the emitter chain.
        ddmobj_base_t *base = sectorEmitter;
        while ((base = (ddmobj_base_t *)base->thinker.next))
        {
            // Stop sounds from this emitter.
            self().stopSound(soundId, (mobj_t *)base);
        }
    }

#ifdef __CLIENT__

    /**
     * Returns the 3D position of the sound effect listener, in map space.
     */
    Vector3d getSfxListenerOrigin() const
    {
        if (sfxListener)
        {
            auto origin = Vector3d(sfxListener->origin);
            origin.z += sfxListener->height - 5;  /// @todo Make it exactly eye-level! (viewheight).
            return origin;
        }
        return Vector3d();
    }

    void sfxListenerNoReverb()
    {
        if (!sfxAvail) return;

        sfxListenerSubsector = nullptr;

        dfloat rev[4] = { 0, 0, 0, 0 };
        self().sfx()->Listenerv(SFXLP_REVERB, rev);
        self().sfx()->Listener(SFXLP_UPDATE, 0);
    }

    void updateSfxListener()
    {
        if (!sfxAvail || !sfx3D) return;

        // No volume means no sound.
        if (!::sfxVolume) return;

        // Update the listener mobj.
        self().setSfxListener(S_GetListenerMobj());
        if (sfxListener)
        {
            {
                // Origin. At eye-level.
                auto const origin = Vector4f(getSfxListenerOrigin().toVector3f(), 0);
                dfloat vec[4];
                origin.decompose(vec);
                self().sfx()->Listenerv(SFXLP_POSITION, vec);
            }
            {
                // Orientation. (0,0) will produce front=(1,0,0) and up=(0,0,1).
                dfloat vec[2] = {
                    sfxListener->angle / (dfloat) ANGLE_MAX * 360,
                    (sfxListener->dPlayer ? LOOKDIR2DEG(sfxListener->dPlayer->lookDir) : 0)
                };
                self().sfx()->Listenerv(SFXLP_ORIENTATION, vec);
            }
            {
                // Velocity. The unit is world distance units per second
                auto const velocity = Vector4f(Vector3d(sfxListener->mom).toVector3f(), 0) * TICSPERSEC;
                dfloat vec[4];
                velocity.decompose(vec);
                self().sfx()->Listenerv(SFXLP_VELOCITY, vec);
            }

            // Reverb effects. Has the current subsector changed?
            world::Subsector *newSubsector = Mobj_SubsectorPtr(*sfxListener);
            if (newSubsector && (!sfxListenerSubsector || sfxListenerSubsector != newSubsector))
            {
                sfxListenerSubsector = newSubsector;

                // It may be necessary to recalculate the reverb properties...
                world::ClientSubsector::AudioEnvironment const &aenv = sfxListenerSubsector->as<world::ClientSubsector>().reverb();

                dfloat args[NUM_REVERB_DATA];
                args[SFXLP_REVERB_VOLUME ] = aenv.volume * sfxReverbStrength;
                args[SFXLP_REVERB_SPACE  ] = aenv.space;
                args[SFXLP_REVERB_DECAY  ] = aenv.decay;
                args[SFXLP_REVERB_DAMPING] = aenv.damping;

                self().sfx()->Listenerv(SFXLP_REVERB, args);
            }
        }

        // Update all listener properties.
        self().sfx()->Listener(SFXLP_UPDATE, 0);
    }

    void updateSfx3DModeIfChanged()
    {
        static dint old3DMode = false;

        if (old3DMode == sfx3D) return;  // No change.

        LOG_AUDIO_VERBOSE("Switching to %s mode...") << (old3DMode ? "2D" : "3D");

        // To make the change effective, re-create all channels.
        recreateSfxChannels();

        if (old3DMode)
        {
            // Going 2D - ensure reverb is disabled.
            sfxListenerNoReverb();
        }
        old3DMode = sfx3D;
    }

#if 0
    void updateSfxSampleRateIfChanged()
    {
        static dint old16Bit = false;
        static dint oldRate  = 11025;

        // Ensure the rate is valid.
        if (sfxSampleRate != 11025 && sfxSampleRate != 22050 && sfxSampleRate != 44100)
        {
            LOG_AUDIO_WARNING("\"sound-rate\" corrected to 11025 from invalid value (%i)") << sfxSampleRate;
            sfxSampleRate = 11025;
        }

        // Do we need to change the sample format?
        if (old16Bit != sfx16Bit || oldRate != sfxSampleRate)
        {
            dint const newBits = sfx16Bit ? 16 : 8;
            dint const newRate = sfxSampleRate;
            if (::sfxBits != newBits || ::sfxRate != newRate)
            {
                LOG_AUDIO_VERBOSE("Switching sound rate to %iHz (%i-bit)..") << newRate << newBits;

                // Set the new buffer format.
                ::sfxBits = newBits;
                ::sfxRate = newRate;
                recreateSfxChannels();

                // The cache just became useless, clear it.
                sfxSampleCache.clear();
            }
            old16Bit = sfx16Bit;
            oldRate  = sfxSampleRate;
        }
    }
#endif

    void sfxSampleCacheAboutToRemove(sfxsample_t const &sample)
    {
        // Reset all channels loaded with the sample data and stop all sounds using
        // this sample (the sample data will be gone soon).
        unloadSoundID(sample.id);
    }
#endif  // __CLIENT__

    void reset()
    {
#ifdef __CLIENT__
        self().reset();
#endif
        sfxClearLogical();
    }

    void aboutToUnloadGame(Game const &)
    {
        reset();
    }
};

AudioSystem::AudioSystem() : d(new Impl(this))
{}

AudioSystem &AudioSystem::get()
{
    DENG2_ASSERT(theAudioSystem);
    return *theAudioSystem;
}

void AudioSystem::timeChanged(Clock const &)
{
    // Nothing to do.
}

void AudioSystem::reinitialize()
{
    LOG_AS("AudioSystem");
    LOG_AUDIO_NOTE("Reinitializing all audio interfaces...");

    d->reset();
#ifdef __CLIENT__
    deinitPlayback();
    initPlayback();
#endif
}

String AudioSystem::description() const
{
#define TABBED(A, B)  _E(Ta) "  " _E(l) A _E(.) " " _E(Tb) << (B) << "\n"

    String str;
    QTextStream os(&str);

    os << _E(b) "Audio configuration:\n" _E(.);

#if 0
    /// @todo When a game is loaded, these could be included under an additional
    /// "Music preferences" heading. -jk
    os << TABBED("Music volume:",  musVolume);
#ifdef __CLIENT__
    String const midiFontPath(musMidiFontPath);
    os << TABBED("Music sound font:", midiFontPath.isEmpty() ? "None" : midiFontPath);
    os << TABBED("Music source preference:", musicSourceAsText(musSourcePreference));
#endif
#endif

#ifdef __CLIENT__
    int ifCounts[AUDIO_INTERFACE_COUNT] {};

    // Include an active playback interface itemization.
    for (dint i = d->activeInterfaces.count(); i-- > 0; )
    {
        Impl::AudioInterface const &ifs = d->activeInterfaces[i];

        String ifName = (ifs.type == AUDIO_IMUSIC? "Music" :
                         ifs.type == AUDIO_ISFX?   "SFX" : "CD");
        if (++ifCounts[ifs.type] > 1)
        {
            ifName += String(" %1").arg(ifCounts[ifs.type]);
        }

        os << _E(Ta) _E(l) "  " << ifName << ": " << _E(.) _E(Tb)
           << d->interfaceName(ifs.i.any) << "\n";

        /*}
        else if (ifs.type == AUDIO_ISFX)
        {
            os << _E(Ta) _E(l) << "  SFX: " << _E(.) _E(Tb) << d->interfaceName(ifs.i.sfx) << "\n";
        }*/
    }
#endif

    return str.rightStrip();

#undef TABBED
}

#ifdef __CLIENT__
void AudioSystem::reset()
{
    LOG_AS("AudioSystem");
    LOG_AUDIO_VERBOSE("Reseting...");

    if (d->sfxAvail)
    {
        d->sfxListenerSubsector = nullptr;

        // Stop all channels.
        d->sfxChannels->forAll([] (audio::SfxChannel &ch)
        {
            ch.stop();
            return LoopContinue;
        });

        // Clear the sample cache.
        d->sfxSampleCache.clear();
    }

    stopMusic();
}
#endif

/**
 * @todo Do this in timeChanged()
 */
void AudioSystem::startFrame()
{
    LOG_AS("AudioSystem");

#ifdef __CLIENT__
    d->updateMusicVolumeIfChanged();

    if (sfxIsAvailable())
    {
        // Update all channels (freq, 2D:pan,volume, 3D:position,velocity).

        // Update the active interface.
        d->getBaseInterface(sfx()).Event(SFXEV_BEGIN);

        // Have there been changes to the cvar settings?
        d->updateSfx3DModeIfChanged();
        //d->updateSfxSampleRateIfChanged();

        // Should we purge the cache (to conserve memory)?
        d->sfxSampleCache.maybeRunPurge();
    }

    if (d->musAvail)
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
void AudioSystem::endFrame()
{
    LOG_AS("AudioSystem");

    if (sfxIsAvailable())
    {
        if (!BusyMode_Active())
        {
            // Update channel and listener properties.

            // If no listener is available - no 3D positioning is done.
            d->sfxListener = S_GetListenerMobj();

            // Update channels.
            d->sfxChannels->forAll([] (audio::SfxChannel &ch)
            {
                if (ch.hasBuffer() && (ch.buffer().flags & SFXBF_PLAYING))
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
#endif

void AudioSystem::initPlayback()
{
    LOG_AS("AudioSystem");

    CommandLine &cmdLine = CommandLine::get();
    if (cmdLine.has("-nosound") || cmdLine.has("-noaudio"))
        return;

#ifdef __CLIENT__
    LOG_AUDIO_VERBOSE("Initializing for playback...");

    // Disable random pitch changes?
    sfxNoRndPitch = cmdLine.has("-norndpitch");

    // Try to load the audio driver plugin(s).
    if (d->loadDrivers())
    {
        // Init for sound effects.
        try
        {
            d->initSfx();
        }
        catch (Error const &er)
        {
            LOG_AUDIO_NOTE("Failed initializing playback for sound effects:\n")
                << er.asText();
        }

        // Init for music.
        try
        {
            d->initMusic();
        }
        catch (Error const &er)
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
#endif
}

#ifdef __CLIENT__

void AudioSystem::deinitPlayback()
{
    LOG_AS("AudioSystem");

    d->deinitSfx();
    d->deinitMusic();

    d->unloadDrivers();
}

String AudioSystem::musicSourceAsText(MusicSource source)  // static
{
    static char const *sourceNames[3] = {
        /* MUSP_MUS */ "MUS lumps",
        /* MUSP_EXT */ "External files",
        /* MUSP_CD */  "CD",
    };
    if (source >= MUSP_MUS && source <= MUSP_CD)
        return sourceNames[dint( source )];
    return "(invalid)";
}

bool AudioSystem::musicIsAvailable() const
{
    return d->musAvail;
}

bool AudioSystem::musicIsPlaying() const
{
    //LOG_AS("AudioSystem");
    return d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_t *) ifs;
        return iMusic->gen.Get(MUSIP_PLAYING, nullptr);
    });
}

void AudioSystem::stopMusic()
{
    if (!d->musAvail) return;

    LOG_AS("AudioSystem");
    d->musCurrentSong = "";

    // Stop all interfaces.
    d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        iMusic->Stop();
        return LoopContinue;
    });
}

void AudioSystem::pauseMusic(bool doPause)
{
    if (!d->musAvail) return;

    LOG_AS("AudioSystem");
    d->musPaused = !d->musPaused;

    // Pause playback on all interfaces.
    d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [&doPause] (void *ifs)
    {
        auto *iMusic = (audiointerface_music_generic_t *) ifs;
        iMusic->Pause(doPause);
        return LoopContinue;
    });
}

bool AudioSystem::musicIsPaused() const
{
    return d->musPaused;
}

dint AudioSystem::playMusic(Record const &definition, bool looped)
{
    if (!d->musAvail) return false;

    LOG_AS("AudioSystem");
    LOG_AUDIO_MSG("Starting music \"%s\"%s") << definition.gets("id") << (looped ? " looped" : "");
    //LOG_AUDIO_VERBOSE("Current song '%s'") << d->musCurrentSong;

    // We will not restart the currently playing song.
    if (definition.gets("id") == d->musCurrentSong && musicIsPlaying())
    {
        // This is not a failure, though, since the right music is played.
        return true;
    }

    // Stop the currently playing song.
    stopMusic();

    // Switch to an unused file buffer if asked.
    d->musNeedBufFileSwitch = true;

    // This is the song we're playing now.
    d->musCurrentSong = definition.gets("id");

    // Determine the music source, order preferences.
    dint source[3];
    source[0] = musSourcePreference;
    switch (musSourcePreference)
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
    for (dint i = 0; i < 3; ++i)
    {
        bool canPlayMUS = true;

        switch (source[i])
        {
        case MUSP_CD:
            if (cd())
            {
                const int cdTrack = defn::Music(definition).cdTrack();
                if (d->playMusicCDTrack(cdTrack, looped))
                {
                    LOG_AUDIO_VERBOSE("Playing CD track %d") << cdTrack;
                    return true;
                }
            }
            break;

        case MUSP_EXT:
            if (d->playMusicFile(App_Resources().tryFindMusicFile(definition), looped))
            {
                LOG_AUDIO_VERBOSE("Playing external music file \"%s\"")
                        << definition.gets("path");
                return true;
            }

            // Next, try non-MUS lumps.
            canPlayMUS = false;

        // fall through

        case MUSP_MUS:
        {
            const String lump = definition.gets("lumpName");
            if (d->playMusicLump(App_FileSystem().lumpNumForName(lump), looped, canPlayMUS) == 1)
            {
                LOG_AUDIO_VERBOSE("Playing music lump \"%s\"") << lump;
                return true;
            }
            break;
        }

        default: DENG2_ASSERT(!"Mus_Start: Invalid value for order[i]"); break;
        }
    }

    // No song was started.
    return false;
}

dint AudioSystem::playMusicLump(lumpnum_t lumpNum, bool looped)
{
    stopMusic();
    LOG_AS("AudioSystem");
    return d->playMusicLump(lumpNum, looped);
}

dint AudioSystem::playMusicFile(String const &filePath, bool looped)
{
    stopMusic();
    LOG_AS("AudioSystem");
    return d->playMusicFile(filePath, looped);
}

dint AudioSystem::playMusicCDTrack(dint cdTrack, bool looped)
{
    stopMusic();
    LOG_AS("AudioSystem");
    return d->playMusicCDTrack(cdTrack, looped);
}

void AudioSystem::updateMusicMidiFont()
{
    LOG_AS("AudioSystem");

    NativePath path(musMidiFontPath);
    if (path.isEmpty())
    {
        // The bootstrap script copies the default GeneralUser GS soundfont from the
        // client's package so it can be loaded by FluidSynth.
        path = App::app().nativeHomePath()/"cache/default.sf2";
    }
    d->setMusicProperty(AUDIOP_SOUNDFONT_FILENAME, path.expand().toString().toUtf8().constData());
}

bool AudioSystem::sfxIsAvailable() const
{
    return d->sfxAvail;
}

bool AudioSystem::mustUpsampleToSfxRate() const
{
    dint anyRateAccepted = 0;
    if (sfx()->Getv)
    {
        sfx()->Getv(SFXIP_ANY_SAMPLE_RATE_ACCEPTED, &anyRateAccepted);
    }
    return (anyRateAccepted ? false : true);
}

mobj_t *AudioSystem::sfxListener()
{
    return d->sfxListener;
}

void AudioSystem::setSfxListener(mobj_t *newListener)
{
    d->sfxListener = newListener;
}

#endif  // __CLIENT__

bool AudioSystem::soundIsPlaying(dint soundId, mobj_t *emitter) const
{
    //LOG_AS("AudioSystem");

    // Use the logic sound hash to determine whether the referenced sound is being
    // played currently. We don't care whether its audible or not.
    duint const nowTime = Timer_RealMilliseconds();
    if (soundId)
    {
        auto it = d->sfxLogicHash.constFind(soundId);
        while (it != d->sfxLogicHash.constEnd() && it.key() == soundId)
        {
            Impl::LogicSound const &lsound = *it.value();
            if (lsound.emitter == emitter && lsound.isPlaying(nowTime))
                return true;

            ++it;
        }
    }
    else if (emitter)
    {
        // Check if the emitter is playing any sound.
        auto it = d->sfxLogicHash.constBegin();
        while (it != d->sfxLogicHash.constEnd())
        {
            Impl::LogicSound const &lsound = *it.value();
            if (lsound.emitter == emitter && lsound.isPlaying(nowTime))
                return true;

            ++it;
        }
    }
    return false;

#if 0
    // Use the sound channels to determine whether the referenced sound is actually
    // being played currently.
    if (!d->sfxAvail) return false;

    return d->sfxChannels->forAll([&id, &emitter] (audio::SfxChannel &ch)
    {
        if (ch.hasSample())
        {
            sfxbuffer_t &sbuf = ch.sample();

            if (!(sbuf.flags & SFXBF_PLAYING) ||
               ch.emitter() != emitter ||
               id && sbuf.sample->id != id)
            {
                return LoopContinue;
            }

            // Once playing, repeating sounds don't stop.
            if (sbuf.flags & SFXBF_REPEAT)
                return LoopAbort;

            // Check time. The flag is updated after a slight delay (only at refresh).
            if (Sys_GetTime() - ch->startTime() < sbuf.sample->numsamples / (dfloat) sbuf.freq * TICSPERSEC)
            {
                return LoopAbort;
            }
        }
        return LoopContinue;
    });
#endif
}

#ifdef __CLIENT__

void AudioSystem::stopSoundGroup(dint group, mobj_t const *emitter)
{
    if (!d->sfxAvail) return;
    LOG_AS("AudioSystem");
    d->sfxChannels->forAll([this, &group, &emitter] (audio::SfxChannel &ch)
    {
        if (ch.hasBuffer())
        {
            sfxbuffer_t &sbuf = ch.buffer();
            if ((sbuf.flags & SFXBF_PLAYING) &&
               (sbuf.sample->group == group && (!emitter || ch.emitter() == emitter)))
            {
                // This channel must stop.
                sfx()->Stop(&sbuf);
            }
        }
        return LoopContinue;
    });
}

dint AudioSystem::stopSoundWithLowerPriority(dint id, mobj_t const *emitter, dint defPriority)
{
    if (!d->sfxAvail) return false;

    LOG_AS("AudioSystem");
    dint stopCount = 0;
    d->sfxChannels->forAll([this, &id, &emitter, &defPriority, &stopCount] (audio::SfxChannel &ch)
    {
        if (!ch.hasBuffer()) return LoopContinue;
        sfxbuffer_t &sbuf = ch.buffer();

        if (!(sbuf.flags & SFXBF_PLAYING) || (id && sbuf.sample->id != id) ||
           (emitter && ch.emitter() != emitter))
        {
            return LoopContinue;
        }

        // Can it be stopped?
        if (sbuf.flags & SFXBF_DONT_STOP)
        {
            // The emitter might get destroyed...
            ch.setEmitter(nullptr);
            ch.setFlags(ch.flags() | (SFXCF_NO_UPDATE | SFXCF_NO_ORIGIN));
            return LoopContinue;
        }

        // Check the priority.
        if (defPriority >= 0)
        {
            dint oldPrio = DED_Definitions()->sounds[sbuf.sample->id].priority;
            if (oldPrio < defPriority)  // Old is more important.
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

void AudioSystem::stopSound(dint soundId, mobj_t const *emitter, dint flags)
{
    LOG_AS("AudioSystem");

    // Are we performing any special stop behaviors?
    if (emitter && flags)
    {
        if (emitter->thinker.id)
        {
            // Emitter is a real Mobj.
            d->stopSectorSounds(&Mobj_Sector(emitter)->soundEmitter(), soundId, flags);
            return;
        }

        // The head of the chain is the sector. Find it.
        while (emitter->thinker.prev)
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
    if (d->sfxStopLogical(soundId, emitter))
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
dint AudioSystem::playSound(sfxsample_t *sample, dfloat volume, dfloat freq, mobj_t const *emitter,
    coord_t *fixedOrigin, dint flags)
{
    DENG2_ASSERT(sample);
    if (!d->sfxAvail) return false;

    bool const play3D = sfx3D && (emitter || fixedOrigin);

    LOG_AS("AudioSystem");
    if (sample->id < 1 || sample->id >= DED_Definitions()->sounds.size()) return false;
    if (volume <= 0 || !sample->size) return false;

    if (emitter && sfxOneSoundPerEmitter)
    {
        // Stop any other sounds from the same emitter.
        if (stopSoundWithLowerPriority(0, emitter, DED_Definitions()->sounds[sample->id].priority) < 0)
        {
            // Something with a higher priority is playing, can't start now.
            LOG_AUDIO_MSG("Not playing soundId:%i (prio:%i) because overridden (emitter id:%i)")
                << sample->id
                << DED_Definitions()->sounds[sample->id].priority
                << emitter->thinker.id;
            return false;
        }
    }

    // Calculate the new sound's priority.
    dint const nowTime  = Timer_Ticks();
    dfloat const myPrio = rateSoundPriority(emitter, fixedOrigin, volume, nowTime);

    bool haveChannelPrios = false;
    dfloat channelPrios[256/*MAX_CHANNEL_COUNT*/];
    dfloat lowPrio = 0;

    // Ensure there aren't already too many channels playing this sample.
    sfxinfo_t *info = &::runtimeDefs.sounds[sample->id];
    if (info->channels > 0)
    {
        // The decision to stop channels is based on priorities.
        d->getSfxChannelPriorities(channelPrios);
        haveChannelPrios = true;

        dint count = d->sfxChannels->countPlaying(sample->id);
        while (count >= info->channels)
        {
            // Stop the lowest priority sound of the playing instances, again
            // noting sounds that are more important than us.
            dint idx = 0;
            audio::SfxChannel *selCh = nullptr;
            d->sfxChannels->forAll([&sample, &myPrio, &channelPrios,
                                    &selCh, &lowPrio, &idx] (audio::SfxChannel &ch)
            {
                dfloat const chPriority = channelPrios[idx++];

                if (ch.hasBuffer())
                {
                    sfxbuffer_t &sbuf = ch.buffer();
                    if ((sbuf.flags & SFXBF_PLAYING))
                    {
                        DENG2_ASSERT(sbuf.sample != nullptr);

                        if (sbuf.sample->id == sample->id &&
                           (myPrio >= chPriority && (!selCh || chPriority <= lowPrio)))
                        {
                            selCh   = &ch;
                            lowPrio = chPriority;
                        }
                    }
                }

                return LoopContinue;
            });

            if (!selCh)
            {
                // The new sound can't be played because we were unable to stop
                // enough channels to accommodate the limitation.
                LOG_AUDIO_XVERBOSE("Not playing soundId:%i because all channels are busy",
                                   sample->id);
                return false;
            }

            // Stop this one.
            count--;
            selCh->stop();
        }
    }

    // Hit count tells how many times the cached sound has been used.
    d->sfxSampleCache.hit(sample->id);

    /*
     * Pick a channel for the sound. We will do our best to play the sound,
     * cancelling existing ones if need be. The ideal choice is a free channel
     * that is already loaded with the sample, in the correct format and mode.
     */
    allowSfxRefresh(false);

    // First look through the stopped channels. At this stage we're very picky:
    // only the perfect choice will be good enough.
    audio::SfxChannel *selCh = d->sfxChannels->tryFindVacant(play3D, sample->bytesPer,
                                                             sample->rate, sample->id);

    if (!selCh)
    {
        // Perhaps there is a vacant channel (with any sample, but preferably one
        // with no sample already loaded).
        selCh = d->sfxChannels->tryFindVacant(play3D, sample->bytesPer, sample->rate, 0);
    }

    if (!selCh)
    {
        // Try any non-playing channel in the correct format.
        selCh = d->sfxChannels->tryFindVacant(play3D, sample->bytesPer, sample->rate, -1);
    }

    if (!selCh)
    {
        // A perfect channel could not be found.
        // We must use a channel with the wrong format or decide which one of the
        // playing ones gets stopped.

        if (!haveChannelPrios)
        {
            d->getSfxChannelPriorities(channelPrios);
        }

        // All channels with a priority less than or equal to ours can be stopped.
        audio::SfxChannel *prioCh = nullptr;
        dint idx = 0;
        d->sfxChannels->forAll([&play3D, &myPrio, &channelPrios,
                                &selCh, &prioCh, &lowPrio, &idx] (audio::SfxChannel &ch)
        {
            dfloat const chPriority = channelPrios[idx++];

            if (ch.hasBuffer())
            {
                // Sample buffer must be configured for the right mode.
                sfxbuffer_t &sbuf = ch.buffer();
                if (play3D == ((sbuf.flags & SFXBF_3D) != 0))
                {
                    if (!(sbuf.flags & SFXBF_PLAYING))
                    {
                        // This channel is not playing, we'll take it!
                        selCh = &ch;
                        return LoopAbort;
                    }

                    // Are we more important than this sound?
                    // We want to choose the lowest priority sound.
                    if (myPrio >= chPriority && (!prioCh || chPriority <= lowPrio))
                    {
                        prioCh  = &ch;
                        lowPrio = chPriority;
                    }
                }
            }

            return LoopContinue;
        });

        // If a good low-priority channel was found, use it.
        if (prioCh)
        {
            selCh = prioCh;
            selCh->stop();
        }
    }

    if (!selCh)
    {
        // A suitable channel was not found.
        allowSfxRefresh(true);
        LOG_AUDIO_XVERBOSE("Failed to find suitable channel for sample id:%i", sample->id);
        return false;
    }

    DENG2_ASSERT(selCh->hasBuffer());
    // The sample buffer may need to be reformatted.

    if (selCh->buffer().rate  != sample->rate ||
       selCh->buffer().bytes != sample->bytesPer)
    {
        // Create a new sample buffer with the correct format.
        sfx()->Destroy(&selCh->buffer());
        selCh->setBuffer(sfx()->Create(play3D ? SFXBF_3D : 0, sample->bytesPer * 8, sample->rate));
    }
    sfxbuffer_t &sbuf = selCh->buffer();

    // Configure buffer flags.
    sbuf.flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);
    if (flags & SF_REPEAT)    sbuf.flags |= SFXBF_REPEAT;
    if (flags & SF_DONT_STOP) sbuf.flags |= SFXBF_DONT_STOP;

    // Init the channel information.
    selCh->setFlags(selCh->flags() & ~(SFXCF_NO_ORIGIN | SFXCF_NO_ATTENUATION | SFXCF_NO_UPDATE));
    selCh->setVolume(volume);
    selCh->setFrequency(freq);

    if (!emitter && !fixedOrigin)
    {
        selCh->setFlags(selCh->flags() | SFXCF_NO_ORIGIN);
        selCh->setEmitter(nullptr);
    }
    else
    {
        selCh->setEmitter(emitter);
        if (fixedOrigin)
        {
            selCh->setFixedOrigin(Vector3d(fixedOrigin));
        }
    }

    if (flags & SF_NO_ATTENUATION)
    {
        // The sound can be heard from any distance.
        selCh->setFlags(selCh->flags() | SFXCF_NO_ATTENUATION);
    }

    /**
     * Load in the sample. Must load prior to setting properties, because the audio driver
     * might actually create the real buffer only upon loading.
     *
     * @note The sample is not reloaded if a sample with the same ID is already loaded on
     * the channel.
     */
    if (!sbuf.sample || sbuf.sample->id != sample->id)
    {
        sfx()->Load(&sbuf, sample);
    }

    // Update channel properties.
    selCh->updatePriority();

    // 3D sounds need a few extra properties set up.
    if (play3D)
    {
        // Init the buffer's min/max distances.
        // This is only done once, when the sound is started (i.e., here).
        dfloat const minDist = (selCh->flags() & SFXCF_NO_ATTENUATION) ? 10000 : ::soundMinDist;
        dfloat const maxDist = (selCh->flags() & SFXCF_NO_ATTENUATION) ? 20000 : ::soundMaxDist;

        sfx()->Set(&sbuf, SFXBP_MIN_DISTANCE, minDist);
        sfx()->Set(&sbuf, SFXBP_MAX_DISTANCE, maxDist);
    }

    // This'll commit all the deferred properties.
    sfx()->Listener(SFXLP_UPDATE, 0);

    // Start playing.
    sfx()->Play(&sbuf);

    allowSfxRefresh();

    // Take note of the start time.
    selCh->setStartTime(nowTime);

    // Sound successfully started.
    return true;
}

dfloat AudioSystem::rateSoundPriority(mobj_t const *emitter, coord_t const *point, dfloat volume,
    dint startTic)
{
    // In five seconds all priority of a sound is gone.
    dfloat const timeoff  = 1000 * (Timer_Ticks() - startTic) / (5.0f * TICSPERSEC);

    if (!d->sfxListener || (!emitter && !point))
    {
        // The sound does not have an origin.
        return 1000 * volume - timeoff;
    }

    // The sound has an origin, base the points on distance.
    coord_t const *origin;
    if (emitter)
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

audiointerface_sfx_generic_t *AudioSystem::sfx() const
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

audiointerface_cd_t *AudioSystem::cd() const
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

audiodriverid_t AudioSystem::toDriverId(AudioDriver const *driver) const
{
    if (driver && driver >= &d->drivers[0] && driver <= &d->drivers[AUDIODRIVER_COUNT])
    {
        return audiodriverid_t( driver - d->drivers );
    }
    return AUDIOD_INVALID;
}
#endif

audio::SfxSampleCache /*const*/ &AudioSystem::sfxSampleCache() const
{
    return d->sfxSampleCache;
}

#ifdef __CLIENT__
bool AudioSystem::hasSfxChannels()
{
    return bool( d->sfxChannels );
}

audio::SfxChannels &AudioSystem::sfxChannels() const
{
    DENG2_ASSERT(d->sfxChannels.get() != nullptr);
    return *d->sfxChannels;
}

void AudioSystem::allowSfxRefresh(bool allow)
{
    if (!d->sfxAvail) return;
    if (allowRefresh == allow) return; // No change.

    allowRefresh = allow;

    // If we're denying refresh, let's make sure that if it's currently
    // running, we don't continue until it has stopped.
    if (!allow)
    {
        while (refreshing)
        {
            Sys_Sleep(0);
        }
    }

    // Sys_SuspendThread(::refreshHandle, !allow);
}

void AudioSystem::requestSfxListenerUpdate()
{
    // Request a listener reverb update at the end of the frame.
    d->sfxListenerSubsector = nullptr;
}

#endif  // __CLIENT__

void AudioSystem::startLogical(dint soundIdAndFlags, mobj_t const *emitter)
{
    d->sfxStartLogical(soundIdAndFlags, emitter);
}

void AudioSystem::aboutToUnloadMap()
{
    LOG_AS("AudioSystem");
    LOG_AUDIO_VERBOSE("Cleaning for map unload...");

    d->sfxClearLogical();

#ifdef __CLIENT__
    // Mobjs are about to be destroyed so stop all sound channels using one as an emitter.
    d->sfxChannels->forAll([] (audio::SfxChannel &ch)
    {
        if (ch.emitter())
        {
            ch.setEmitter(nullptr);
            ch.stop();
        }
        return LoopContinue;
    });

    // Sectors, too, for that matter.
    d->sfxListenerSubsector = nullptr;
#endif
}

#ifdef __CLIENT__
void AudioSystem::worldMapChanged()
{
    // Update who is listening now.
    setSfxListener(S_GetListenerMobj());
}
#endif

/**
 * Console command for playing a (local) sound effect.
 */
D_CMD(PlaySound)
{
    DENG2_UNUSED(src);

    if (argc < 2)
    {
        LOG_SCR_NOTE("Usage: %s (id) (volume) at (x) (y) (z)") << argv[0];
        LOG_SCR_MSG("(volume) must be in 0..1, but may be omitted");
        LOG_SCR_MSG("'at (x) (y) (z)' may also be omitted");
        LOG_SCR_MSG("The sound is always played locally");
        return true;
    }
    dint p = 0;

    // The sound ID is always first.
    dint const id = DED_Definitions()->getSoundNum(argv[1]);

    // The second argument may be a volume.
    dfloat volume = 1;
    if (argc >= 3 && String(argv[2]).compareWithoutCase("at"))
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
    if (argc >= p + 4 && !String(argv[p]).compareWithoutCase("at"))
    {
        useFixedPos = true;
        fixedPos[0] = String(argv[p + 1]).toDouble();
        fixedPos[1] = String(argv[p + 2]).toDouble();
        fixedPos[2] = String(argv[p + 3]).toDouble();
    }

    // Check that the volume is valid.
    volume = de::clamp(0.f, volume, 1.f);
    if (de::fequal(volume, 0)) return true;

    if (useFixedPos)
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

    if (!App_AudioSystem().musicIsAvailable())
    {
        LOGDEV_SCR_ERROR("Music subsystem is not available");
        return false;
    }

    bool const looped = true;

    if (argc == 2)
    {
        // Play a file associated with the referenced music definition.
        if (Record const *definition = DED_Definitions()->musics.tryFind("id", argv[1]))
        {
            return Mus_Start(*definition, looped);
        }
        LOG_RES_WARNING("Music '%s' not defined") << argv[1];
        return false;
    }

    if (argc == 3)
    {
        // Play a file referenced directly.
        if (!qstricmp(argv[1], "lump"))
        {
            return Mus_StartLump(App_FileSystem().lumpNumForName(argv[2]), looped);
        }
        else if (!qstricmp(argv[1], "file"))
        {
            return Mus_StartFile(argv[2], looped);
        }
        else if (!qstricmp(argv[1], "cd"))
        {
            if (!App_AudioSystem().cd())
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

static void sfxReverbStrengthChanged()
{
    App_AudioSystem().requestSfxListenerUpdate();
}

static void musicMidiFontChanged()
{
    App_AudioSystem().updateMusicMidiFont();
}

D_CMD(ReverbParameters)
{
    DENG2_UNUSED2(src, argc);

    dfloat args[NUM_REVERB_DATA];

    args[SFXLP_REVERB_VOLUME ] = String(argv[1]).toFloat();
    args[SFXLP_REVERB_SPACE  ] = String(argv[2]).toFloat();
    args[SFXLP_REVERB_DECAY  ] = String(argv[3]).toFloat();
    args[SFXLP_REVERB_DAMPING] = String(argv[4]).toFloat();

    LOG_SCR_MSG("Setting reverb parameters:\n"
                "- volume: %f\n"
                "- space: %f\n"
                "- decay: %f\n"
                "- damping: %f")
            << args[SFXLP_REVERB_VOLUME ]
            << args[SFXLP_REVERB_SPACE  ]
            << args[SFXLP_REVERB_DECAY  ]
            << args[SFXLP_REVERB_DAMPING];

    App_AudioSystem().sfx()->Listenerv(SFXLP_REVERB, args);

    return true;
}
#endif

void AudioSystem::consoleRegister()  // static
{
    // Sound effects:
#ifdef __CLIENT__
    //C_VAR_INT     ("sound-16bit",         &sfx16Bit,              0, 0, 1);
    C_VAR_INT     ("sound-3d",            &sfx3D,                 0, 0, 1);
#endif
    C_VAR_BYTE    ("sound-overlap-stop",  &sfxOneSoundPerEmitter, 0, 0, 1);
#ifdef __CLIENT__
    //C_VAR_INT     ("sound-rate",          &sfxSampleRate,         0, 11025, 44100);
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

    C_CMD("reverbparams", "ffff", ReverbParameters);

    // Debug:
    C_VAR_INT     ("sound-info",          &showSoundInfo,         0, 0, 1);
#endif
}

// Music: ---------------------------------------------------------------------------

bool Mus_IsPlaying()
{
#ifdef __CLIENT__
    return App_AudioSystem().musicIsPlaying();
#else
    return false;
#endif
}

#undef S_StopMusic
void S_StopMusic()
{
#ifdef __CLIENT__
    App_AudioSystem().stopMusic();
#endif
}

#undef S_PauseMusic
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

#undef S_StartMusicNum
dint S_StartMusicNum(dint musicId, dd_bool looped)
{
#ifdef __CLIENT__
    if (::isDedicated) return true;

    if (musicId >= 0 && musicId < DED_Definitions()->musics.size())
    {
        const Record &def = DED_Definitions()->musics[musicId];
        return Mus_Start(def, looped);
    }
    return false;
#else
    DENG2_UNUSED2(musicId, looped);
    return false;
#endif
}

#undef S_StartMusic
dint S_StartMusic(char const *musicId, dd_bool looped)
{
    LOG_AS("S_StartMusic");
    dint idx = DED_Definitions()->getMusicNum(musicId);
    if (idx < 0)
    {
        if (musicId && !String(musicId).isEmpty())
        {
#if defined __CLIENT__
            // Fallback: maybe there's a lump with this name instead.
            const String musicLumpName = String::format("d_%s.lmp", musicId);
            const lumpnum_t lumpNum = App_FileSystem().lumpNumForName(musicLumpName);
            if (lumpNum >= 0)
            {
                LOG_AUDIO_MSG("No Music definition for \"%s\", but found lump \"%s\" (%d) instead")
                    << musicId << musicLumpName << lumpNum;
                if (const auto result = App_AudioSystem().playMusicLump(lumpNum, looped))
                {
                    return result;
                }
            }
#endif // __CLIENT__
            LOG_AUDIO_WARNING("Music \"%s\" not defined, cannot start playback") << musicId;
        }
        return false;
    }
    return S_StartMusicNum(idx, looped);
}

// Sound Effects: -------------------------------------------------------------------

mobj_t *S_GetListenerMobj()
{
    return DD_Player(::displayPlayer)->publicData().mo;
}

#undef S_LocalSoundAtVolumeFrom
dint S_LocalSoundAtVolumeFrom(dint soundIdAndFlags, mobj_t const *origin, coord_t *point, dfloat volume)
{
#ifdef __CLIENT__
    LOG_AS("S_LocalSoundAtVolumeFrom");

    // A dedicated server never starts any local sounds (only logical sounds in the LSM).
    if (::isDedicated) return false;

    // Sounds cannot be started while in busy mode...
    if (DoomsdayApp::app().busyMode().isActive())
        return false;

    dint const soundId = (soundIdAndFlags & ~DDSF_FLAG_MASK);
    if (soundId <= 0 || soundId >= DED_Definitions()->sounds.size())
        return false;

    // Skip if sounds won't be heard.
    if (::sfxVolume <= 0 || volume <= 0)
        return false;

    if (volume > 1)
    {
        LOGDEV_AUDIO_WARNING("Volume is too high (%f > 1)") << volume;
    }

    dfloat freq = 1;
    // This is the sound we're going to play.
    sfxinfo_t *info = Def_GetSoundInfo(soundId, &freq, &volume);
    if (!info) return false;  // Hmm? This ID is not defined.

    bool const isRepeating = (soundIdAndFlags & DDSF_REPEAT) || Def_SoundIsRepeating(soundId);

    // Check the distance (if applicable).
    if (!(info->flags & SF_NO_ATTENUATION) && !(soundIdAndFlags & DDSF_NO_ATTENUATION))
    {
        // If origin is too far, don't even think about playing the sound.
        coord_t const *fixPoint = (origin ? origin->origin : point);

        if (Mobj_ApproxPointDistance(S_GetListenerMobj(), fixPoint) > soundMaxDist)
            return false;
    }

    // Load the sample.
    sfxsample_t *sample = App_AudioSystem().sfxSampleCache().cache(soundId);
    if (!sample)
    {
        if (App_AudioSystem().sfxIsAvailable())
        {
            LOG_AUDIO_VERBOSE("Caching of sound %i failed") << soundId;
        }
        return false;
    }

    // Random frequency alteration? (Multipliers chosen to match original
    // sound code.)
    if (!sfxNoRndPitch)
    {
        if (info->flags & SF_RANDOM_SHIFT)
        {
            freq += (RNG_RandFloat() - RNG_RandFloat()) * (7.0f / 255);
        }
        if (info->flags & SF_RANDOM_SHIFT2)
        {
            freq += (RNG_RandFloat() - RNG_RandFloat()) * (15.0f / 255);
        }
    }

    // If the sound has an exclusion group, either all or the same emitter's
    // iterations of this sound will stop.
    if (info->group)
    {
        mobj_t const *emitter = ((info->flags & SF_GLOBAL_EXCLUDE) ? nullptr : origin);
        S_StopSoundGroup(info->group, emitter);
    }

    // Let's play it.
    dint flags = 0;
    flags |= (((info->flags & SF_NO_ATTENUATION) || (soundIdAndFlags & DDSF_NO_ATTENUATION)) ? SF_NO_ATTENUATION : 0);
    flags |= (isRepeating ? SF_REPEAT : 0);
    flags |= ((info->flags & SF_DONT_STOP) ? SF_DONT_STOP : 0);
    return App_AudioSystem().playSound(sample, volume, freq, origin, point, flags);

#else
    DENG2_UNUSED4(soundIdAndFlags, origin, point, volume);
    return false;
#endif
}

#undef S_LocalSoundAtVolume
dint S_LocalSoundAtVolume(dint soundIdAndFlags, mobj_t const *emitter, dfloat volume)
{
    return S_LocalSoundAtVolumeFrom(soundIdAndFlags, emitter, nullptr, volume);
}

#undef S_LocalSound
dint S_LocalSound(dint soundIdAndFlags, mobj_t const *emitter)
{
    // Play local sound at max volume.
    return S_LocalSoundAtVolumeFrom(soundIdAndFlags, emitter, nullptr, 1);
}

#undef S_LocalSoundFrom
dint S_LocalSoundFrom(dint soundIdAndFlags, coord_t *origin)
{
    return S_LocalSoundAtVolumeFrom(soundIdAndFlags, nullptr, origin, 1);
}

#undef S_StartSound
dint S_StartSound(dint soundIdAndFlags, mobj_t const *emitter)
{
#ifdef __SERVER__
    // The sound is audible to everybody.
    Sv_Sound(soundIdAndFlags, emitter, SVSF_TO_ALL);
#endif
    App_AudioSystem().startLogical(soundIdAndFlags, emitter);

    return S_LocalSound(soundIdAndFlags, emitter);
}

#undef S_StartSoundEx
dint S_StartSoundEx(dint soundIdAndFlags, mobj_t const *emitter)
{
#ifdef __SERVER__
    Sv_Sound(soundIdAndFlags, emitter, SVSF_TO_ALL | SVSF_EXCLUDE_ORIGIN);
#endif
    App_AudioSystem().startLogical(soundIdAndFlags, emitter);

    return S_LocalSound(soundIdAndFlags, emitter);
}

#undef S_StartSoundAtVolume
dint S_StartSoundAtVolume(dint soundIdAndFlags, mobj_t const *emitter, dfloat volume)
{
#ifdef __SERVER__
    Sv_SoundAtVolume(soundIdAndFlags, emitter, volume, SVSF_TO_ALL);
#endif
    App_AudioSystem().startLogical(soundIdAndFlags, emitter);

    // The sound is audible to everybody.
    return S_LocalSoundAtVolume(soundIdAndFlags, emitter, volume);
}

#undef S_ConsoleSound
dint S_ConsoleSound(dint soundId, mobj_t *emitter, dint targetConsole)
{
#ifdef __SERVER__
    Sv_Sound(soundId, emitter, targetConsole);
#endif

    // If it's for us, we can hear it.
    if (targetConsole == consolePlayer)
    {
        S_LocalSound(soundId, emitter);
    }

    return true;
}

#undef S_StopSound
void S_StopSound(dint soundId, mobj_t const *emitter)
{
    App_AudioSystem().stopSound(soundId, emitter);
}

#undef S_StopSound2
void S_StopSound2(dint soundId, mobj_t const *emitter, dint flags)
{
    App_AudioSystem().stopSound(soundId, emitter, flags);
}

#undef S_IsPlaying
dint S_IsPlaying(dint soundId, mobj_t *emitter)
{
    return (dint) App_AudioSystem().soundIsPlaying(soundId, emitter);
}

#ifdef __CLIENT__

void S_StopSoundGroup(dint group, mobj_t const *emitter)
{
    App_AudioSystem().stopSoundGroup(group, emitter);
}

dint S_StopSoundWithLowerPriority(dint soundId, mobj_t *emitter, dint defPriority)
{
    return App_AudioSystem().stopSoundWithLowerPriority(soundId, emitter, defPriority);
}

static Value *Function_Audio_LocalSound(Context &, const Function::ArgumentValues &args)
{
    const int   sound  = DED_Definitions()->getSoundNum(args.at(0)->asText());
    const float volume = float(args.at(1)->asNumber());
    if (sound >= 0)
    {
        S_LocalSoundAtVolume(sound, nullptr, volume);
    }
    else
    {
        throw Error("Function_Thing_StartSound", "Undefined sound: " + args.at(0)->asText());
    }
    return nullptr;
}

#endif  // __CLIENT__

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
    S_IsPlaying,
    S_StartMusic,
    S_StartMusicNum,
    S_StopMusic,
    S_PauseMusic
};
