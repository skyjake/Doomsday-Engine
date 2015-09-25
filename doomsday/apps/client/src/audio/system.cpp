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

#include "dd_share.h"  // SF_* flags
#include "dd_main.h"   // ::isDedicated
#include "def_main.h"  // ::defs

#ifdef __SERVER__
#  include "server/sv_sound.h"
#endif

#include "api_sound.h"
#include "audio/samplecache.h"
#ifdef __CLIENT__
#  include "audio/drivers/dummydriver.h"
#  include "audio/drivers/plugindriver.h"
#  include "audio/drivers/sdlmixerdriver.h"
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
#ifdef __CLIENT__
#  include <de/LibraryFile>
#endif
#include <de/timer.h>
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

#ifdef __CLIENT__
enum audiointerfacetype_t
{
    AUDIO_ISFX,
    AUDIO_IMUSIC,
    AUDIO_ICD,
    AUDIO_IMUSIC_OR_ICD
};
#endif

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
#endif  // __CLIENT__

static audio::System *theAudioSystem;

#ifdef __CLIENT__

/**
 * Usually the display player.
 */
static mobj_t *getListenerMobj()
{
    return DD_Player(::displayPlayer)->publicData().mo;
}

#endif
// --------------------------------------------------------------------------------------

/**
 * LogicSounds are used to track currently playing sounds on a logical level (irrespective
 * of whether playback is available, or if the sounds are actually audible to anyone).
 *
 * @todo The premise behind this functionality is fundamentally flawed in that it assumes
 * that the same samples are used by both the Client and the Server, and that the latter
 * schedules remote playback of the former (determined by examining sample lengths on the
 * Server's side). Furthermore, the Server should not be dictating 'oneSoundPerEmitter'
 * policy so that Clients can be configured independently. -ds
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

// --------------------------------------------------------------------------------------
#ifdef __CLIENT__

System::IDriver::IPlayer::IPlayer(IDriver &driver) : _driver(&driver)
{}

System::IDriver &System::IDriver::IPlayer::driver() const
{
    DENG2_ASSERT(_driver != nullptr);
    return *_driver;
};

DotPath System::IDriver::IPlayer::identityKey() const
{
    return driver().identityKey() + "." + name();
}

ICdPlayer::ICdPlayer(System::IDriver &driver) : IPlayer(driver)
{}

IMusicPlayer::IMusicPlayer(System::IDriver &driver) : IPlayer(driver)
{}

ISoundPlayer::ISoundPlayer(System::IDriver &driver) : IPlayer(driver)
{}

static String playerTypeName(System::IDriver::IPlayer const &player)
{
    if(player.is<ICdPlayer>())    return "CD";
    if(player.is<IMusicPlayer>()) return "Music";
    if(player.is<ISoundPlayer>()) return "SFX";
    return "(unknown)";
}

// --------------------------------------------------------------------------------------

String System::IDriver::statusAsText() const
{
    switch(status())
    {
    case Loaded:      return "Loaded";
    case Initialized: return "Initialized";

    default: DENG2_ASSERT(!"audio::System::IDriver::statusAsText: Invalid status"); break;
    }
    return "Invalid";
}

String System::IDriver::description() const
{
    auto desc = String(     _E(b) "%1" _E(.)
                       "\n" _E(l) "IdentityKey: " _E(.) "%2")
                  .arg(title())
                  .arg(identityKey());

    if(isInitialized())
    {
        // Summarize available player interfaces.
        String pSummary;
        forAllPlayers([&pSummary] (IPlayer &player)
        {
            if(!pSummary.isEmpty()) pSummary += "\n" _E(0);
            pSummary += " - " + playerTypeName(player) + ": " _E(>) + player.name() + _E(<);
            return LoopContinue;
        });
        if(!pSummary.isEmpty())
        {
            desc += "\n" _E(D)_E(b) "Player interfaces:"
                    "\n" _E(.)_E(.) + pSummary;
        }
    }

    // Finally, the high-level status of the driver.
    desc += "\n" _E(D)_E(b) "Status: " _E(.) + statusAsText();

    return desc;
}

#endif  // __CLIENT__
// --------------------------------------------------------------------------------------

/**
 * @todo Simplify architecture - load the "dummy" driver, always -ds
 */
DENG2_PIMPL(System)
#ifdef __CLIENT__
, DENG2_OBSERVES(App, GameUnload)
#endif
{
#ifdef __CLIENT__
    SettingsRegister settings;
    QList<IDriver *> drivers;

    IDriver *tryFindDriver(String driverIdKey)
    {
        driverIdKey = driverIdKey.toLower();  // Symbolic identity keys are lowercase.
        for(IDriver *driver : drivers)
        for(String const &idKey : driver->identityKey().split(';'))
        {
            if(idKey == driverIdKey)
                return driver;
        }
        return nullptr;
    }

    IDriver &findDriver(String driverIdKey)
    {
        if(IDriver *driver = tryFindDriver(driverIdKey)) return *driver;
        /// @throw MissingDriverError  Unknown driver identifier specified.
        throw MissingDriverError("audio::System::findDriver", "Unknown audio driver '" + driverIdKey + "'");
    }

    void unloadDrivers()
    {
        // Deinitialize all loaded drivers. (Note: reverse order)
        /// @todo fixme: load order != initialization order -ds
        for(dint i = drivers.count(); i--> 0; )
        {
            drivers[i]->deinitialize();
        }

        // Unload the plugins after everything has been shut down.
        qDeleteAll(drivers);
        drivers.clear();

        // No more interfaces available.
        activeInterfaces.clear();
    }

    /**
     * Chooses the default audio driver based on configuration options.
     */
    String chooseDriver()
    {
        // Presently the audio driver configuration is inferred and/or specified
        // using command line options.
        /// @todo Store this information persistently in Config. -ds
        CommandLine &cmdLine = App::commandLine();
        for(IDriver const *driver : drivers)
        for(QString const &driverIdKey : driver->identityKey().split(';'))
        {
            if(cmdLine.has("-" + driverIdKey))
                return driverIdKey;
        }

        return "fmod";  // The default audio driver.
    }

    void loadDrivers()
    {
        DENG2_ASSERT(activeInterfaces.isEmpty());
        DENG2_ASSERT(drivers.isEmpty());
        DENG2_ASSERT(!App::commandLine().has("-nosound"));

        // Firstly - built-in audio drivers.
        drivers << new DummyDriver;
#ifndef DENG_DISABLE_SDLMIXER
        drivers << new SdlMixerDriver;
#endif

        // Secondly - plugin audio drivers.
        Library_forAll([this] (LibraryFile &libFile)
        {
            if(PluginDriver::recognize(libFile))
            {
                LOG_AUDIO_VERBOSE("Loading plugin audio driver '%s'...") << libFile.name();
                if(auto *driver = PluginDriver::newFromLibrary(libFile))
                {
                    // Add the new driver to the collection.
                    drivers << driver;
                }
                else
                {
                    LOG_AUDIO_WARNING("Failed loading plugin audio driver '%s'") << libFile.name();
                }
            }
            return LoopContinue;
        });

        // Choose the default driver and initialize it.
        /// @todo Defer until an interface is added. -ds
        IDriver *defaultDriver = tryFindDriver(chooseDriver());
        if(defaultDriver)
        {
            initDriverIfNeeded(*defaultDriver);
        }
        // Fallback option for the default driver.
#ifndef DENG_DISABLE_SDLMIXER
        if(!defaultDriver || !defaultDriver->isInitialized())
        {
            defaultDriver = &findDriver("sdlmixer");
            initDriverIfNeeded(*defaultDriver);
        }
#endif

        if(defaultDriver && defaultDriver->isInitialized())
        {
            // Choose the interfaces to use.
            selectInterfaces(*defaultDriver);
        }
    }

    /**
     * Initialize the given audio @a driver if necessary.
     *
     * @return  Same as @a driver, for caller convenience.
     */
    IDriver &initDriverIfNeeded(IDriver &driver)
    {
        if(!driver.isInitialized())
        {
            LOG_AUDIO_VERBOSE("Initializing audio driver '%s'...") << driver.identityKey();
            driver.initialize();
            if(!driver.isInitialized())
            {
                /// @todo Why, exactly? (log it!) -ds
                LOG_AUDIO_WARNING("Failed initializing audio driver '%s'") << driver.identityKey();
            }
        }
        return driver;
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
        IDriver::IPlayer *player;
    };
    QList<PlaybackInterface> activeInterfaces;

    void addPlaybackInterface(PlaybackInterface const &ifs)
    {
        DENG2_ASSERT(ifs.type == AUDIO_ISFX || ifs.type == AUDIO_IMUSIC || ifs.type == AUDIO_ICD);
        DENG2_ASSERT(ifs.player != nullptr);
        /// @todo Ensure this interface is not already present! -ds
        activeInterfaces << ifs;  // a copy is made
    }

    /**
     * Returns the currently active, primary CdPlayer.
     */
    ICdPlayer &getCdPlayer() const
    {
        // The primary interface is the first one.
        ICdPlayer *found = nullptr;
        forAllInterfaces(AUDIO_ICD, [&found] (IDriver::IPlayer &plr)
        {
            found = &plr.as<ICdPlayer>();
            return LoopAbort;
        });
        if(found) return *found;

        /// @throw Error  No suitable sound player is available.
        throw Error("audio::System::Instance::getCdPlayer", "No CdPlayer available");
    }

    ICdPlayer *tryFindCdPlayer(IDriver &driver)
    {
        ICdPlayer *found = nullptr;  // Not found.
        driver.forAllPlayers([&found] (IDriver::IPlayer &player)
        {
            if(auto *cdPlayer = player.maybeAs<ICdPlayer>())
            {
                found = cdPlayer;
                return LoopAbort;
            }
            return LoopContinue;
        });
        return found;
    }

    /**
     * Returns the currently active, primary MusicPlayer.
     */
    IMusicPlayer &getMusicPlayer() const
    {
        // The primary interface is the first one.
        IMusicPlayer *found = nullptr;
        forAllInterfaces(AUDIO_IMUSIC, [&found] (IDriver::IPlayer &plr)
        {
            found = &plr.as<IMusicPlayer>();
            return LoopAbort;
        });
        if(found) return *found;

        /// @throw Error  No suitable sound player is available.
        throw Error("audio::System::Instance::getMusicPlayer", "No MusicPlayer available");
    }

    IMusicPlayer *tryFindMusicPlayer(IDriver &driver)
    {
        IMusicPlayer *found = nullptr;  // Not found.
        driver.forAllPlayers([&found] (IDriver::IPlayer &player)
        {
            if(auto *musicPlayer = player.maybeAs<IMusicPlayer>())
            {
                found = musicPlayer;
                return LoopAbort;
            }
            return LoopContinue;
        });
        return found;
    }

    /**
     * Returns the currently active, primary SoundPlayer.
     */
    ISoundPlayer &getSoundPlayer() const
    {
        // The primary interface is the first one.
        ISoundPlayer *found = nullptr;
        forAllInterfaces(AUDIO_ISFX, [&found] (IDriver::IPlayer &plr)
        {
            found = &plr.as<ISoundPlayer>();
            return LoopAbort;
        });
        if(found) return *found;

        /// @throw Error  No suitable sound player is available.
        throw Error("audio::System::Instance::getSoundPlayer", "No SoundPlayer available");
    }

    ISoundPlayer *tryFindSoundPlayer(IDriver &driver)
    {
        ISoundPlayer *found = nullptr;  // Not found.
        driver.forAllPlayers([&found] (IDriver::IPlayer &player)
        {
            if(auto *soundPlayer = player.maybeAs<ISoundPlayer>())
            {
                found = soundPlayer;
                return LoopAbort;
            }
            return LoopContinue;
        });
        return found;
    }

    /**
     * Choose the SFX, Music, and CD audio interfaces to use.
     */
    void selectInterfaces(IDriver &defaultDriver)
    {
        // The default driver goes on the bottom of the stack.
        if(ISoundPlayer *player = tryFindSoundPlayer(defaultDriver))
        {
            PlaybackInterface ifs; zap(ifs);
            ifs.type   = AUDIO_ISFX;
            ifs.player = player;
            addPlaybackInterface(ifs);  // a copy is made
        }

        if(IMusicPlayer *player = tryFindMusicPlayer(defaultDriver))
        {
            PlaybackInterface ifs; zap(ifs);
            ifs.type   = AUDIO_IMUSIC;
            ifs.player = player;
            addPlaybackInterface(ifs);  // a copy is made
        }
#if 0
#ifdef MACOSX
        else if(defaultDriver.identityKey() != "dummy")
        {
            // On the Mac, use the built-in QuickTime interface as the fallback for music.
            PlaybackInterface ifs; zap(ifs);
            ifs.type   = AUDIO_IMUSIC;
            ifs.player = &::audiodQuickTimeMusic;
            addPlaybackInterface(ifs);  // a copy is made
        }
#endif

#ifndef WIN32
        // At the moment, dsFMOD supports streaming samples so we can
        // automatically load dsFluidSynth for MIDI music.
        if(defaultDriver.identityKey() == "fmod")
        {
            initDriverIfNeeded("fluidsynth");
            IDriver &fluidSynth = driverById(AUDIOD_FLUIDSYNTH);
            if(IMusicPlayer *player = tryFindMusicPlayer(fluidSynth))
            {
                PlaybackInterface ifs; zap(ifs);
                ifs.type   = AUDIO_IMUSIC;
                ifs.player = player;
                addPlaybackInterface(ifs);  // a copy is made
            }
        }
#endif
#endif

        if(ICdPlayer *player = tryFindCdPlayer(defaultDriver))
        {
            PlaybackInterface ifs; zap(ifs);
            ifs.type   = AUDIO_ICD;
            ifs.player = player;
            addPlaybackInterface(ifs);  // a copy is made
        }

        CommandLine &cmdLine = App::commandLine();
        for(dint p = 1; p < cmdLine.count() - 1; ++p)
        {
            if(!cmdLine.isOption(p)) continue;

            // A SFX override?
            if(cmdLine.matches("-isfx", cmdLine.at(p)))
            {
                try
                {
                    IDriver &driver = findDriver(cmdLine.at(++p));
                    initDriverIfNeeded(driver);
                    if(ISoundPlayer *player = tryFindSoundPlayer(driver))
                    {
                        PlaybackInterface ifs; zap(ifs);
                        ifs.type   = AUDIO_ISFX;
                        ifs.player = player;
                        addPlaybackInterface(ifs);  // a copy is made
                    }
                    else
                    {
                        LOG_AUDIO_WARNING("Audio driver \"" + driver.title() + "\" does not provide a SFX interface");
                    }
                }
                catch(MissingDriverError const &er)
                {
                    LOG_AUDIO_WARNING("") << er.asText();
                }
                continue;
            }

            // A Music override?
            if(cmdLine.matches("-imusic", cmdLine.at(p)))
            {
                try
                {
                    IDriver &driver = findDriver(cmdLine.at(++p));
                    initDriverIfNeeded(driver);
                    if(IMusicPlayer *player = tryFindMusicPlayer(driver))
                    {
                        PlaybackInterface ifs; zap(ifs);
                        ifs.type   = AUDIO_IMUSIC;
                        ifs.player = player;
                        addPlaybackInterface(ifs);  // a copy is made
                    }
                    else
                    {
                        LOG_AUDIO_WARNING("Audio driver \"" + driver.title() + "\" does not provide a Music interface");
                    }
                }
                catch(MissingDriverError const &er)
                {
                    LOG_AUDIO_WARNING("") << er.asText();
                }
                continue;
            }

            // A CD override?
            if(cmdLine.matches("-icd", cmdLine.at(p)))
            {
                try
                {
                    IDriver &driver = findDriver(cmdLine.at(++p));
                    initDriverIfNeeded(driver);
                    if(ICdPlayer *player = tryFindCdPlayer(driver))
                    {
                        PlaybackInterface ifs; zap(ifs);
                        ifs.type   = AUDIO_ICD;
                        ifs.player = player;
                        addPlaybackInterface(ifs);  // a copy is made
                    }
                    else
                    {
                        LOG_AUDIO_WARNING("Audio driver \"" + driver.title() + "\" does not provide a CD interface");
                    }
                }
                catch(MissingDriverError const &er)
                {
                    LOG_AUDIO_WARNING("") << er.asText();
                }
                continue;
            }
        }
    }

    /**
     * Iterate through the active interfaces of a given type, in descending
     * priority order: the most important interface is visited first.
     *
     * @param type  Type of interface to process.
     * @param func  Callback to make for each interface.
     */
    LoopResult forAllInterfaces(audiointerfacetype_t type,
        std::function<LoopResult (IDriver::IPlayer &)> func) const
    {
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            PlaybackInterface const &ifs = activeInterfaces[i];
            if(ifs.type == type ||
                (type == AUDIO_IMUSIC_OR_ICD && (ifs.type == AUDIO_IMUSIC ||
                                                ifs.type == AUDIO_ICD)))
            {
                if(auto result = func(*ifs.player))
                    return result;
            }
        }
        return LoopContinue;
    }

    bool musAvail = false;              ///< @c true if at least one driver is initialized for music playback.
    bool musNeedBufFileSwitch = false;  ///< @c true= choose a new file name for the buffered playback file when asked. */
    String musCurrentSong;
    bool musPaused = false;

    bool sfxAvail = false;              ///< @c true if a sound driver is initialized for sound effect playback.
    mobj_t *sfxListener = nullptr;
    SectorCluster *sfxListenerCluster = nullptr;
    std::unique_ptr<Channels> channels;
#endif

    LogicSoundHash sfxLogicHash;
    duint sfxLogicLastPurge = 0;
    bool sfxLogicOneSoundPerEmitter = false;  ///< set at the start of the frame

    SampleCache sampleCache;

    Instance(Public *i) : Base(i)
    {
        theAudioSystem = thisPublic;

#ifdef __CLIENT__
        // Initialize settings.
        typedef SettingsRegister SReg; // convenience
        settings
            .define(SReg::IntCVar,    "sound-volume",        255 * 2/3)
            .define(SReg::IntCVar,    "music-volume",        255 * 2/3)
            .define(SReg::FloatCVar,  "sound-reverb-volume", 0.5f)
            .define(SReg::IntCVar,    "sound-info",          0)
            .define(SReg::IntCVar,    "sound-rate",          11025)
            .define(SReg::IntCVar,    "sound-16bit",         0)
            .define(SReg::IntCVar,    "sound-3d",            0)
            .define(SReg::IntCVar,    "sound-overlap-stop",  0)
            .define(SReg::IntCVar,    "music-source",        MUSP_EXT)
            .define(SReg::StringCVar, "music-soundfont",     "");

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

            auto didPlay = forAllInterfaces(AUDIO_IMUSIC, [this, &hndl, &looped] (IDriver::IPlayer &plr)
            {
                auto &musicPlayer = plr.as<IMusicPlayer>();

                // Does this interface offer buffered playback?
                if(musicPlayer.canPlayBuffer())
                {
                    // Buffer the data using the driver's own facility.
                    dsize const len = hndl->length();
                    hndl->read((duint8 *) musicPlayer.songBuffer(len), len);

                    return musicPlayer.play(looped);
                }
                // Does this interface offer playback from a native file?
                else if(musicPlayer.canPlayFile())
                {
                    // Write the data to disk and play from there.
                    String const bufPath = composeMusicBufferFilename();

                    dsize len = hndl->length();
                    auto *buf = (duint8 *)M_Malloc(len);
                    hndl->read(buf, len);
                    F_Dump(buf, len, bufPath.toUtf8().constData());
                    M_Free(buf); buf = nullptr;

                    return musicPlayer.playFile(bufPath.toUtf8().constData(), looped);
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

            return forAllInterfaces(AUDIO_IMUSIC, [&srcFile, &looped] (IDriver::IPlayer &plr)
            {
                auto &musicPlayer = plr.as<IMusicPlayer>();
                if(musicPlayer.canPlayFile())
                {
                    return musicPlayer.playFile(srcFile.toUtf8().constData(), looped);
                }
                return 0;  // Continue.
            });
        }

        return forAllInterfaces(AUDIO_IMUSIC, [this, &lump, &looped] (IDriver::IPlayer &plr)
        {
            auto &musicPlayer = plr.as<IMusicPlayer>();

            // Does this interface offer buffered playback?
            if(musicPlayer.canPlayBuffer())
            {
                // Buffer the data using the driver's own facility.
                std::unique_ptr<FileHandle> hndl(&App_FileSystem().openLump(lump));
                dsize const length  = hndl->length();
                hndl->read((duint8 *) musicPlayer.songBuffer(length), length);
                App_FileSystem().releaseFile(hndl->file());

                return musicPlayer.play(looped);
            }
            // Does this interface offer playback from a native file?
            else if(musicPlayer.canPlayFile())
            {
                // Write the data to disk and play from there.
                String const fileName = composeMusicBufferFilename();
                if(!F_DumpFile(lump, fileName.toUtf8().constData()))
                {
                    // Failed to write the lump...
                    return 0;
                }

                return musicPlayer.playFile(fileName.toUtf8().constData(), looped);
            }

            return 0;  // Continue.
        });
    }

    dint playMusicCDTrack(dint track, bool looped)
    {
        // Assume track 0 is not valid.
        if(track == 0) return 0;

        return forAllInterfaces(AUDIO_ICD, [&track, &looped] (IDriver::IPlayer &plr)
        {
            return plr.as<ICdPlayer>().play(track, looped);
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
        forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [&initialized] (IDriver::IPlayer &plr)
        {
            if(plr.initialize())
            {
                initialized += 1;
            }
            else
            {
                LOG_AUDIO_WARNING("Failed to initialize \"%s\" for music playback")
                    << plr.identityKey();
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
        forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (IDriver::IPlayer &plr)
        {
            plr.deinitialize();
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
            forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [&newVolume] (IDriver::IPlayer &plr)
            {
                if(auto *musicPlayer = plr.maybeAs<IMusicPlayer>())
                {
                    musicPlayer->setVolume(newVolume);
                }
                if(auto *cdPlayer = plr.maybeAs<ICdPlayer>())
                {
                    cdPlayer->setVolume(newVolume);
                }
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

        // (Re)Init the sample cache.
        sampleCache.clear();

        // Initialize interfaces for sound playback.
        dint initialized = 0;
        forAllInterfaces(AUDIO_ISFX, [&initialized] (IDriver::IPlayer &plr)
        {
            if(plr.initialize())
            {
                initialized += 1;
            }
            else
            {
                LOG_AUDIO_WARNING("Failed to initialize \"%s\" for sound playback")
                    << plr.identityKey();
            }
            return LoopContinue;
        });

        // Remember whether an interface for sound playback initialized successfully.
        sfxAvail = initialized >= 1;

        if(sfxAvail)
        {
            // This is based on the scientific calculations that if the DOOM marine
            // is 56 units tall, 60 is about two meters.
            //// @todo Derive from the viewheight.
            getSoundPlayer().listener(SFXLP_UNITS_PER_METER, 30);
            getSoundPlayer().listener(SFXLP_DOPPLER, 1.5f);

            sfxListenerNoReverb();
        }

        // Prepare the channel map.
        initChannels();
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

        // Clear the channel map (and stop the refresh thread if running).
        channels.reset();

        // Shutdown interfaces.
        forAllInterfaces(AUDIO_ISFX, [] (IDriver::IPlayer &plr)
        {
            plr.deinitialize();
            return LoopContinue;
        });
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
        getSoundPlayer().listenerv(SFXLP_PRIMARY_FORMAT, parm);

        // Replace the entire channel set (we'll reconfigure).
        channels.reset(new Channels);
        dint num2D = sfx3D ? CHANNEL_2DCOUNT: numChannels;  // The rest will be 3D.
        for(dint i = 0; i < numChannels; ++i)
        {
            Sound/*Channel*/ &ch = channels->add(*getSoundPlayer().makeSound(num2D-- > 0, sfxBits, sfxRate));
            if(!ch.hasBuffer())
            {
                LOG_AUDIO_WARNING("Failed creating Sound for Channel #%i") << i;
            }
        }
    }

    void getChannelPriorities(dfloat *prios) const
    {
        if(!prios) return;

        dint idx = 0;
        channels->forAll([&prios, &idx] (Sound/*Channel*/ &ch)
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
            if(channels->stopWithLowerPriority(0, emitter, ::defs.sounds[sample.soundId].geti("priority")) < 0)
            {
                // Something with a higher priority is playing, can't start now.
                LOG_AUDIO_MSG("Not playing sound id:%i (prio:%i) because overridden (emitter id:%i)")
                    << sample.soundId
                    << ::defs.sounds[sample.soundId].geti("priority")
                    << emitter->thinker.id;
                return false;
            }
        }

        // Calculate the new sound's priority.
        dfloat const myPrio =
            Sound::ratePriority(self.listener(), emitter, origin, volume, Timer_Ticks());

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
                Sound/*Channel*/ *selCh = nullptr;
                channels->forAll([&sample, &myPrio, &channelPrios,
                                  &selCh, &lowPrio, &idx] (Sound/*Channel*/ &ch)
                {
                    dfloat const chPriority = channelPrios[idx++];

                    if(ch.isPlaying())
                    {
                        if(   ch.buffer().sample->soundId == sample.soundId
                           && (myPrio >= chPriority && (!selCh || chPriority <= lowPrio)))
                        {
                            selCh   = &ch;
                            lowPrio = chPriority;
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
        self.allowSoundRefresh(false);

        // First look through the stopped channels. At this stage we're very picky:
        // only the perfect choice will be good enough.
        Sound/*Channel*/ *selCh = channels->tryFindVacant(!play3D, sample.bytesPer,
                                                          sample.rate, sample.soundId);

        if(!selCh)
        {
            // Perhaps there is a vacant channel (with any sample, but preferably one
            // with no sample already loaded).
            selCh = channels->tryFindVacant(!play3D, sample.bytesPer, sample.rate, 0);
        }

        if(!selCh)
        {
            // Try any non-playing channel in the correct format.
            selCh = channels->tryFindVacant(!play3D, sample.bytesPer, sample.rate, -1);
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
            Sound/*Channel*/ *prioCh = nullptr;
            dint idx = 0;
            channels->forAll([&play3D, &myPrio, &channelPrios,
                              &selCh, &prioCh, &lowPrio, &idx] (Sound/*Channel*/ &ch)
            {
                dfloat const chPriority = channelPrios[idx++];

                if(ch.hasBuffer())
                {
                    // Sample buffer must be configured for the right mode.
                    if(play3D == ((ch.buffer().flags & SFXBF_3D) != 0))
                    {
                        if(!ch.isPlaying())
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
            self.allowSoundRefresh();
            LOG_AUDIO_XVERBOSE("Failed to find suitable channel for sample id:%i") << sample.soundId;
            return false;
        }

        Sound &sound = *selCh;
        DENG2_ASSERT(sound.hasBuffer());
        // The sample buffer may need to be reformatted.

        sound.format(!play3D, sample.bytesPer, sample.rate);
        sound.setPlayingMode(flags);
        sound.setFlags(sound.flags() & ~(SFXCF_NO_ORIGIN | SFXCF_NO_ATTENUATION | SFXCF_NO_UPDATE));
        sound.setVolume(volume)
             .setFrequency(freq);

        if(!emitter && !origin)
        {
            sound.setFlags(sound.flags() | SFXCF_NO_ORIGIN);
            sound.setEmitter(nullptr);
        }
        else
        {
            sound.setEmitter(emitter);
            if(origin)
            {
                sound.setOrigin(Vector3d(origin));
            }
        }

        if(flags & SF_NO_ATTENUATION)
        {
            // The sound can be heard from any distance.
            sound.setFlags(sound.flags() | SFXCF_NO_ATTENUATION);
        }

        // Update listener properties.
        getSoundPlayer().listener(SFXLP_UPDATE, 0);

        // Load in the sample if needed and start playing.
        sound.load(sample);
        sound.play();

        // Paging of Sound playback data may now continue.
        self.allowSoundRefresh();

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

            auto *ls = new LogicSound;
            ls->emitter     = emitter;
            ls->isRepeating = isRepeating;
            ls->endTime     = Timer_RealMilliseconds() + length;
            sfxLogicHash.insert(soundId, ls);
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
        getSoundPlayer().listenerv(SFXLP_REVERB, rev);
        getSoundPlayer().listener(SFXLP_UPDATE, 0);
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
                getSoundPlayer().listenerv(SFXLP_POSITION, vec);
            }
            {
                // Orientation. (0,0) will produce front=(1,0,0) and up=(0,0,1).
                dfloat vec[2] = {
                    sfxListener->angle / (dfloat) ANGLE_MAX * 360,
                    (sfxListener->dPlayer ? LOOKDIR2DEG(sfxListener->dPlayer->lookDir) : 0)
                };
                getSoundPlayer().listenerv(SFXLP_ORIENTATION, vec);
            }
            {
                // Velocity. The unit is world distance units per second
                auto const velocity = Vector4f(Vector3d(sfxListener->mom).toVector3f(), 0) * TICSPERSEC;
                dfloat vec[4];
                velocity.decompose(vec);
                getSoundPlayer().listenerv(SFXLP_VELOCITY, vec);
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
                getSoundPlayer().listenerv(SFXLP_REVERB, vec);
            }
        }

        // Update all listener properties.
        getSoundPlayer().listener(SFXLP_UPDATE, 0);
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

    DENG2_PIMPL_AUDIENCE(FrameBegins)
    DENG2_PIMPL_AUDIENCE(FrameEnds)
    DENG2_PIMPL_AUDIENCE(MidiFontChange)

#endif  // __CLIENT__
};

#ifdef __CLIENT__
DENG2_AUDIENCE_METHOD(System, FrameBegins)
DENG2_AUDIENCE_METHOD(System, FrameEnds)
DENG2_AUDIENCE_METHOD(System, MidiFontChange)
#endif

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

SettingsRegister &System::settings()
{
    return d->settings;
}

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
               << _E(.) _E(Tb) << ifs.player->identityKey() << "\n";
        }
        else if(ifs.type == AUDIO_ISFX)
        {
            os << _E(Ta) _E(l) << "  SFX: " << _E(.) _E(Tb)
               << ifs.player->identityKey() << "\n";
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

        // Stop all sounds.
        d->channels->forAll([] (Sound/*Channel*/ &ch)
        {
            ch.stop();
            return LoopContinue;
        });

        // Clear the sample cache.
        d->sampleCache.clear();
    }

    stopMusic();
}

/**
 * @todo Do this in timeChanged()
 */
void System::startFrame()
{
    LOG_AS("audio::System");

    d->updateMusicVolumeIfChanged();

    // Notify interested parties.
    DENG2_FOR_AUDIENCE2(FrameBegins, i) i->systemFrameBegins(*this);

    if(soundPlaybackAvailable())
    {
        // Update all channels (freq, 2D:pan,volume, 3D:position,velocity).

        // Have there been changes to the cvar settings?
        d->updateSfx3DModeIfChanged();
        d->updateSfxSampleRateIfChanged();

        // Should we purge the cache (to conserve memory)?
        d->sampleCache.maybeRunPurge();
    }

    d->sfxLogicOneSoundPerEmitter = sfxOneSoundPerEmitter;
    d->sfxPurgeLogical();
}

void System::endFrame()
{
    LOG_AS("audio::System");

    if(soundPlaybackAvailable() && !BusyMode_Active())
    {
        // Update listener properties.
        // If no listener is available - no 3D positioning is done.
        d->sfxListener = getListenerMobj();
        d->updateSfxListener();
    }

    // Notify interested parties.
    DENG2_FOR_AUDIENCE2(FrameEnds, i) i->systemFrameEnds(*this);
}

void System::initPlayback()
{
    LOG_AS("audio::System");

    CommandLine &cmdLine = App::commandLine();
    if(cmdLine.has("-nosound") || cmdLine.has("-noaudio"))
    {
        LOG_AUDIO_NOTE("Music and sound effects are disabled");
        return;
    }

    LOG_AUDIO_VERBOSE("Initializing for playback...");

    // Disable random pitch changes?
    sfxNoRndPitch = cmdLine.has("-norndpitch");

    // Load all the available audio drivers and then select and initialize playback
    // interfaces specified in Config.
    d->loadDrivers();

    // Initialize sfx playback.
    try
    {
        d->initSfx();
    }
    catch(Error const &er)
    {
        LOG_AUDIO_NOTE("Failed initializing playback for sound effects:\n") << er.asText();
    }

    // Initialize music playback.
    try
    {
        d->initMusic();
    }
    catch(Error const &er)
    {
        LOG_AUDIO_NOTE("Failed initializing playback for music:\n") << er.asText();
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

dint System::driverCount() const
{
    return d->drivers.count();
}

System::IDriver const *System::tryFindDriver(String driverIdKey) const
{
    return d->tryFindDriver(driverIdKey);
}

System::IDriver const &System::findDriver(String driverIdKey) const
{
    return d->findDriver(driverIdKey);
}

LoopResult System::forAllDrivers(std::function<LoopResult (IDriver const &)> func) const
{
    for(IDriver *driver : d->drivers)
    {
        if(auto result = func(*driver)) return result;
    }
    return LoopContinue;
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

bool System::musicPlaybackAvailable() const
{
    return d->musAvail;
}

dint System::musicVolume() const
{
    return musVolume;
}

bool System::musicIsPlaying() const
{
    //LOG_AS("audio::System");
    return d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (IDriver::IPlayer &plr)
    {
        if(auto *musicPlayer = plr.maybeAs<IMusicPlayer>())
        {
            return musicPlayer->isPlaying();
        }
        if(auto *cdPlayer = plr.maybeAs<ICdPlayer>())
        {
            return cdPlayer->isPlaying();
        }
        return false; // Continue
    });
}

void System::stopMusic()
{
    if(!d->musAvail) return;

    LOG_AS("audio::System");
    d->musCurrentSong = "";

    // Stop all interfaces.
    d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [] (IDriver::IPlayer &plr)
    {
        if(auto *musicPlayer = plr.maybeAs<IMusicPlayer>())
        {
            musicPlayer->stop();
        }
        if(auto *cdPlayer = plr.maybeAs<ICdPlayer>())
        {
            cdPlayer->stop();
        }
        return LoopContinue;
    });
}

void System::pauseMusic(bool doPause)
{
    if(!d->musAvail) return;

    LOG_AS("audio::System");
    d->musPaused = !d->musPaused;

    // Pause playback on all interfaces.
    d->forAllInterfaces(AUDIO_IMUSIC_OR_ICD, [&doPause] (IDriver::IPlayer &plr)
    {
        if(auto *musicPlayer = plr.maybeAs<IMusicPlayer>())
        {
            musicPlayer->pause(doPause);
        }
        if(auto *cdPlayer = plr.maybeAs<ICdPlayer>())
        {
            cdPlayer->pause(doPause);
        }
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
            if(d->playMusicCDTrack(defn::Music(definition).cdTrack(), looped))
            {
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
    path = path.expand();

    if(F_FileExists(path.toString().toLatin1().constData()))
    {
        LOG_AUDIO_MSG("Current soundfont set to: \"%s\"") << path.pretty();
    }
    else
    {
        LOG_AUDIO_WARNING("Soundfont \"%s\" not found") << path.pretty();
    }

    // Notify interested parties.
    DENG2_FOR_AUDIENCE2(MidiFontChange, i) i->systemMidiFontChanged(path);
}

dint System::soundVolume() const
{
    return sfxVolume;
}

#endif  // __CLIENT__

Ranged System::soundVolumeAttenuationRange() const
{
    return Ranged(sfxDistMin, sfxDistMax);
}

#ifdef __CLIENT__

bool System::soundPlaybackAvailable() const
{
    return d->sfxAvail;
}

mobj_t *System::listener()
{
    return d->sfxListener;
}

coord_t System::distanceToListener(Vector3d const &point) const
{
    if(mobj_t const *listener = getListenerMobj())
    {
        return Mobj_ApproxPointDistance(*listener, point);
    }
    return 0;
}

bool System::soundIsPlaying(dint soundId, mobj_t *emitter) const
{
    // Use the logic sound hash to determine whether the referenced sound is being
    // played currently. We don't care whether its audible or not.
    return logicalIsPlaying(soundId, emitter);

#if 0
    // Use the sound channels to determine whether the referenced sound is actually
    // being played currently.
    return d->channels->isPlaying(soundId, emitter);
#endif
}

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
    if(emitter || origin)
    {
        if(!(info->flags & SF_NO_ATTENUATION) && !(soundIdAndFlags & DDSF_NO_ATTENUATION))
        {
            // If origin is too far, don't even think about playing the sound.
            if(distanceToListener(emitter ? emitter->origin : origin) > sfxDistMax)
                return false;
        }
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
        d->channels->stopGroup(info->group, (info->flags & SF_GLOBAL_EXCLUDE) ? nullptr : emitter);
    }

    // Let's play it.
    dint flags = 0;
    flags |= (((info->flags & SF_NO_ATTENUATION) || (soundIdAndFlags & DDSF_NO_ATTENUATION)) ? SF_NO_ATTENUATION : 0);
    flags |= (isRepeating ? SF_REPEAT : 0);
    flags |= ((info->flags & SF_DONT_STOP) ? SF_DONT_STOP : 0);
    return d->playSound(*sample, volume, freq, emitter, origin, flags);
}

SampleCache &System::sampleCache() const
{
    return d->sampleCache;
}

dint System::upsampleFactor(dint rate) const
{
    dint factor = 1;
    if(soundPlaybackAvailable())
    {
        // If we need to upsample - determine the scale factor.
        if(!d->getSoundPlayer().anyRateAccepted())
        {
            factor = de::max(1, ::sfxRate / rate);
        }
    }
    return factor;
}

Channels &System::channels() const
{
    DENG2_ASSERT(d->channels.get() != nullptr);
    return *d->channels;
}

void System::requestListenerUpdate()
{
    // Request a listener reverb update at the end of the frame.
    d->sfxListenerCluster = nullptr;
}

void System::allowSoundRefresh(bool allow)
{
    d->forAllInterfaces(AUDIO_ISFX, [&allow] (IDriver::IPlayer &plr)
    {
        plr.as<ISoundPlayer>().allowRefresh(allow);
        return LoopContinue;
    });
}

#endif  // __CLIENT__

void System::clearAllLogical()
{
    LOG_AS("audio::System");
    d->sfxClearLogical();
}

bool System::logicalIsPlaying(dint soundId, mobj_t *emitter) const
{
    LOG_AS("audio::System");
    duint const nowTime = Timer_RealMilliseconds();
    if(soundId)
    {
        auto it = d->sfxLogicHash.constFind(soundId);
        while(it != d->sfxLogicHash.constEnd() && it.key() == soundId)
        {
            LogicSound const &lsound = *it.value();
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
            LogicSound const &lsound = *it.value();
            if(lsound.emitter == emitter && lsound.isPlaying(nowTime))
                return true;

            ++it;
        }
    }
    return false;
}

dint System::stopLogical(dint soundId, mobj_t *emitter)
{
    LOG_AS("audio::System");
    dint numStopped = 0;
    MutableLogicSoundHashIterator it(d->sfxLogicHash);
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

void System::startLogical(dint soundIdAndFlags, mobj_t *emitter)
{
    LOG_AS("audio::System");
    d->sfxStartLogical(soundIdAndFlags, emitter);
}

#ifdef __CLIENT__
void System::worldMapChanged()
{
    // Update who is listening now.
    d->sfxListener = getListenerMobj();
}

/**
 * Console command for logging a summary of the loaded audio drivers.
 */
D_CMD(ListDrivers)
{
    DENG2_UNUSED3(src, argc, argv);
    LOG_AS("listaudiodrivers (Cmd)");

    if(System::get().driverCount() <= 0)
    {
        LOG_MSG("No audio drivers are currently loaded");
        return true;
    }

    String list;
    dint numDrivers = 0;
    System::get().forAllDrivers([&list, &numDrivers] (System::IDriver const &driver)
    {
        if(!list.isEmpty()) list += "\n";

        list += String(_E(0)
                       _E(Ta) "%1 "
                       _E(Tb) _E(2) "%2 " _E(i) "%3")
                  .arg(driver.identityKey())
                  .arg(driver.title())
                  .arg(driver.statusAsText());

        numDrivers += 1;
        return LoopContinue;
    });

    LOG_MSG(_E(b) "Loaded Audio Drivers (%i):") << numDrivers;
    LOG_MSG(_E(R) "\n");
    LOG_MSG("") << list;
    return true;
}

/**
 * Console command for inspecting a loaded audio driver.
 */
D_CMD(InspectDriver)
{
    DENG2_UNUSED2(src, argc);
    LOG_AS("inspectaudiodriver (Cmd)");

    String const driverId(argv[1]);
    if(System::IDriver const *driver = System::get().tryFindDriver(driverId))
    {
        LOG_MSG("") << driver->description();
        return true;
    }
    LOG_WARNING("Unknown audio driver \"%s\"") << driverId;
    return false;
}
#endif

/**
 * Console command for playing a (local) sound effect.
 */
D_CMD(PlaySound)
{
    DENG2_UNUSED(src);
    LOG_AS("playsound (Cmd)");

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

    if(!System::get().musicPlaybackAvailable())
    {
        LOGDEV_SCR_ERROR("Music playback is not available");
        return false;
    }

    bool const looped = true;

    if(argc == 2)
    {
        // Play a file associated with the referenced music definition.
        if(Record const *definition = ::defs.musics.tryFind("id", argv[1]))
        {
            return System::get().playMusic(*definition, looped);
        }
        LOG_RES_WARNING("Music '%s' not defined") << argv[1];
        return false;
    }

    if(argc == 3)
    {
        // Play a file referenced directly.
        if(!qstricmp(argv[1], "lump"))
        {
            return System::get().playMusicLump(App_FileSystem().lumpNumForName(argv[2]), looped);
        }
        else if(!qstricmp(argv[1], "file"))
        {
            return System::get().playMusicFile(argv[2], looped);
        }
        else if(!qstricmp(argv[1], "cd"))
        {
            return System::get().playMusicCDTrack(String(argv[2]).toInt(), looped);
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

    System::get().stopMusic();
    return true;
}

D_CMD(PauseMusic)
{
    DENG2_UNUSED3(src, argc, argv);

    System::get().pauseMusic(!System::get().musicIsPaused());
    return true;
}

static void sfxReverbStrengthChanged()
{
    System::get().requestListenerUpdate();
}

static void musicMidiFontChanged()
{
    System::get().updateMusicMidiFont();
}
#endif

void System::consoleRegister()  // static
{
#ifdef __CLIENT__
    // Drivers:
    C_CMD("listaudiodrivers",   nullptr, ListDrivers);
    C_CMD("inspectaudiodriver", "s",     InspectDriver);

    // Sound effects:
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
    audio::System::get().pauseMusic(paused);
#else
    DENG2_UNUSED(paused);
#endif
}

void S_StopMusic()
{
#ifdef __CLIENT__
    audio::System::get().stopMusic();
#endif
}

dd_bool S_StartMusicNum(dint musicId, dd_bool looped)
{
#ifdef __CLIENT__
    if(musicId >= 0 && musicId < ::defs.musics.size())
    {
        return audio::System::get().playMusic(::defs.musics[musicId], looped);
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

// Sounds: ------------------------------------------------------------------------------

dd_bool S_SoundIsPlaying(dint soundId, mobj_t *emitter)
{
#ifdef __CLIENT__
    return (dd_bool) audio::System::get().soundIsPlaying(soundId, emitter);
#else
    // Use the logic sound hash to determine whether the referenced sound is being
    // played currently. We don't care whether its audible or not.
    return (dd_bool) audio::System::get().logicalIsPlaying(soundId, emitter);
#endif
}

/**
 * @param soundId  @c 0= stop all sounds originating from the given @a emitter.
 * @param emitter  @c nullptr: stops all sounds with the given @a soundId. Otherwise
 *                 both @a soundId and @a emitter must match.
 * @param flags    @ref soundStopFlags.
 */
static void stopSound(dint soundId, struct mobj_s *emitter, dint flags = 0);

/**
 * @param sectorEmitter  Sector in which to stop sounds.
 * @param soundId        Unique identifier of the sound to be stopped.
 *                       If @c 0, ID not checked.
 * @param flags          @ref soundStopFlags
 */
static void stopSoundsInEmitterChain(ddmobj_base_t *sectorEmitter, dint soundId, dint flags)
{
    if(!sectorEmitter || !flags)
        return;

    // Are we stopping with this sector's emitter?
    if(flags & SSF_SECTOR)
    {
        stopSound(soundId, (mobj_t *)sectorEmitter);
    }

    // Are we stopping with linked emitters?
    if(!(flags & SSF_SECTOR_LINKED_SURFACES))
        return;

    // Process the rest of the emitter chain.
    ddmobj_base_t *base = sectorEmitter;
    while((base = (ddmobj_base_t *)base->thinker.next))
    {
        // Stop sounds from this emitter.
        stopSound(soundId, (mobj_t *)base);
    }
}

static void stopSound(dint soundId, mobj_t *emitter, dint flags)
{
    // Are we performing any special stop behaviors?
    if(emitter && flags)
    {
        if(emitter->thinker.id)
        {
            // Emitter is a real Mobj.
            stopSoundsInEmitterChain(&Mobj_Sector(emitter)->soundEmitter(), soundId, flags);
            return;
        }

        // The head of the chain is the sector. Find it.
        while(emitter->thinker.prev)
        {
            emitter = (mobj_t *)emitter->thinker.prev;
        }
        stopSoundsInEmitterChain((ddmobj_base_t *)emitter, soundId, flags);
        return;
    }

    // No special stop behavior.
#ifdef __CLIENT__
    audio::System::get().channels().stopWithLowerPriority(soundId, emitter, -1);
#endif

    // Notify the LSM.
    if(audio::System::get().stopLogical(soundId, emitter))
    {
#ifdef __SERVER__
        // In netgames, the server is responsible for telling clients when to stop sounds.
        // The LSM will tell us if a sound was stopped somewhere in the world.
        Sv_StopSound(soundId, emitter);
#endif
    }
}

void S_StopSound2(dint soundId, mobj_t *emitter, dint flags)
{
    stopSound(soundId, emitter, flags);
}

void S_StopSound(dint soundId, mobj_t *emitter)
{
    S_StopSound2(soundId, emitter, 0/*flags*/);
}

dint S_LocalSoundAtVolumeFrom(dint soundIdAndFlags, mobj_t *emitter, coord_t *origin,
    dfloat volume)
{
#ifdef __CLIENT__
    return audio::System::get().playSound(soundIdAndFlags, emitter, origin, volume);
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
    audio::System::get().startLogical(soundIdAndFlags, emitter);

    return S_LocalSound(soundIdAndFlags, emitter);
}

dint S_StartSoundEx(dint soundIdAndFlags, mobj_t *emitter)
{
#ifdef __SERVER__
    Sv_Sound(soundIdAndFlags, emitter, SVSF_TO_ALL | SVSF_EXCLUDE_ORIGIN);
#endif
    audio::System::get().startLogical(soundIdAndFlags, emitter);

    return S_LocalSound(soundIdAndFlags, emitter);
}

dint S_StartSoundAtVolume(dint soundIdAndFlags, mobj_t *emitter, dfloat volume)
{
#ifdef __SERVER__
    Sv_SoundAtVolume(soundIdAndFlags, emitter, volume, SVSF_TO_ALL);
#endif
    audio::System::get().startLogical(soundIdAndFlags, emitter);

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
