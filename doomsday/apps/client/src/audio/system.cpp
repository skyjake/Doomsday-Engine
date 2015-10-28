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

#include "api_sound.h"
#include "audio/drivers/dummydriver.h"
#include "audio/drivers/plugindriver.h"
#include "audio/drivers/sdlmixerdriver.h"
#include "audio/mixer.h"
#include "audio/mus.h"
#include "audio/samplecache.h"
#include "audio/sound.h"
#include "audio/stage.h"

#include "api_map.h"
#include "world/p_players.h"  // consolePlayer
#include "world/thinkers.h"
#include "Sector"

#include "dd_main.h"   // App_*System()
#include "dd_share.h"  // SF_* flags
#include "def_main.h"  // ::defs

#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <doomsday/defs/music.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <de/App>
#include <de/LibraryFile>
#include <de/timer.h>
#include <QMap>
#include <QMutableStringListIterator>
#include <QStringList>
#include <QtAlgorithms>

using namespace de;

#ifdef MACOSX
/// Built-in QuickTime audio interface implemented by MusicPlayer.m
DENG_EXTERN_C audiointerface_music_t audiodQuickTimeMusic;
#endif

dint sfxBits = 8;
dint sfxRate = 11025;

namespace audio {

static dint const CHANNEL_COUNT_DEFAULT  = 16;
static dint const CHANNEL_COUNT_MAX      = 256;
static dint const CHANNEL_2DCOUNT        = 4;

static String const MUSIC_BUFFEREDFILE   ("dd-buffered-song");

static bool sfxNoRndPitch;  ///< @c true= Pitch randomization is disabled. @todo should be a cvar.

// Console variables:
static dint sfxVolume = 255 * 2/3;
static dint sfx16Bit;
static dint sfxSampleRate = 11025;
static dint sfx3D;
static byte sfxOneSoundPerEmitter;  ///< @c false= Traditional Doomsday behavior: allow sounds to overlap.

static dint musVolume = 255 * 2/3;
static char *musMidiFontPath = (char *) "";
// When multiple sources are available this setting determines which to use (mus < ext < cd).
static audio::MusicSource musSourcePriority = audio::MUSP_EXT;

static audio::System *theAudioSystem;

/**
 * Usually the display player.
 */
static mobj_t *getListenerMob()
{
    if(App_WorldSystem().hasMap())
    {
        return DD_Player(::displayPlayer)->publicData().mo;
    }
    return nullptr;
}

String MusicSourceAsText(MusicSource source)  // static
{
    switch(source)
    {
    case MUSP_MUS: return "MUS lumps";
    case MUSP_EXT: return "External files";
    case MUSP_CD:  return "CD";

    default: DENG2_ASSERT(!"Unknown MusicSource"); break;
    }
    return "(invalid)";
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
    return "(invalid)";
}

String System::IDriver::description() const
{
    auto desc = String(     _E(b) "%1" _E(.)
                       "\n" _E(l) "IdentityKey: " _E(.) "%2")
                  .arg(title())
                  .arg(identityKey().replace(';', " | "));

    // Summarize playback interfaces.
    QList<Record> const interfaces = listInterfaces();
    if(!interfaces.isEmpty())
    {
        desc += "\n" _E(D)_E(b) "Playback interfaces:";

        String pSummary;
        for(Record const &rec : interfaces)
        {
            if(!pSummary.isEmpty()) pSummary += "\n" _E(0);
            pSummary += " - " + playbackInterfaceTypeAsText(PlaybackInterfaceType(rec.geti("type")))
                      + ": " _E(>) + rec.gets("identityKey") + _E(<);
        }
        desc += "\n" _E(.)_E(.) + pSummary;
    }

    // Finally, the high-level status of the driver.
    desc += "\n" _E(D)_E(b) "Status: " _E(.) + statusAsText();

    return desc;
}

String System::IDriver::playbackInterfaceTypeAsText(PlaybackInterfaceType type)  // static
{
    switch(type)
    {
    case AUDIO_ICD:    return "CD";
    case AUDIO_IMUSIC: return "Music";
    case AUDIO_ISFX:   return "SFX";

    default: DENG2_ASSERT(!"Unknown PlaybackInterfaceType"); break;
    }
    return "(unknown)";
}

// --------------------------------------------------------------------------------------

/**
 * @todo Simplify architecture - load the "dummy" driver, always. -ds
 *
 * @todo The playback interface could also declare which audio formats it is capable of
 * playing (e.g., MIDI only, CD tracks only). -jk
 */
DENG2_PIMPL(System)
, DENG2_OBSERVES(App, GameUnload)
{
    /// Required/referenced playback interface is missing. @ingroup errors
    DENG2_ERROR(MissingPlaybackInterfaceError);

    SettingsRegister settings;
    QList<IDriver *> drivers;  //< All loaded audio drivers.

    /// All indexed playback interfaces.
    typedef QMap<String /*key: identity key*/, Record> PlaybackInterfaceMap;
    PlaybackInterfaceMap interfaces[IDriver::PlaybackInterfaceTypeCount];

    class ActiveInterface
    {
        Record *_def = nullptr;

    public:
        IDriver *driver = nullptr;
        IPlayer *player = nullptr;

        ActiveInterface(Record &def, IDriver *driver, IPlayer *player)
            : _def(&def), driver(driver), player(player)
        {}

        Record &def() const
        {
            DENG2_ASSERT(_def);
            return *_def;
        }

        inline IDriver::PlaybackInterfaceType type() const
        {
            return IDriver::PlaybackInterfaceType( def().geti("type") );
        }

        bool initialize()
        {
            if(player)
            {
                return CPP_BOOL( player->initialize() );
            }
            return true;
        }

        void deinitialize()
        {
            if(player)
            {
                player->deinitialize();
            }
        }

        // Note: Drivers retain ownership of channels.
        Channel *makeChannel()
        {
            DENG2_ASSERT(driver);
            return driver->makeChannel(type());
        }
        
        void allowRefresh(bool allow)
        {
            if(player && type() == IDriver::AUDIO_ISFX)
            {
                player->as<ISoundPlayer>().allowRefresh(allow);
            }
        }
    };
    QList<ActiveInterface> activeInterfaces;  //< Initialization order.

    Record &findInterface(IDriver::PlaybackInterfaceType type, DotPath const &identityKey)
    {
        if(auto *found = tryFindInterface(type, identityKey)) return *found;
        /// @throw MissingPlaybackInterfaceError  Unknown type & identity key pair specified.
        throw MissingPlaybackInterfaceError("audio::System::Instance::findInterface", "Unknown interface identity key \"" + identityKey + "\"");
    }

    Record *tryFindInterface(IDriver::PlaybackInterfaceType type, DotPath const &identityKey)
    {
        DENG2_ASSERT(dint(type) >= 0 && dint(type) < IDriver::PlaybackInterfaceTypeCount);
        auto found = interfaces[dint(type)].find(identityKey.toStringRef().toLower());
        if(found != interfaces[dint(type)].end()) return &found.value();
        return nullptr;
    }

    Record &addInterface(Record const &rec)
    {
        dint type = rec.geti("type");
        DENG2_ASSERT(type >= 0 && type < IDriver::PlaybackInterfaceTypeCount);
        return interfaces[type].insert(rec.gets("identityKey"), rec).value();  // a copy is made
    }

    bool interfaceIsActive(Record const &interfaceDef)
    {
        for(ActiveInterface const &active : activeInterfaces)
        {
            if(&active.def() == &interfaceDef) return true;
        }
        return false;
    }

    IDriver &findDriver(String driverIdKey)
    {
        if(IDriver *driver = tryFindDriver(driverIdKey)) return *driver;
        /// @throw MissingDriverError  Unknown driver identifier specified.
        throw MissingDriverError("audio::System::findDriver", "Unknown audio driver '" + driverIdKey + "'");
    }

    IDriver *tryFindDriver(String driverIdKey)
    {
        driverIdKey = driverIdKey.toLower();  // Symbolic identity keys are lowercase.

        for(IDriver *driver : drivers)
        for(QString const &idKey : driver->identityKey().split(';'))
        {
            if(idKey == driverIdKey)
                return driver;
        }
        return nullptr;
    }

    /**
     * @param driver  Audio driver to add. Ownership is given.
     */
    void addDriver(IDriver *driver)
    {
        if(!driver) return;

        // Have we already indexed (and taken ownership of) this driver?
        if(drivers.contains(driver)) return;

        // Reject this driver if it's identity key(s) is not unique.
        for(IDriver const *other : drivers)
        for(QString const &otherIdKey : other->identityKey().split(';'))
        {
            for(QString const &idKey : driver->identityKey().split(';'))
            {
                if(otherIdKey == idKey)
                {
                    LOGDEV_AUDIO_WARNING("Audio driver \"%s\" is already attributed with the"
                                         " identity key \"%s\" (must be unique) - cannot add driver \"%s\"")
                        << other->title()
                        << otherIdKey
                        << driver->title();

                    delete driver;
                    return;
                }
            }
        }

        // Add the new driver to the collection.
        drivers << driver;

        // Index (and validate) playback interfaces.
        for(Record const &rec : driver->listInterfaces())
        {
            DotPath const idKey(rec.gets("identityKey"));
            auto const type = IDriver::PlaybackInterfaceType( rec.geti("type") );

            // Ensure the identity key for this interface is well-formed.
            if(idKey.segmentCount() < 2 || idKey.firstSegment() != driver->identityKey().split(';').first())
            {
                LOGDEV_AUDIO_WARNING("Playback interface identity key \"%s\" for driver \"%s\""
                                     " is malformed (expected \"<driverIdentityKey>.<interfaceIdentityKey>\")"
                                     " - cannot add interface")
                    << idKey << driver->identityKey();
                continue;
            }

            // Driver interface identity keys must be unique.
            if(tryFindInterface(type, idKey))
            {
                LOGDEV_AUDIO_WARNING("A playback interface with identity key \"%s\" already"
                                     " exists (must be unique) - cannot add interface")
                    << idKey;
                continue;
            }

            // Seems legit...
            addInterface(rec);  // A copy is made.
        }
    }

    void unloadDrivers()
    {
        // Deinitialize all loaded drivers we have since initialized.
        // As each driver may provide multiple interfaces, which, may be initialized in any
        // order - the initialization order is reverse earliest in the active interface order.
        QList<IDriver *> reverseInitOrder;
        for(ActiveInterface const &active : activeInterfaces)
        {
            if(IDriver *driver = active.driver)
            {
                if(!reverseInitOrder.contains(driver))
                    reverseInitOrder.prepend(driver);
            }
        }
        for(IDriver *driver : reverseInitOrder)
        {
            LOG_AUDIO_VERBOSE("Deinitializing audio driver '%s'...") << driver->identityKey().split(';').first();
            driver->deinitialize();
        }
        activeInterfaces.clear();

        // Clear the interface database.
        for(auto &interfaceMap : interfaces) { interfaceMap.clear(); }

        // Finally, unload all the drivers.
        qDeleteAll(drivers);
        drivers.clear();
    }

    void loadDrivers()
    {
        DENG2_ASSERT(activeInterfaces.isEmpty());
        DENG2_ASSERT(drivers.isEmpty());
        DENG2_ASSERT(!App::commandLine().has("-nosound"));

        // Firstly - built-in drivers.
        addDriver(new DummyDriver);
#ifndef DENG_DISABLE_SDLMIXER
        addDriver(new SdlMixerDriver);
#endif

        // Secondly - plugin drivers.
        Library_forAll([this] (LibraryFile &libFile)
        {
            if(libFile.name().beginsWith("audio_", Qt::CaseInsensitive))
            {
                if(auto *driver = PluginDriver::interpretFile(&libFile))
                {
                    addDriver(driver);  // Takes ownership.
                }
                else
                {
                    LOGDEV_AUDIO_ERROR("\"%s\" is not a valid audio driver plugin")
                        << NativePath(libFile.path()).pretty();;
                }
            }
            return LoopContinue;
        });
    }

    /**
     * Lookup the user's preferred priority order for playback interfaces of the given
     * @a type (from Config).
     *
     * @return  ';' delimited listing of player interface identity keys, from least to
     * most preferred.
     *
     * @todo Actually store it persistently (in Config). -ds
     */
    String interfacePriority(IDriver::PlaybackInterfaceType type)
    {
        String list;

        String arg;
        switch(type)
        {
        case IDriver::AUDIO_ICD:    arg = "-icd";    break;
        case IDriver::AUDIO_IMUSIC: arg = "-imusic"; break;
        case IDriver::AUDIO_ISFX:   arg = "-isfx";   break;
        }

        CommandLine &cmdLine = App::commandLine();
        for(dint p = 1; p < cmdLine.count() - 1; ++p)
        {
            if(cmdLine.isOption(p) && cmdLine.matches(arg, cmdLine.at(p)))
            {
                if(!list.isEmpty())
                    list += ";";
                list += cmdLine.at(++p);
            }
        }

        return list;
    }

    /**
     * Lookup the user's preferred sound channel limit (from Config).
     *
     * @todo Actually store it persistently (in Config). -ds
     */
    dint maxSoundChannels()
    {
        // The -sfxchan option can be used to override the default.
        if(auto arg = App::commandLine().check("-sfxchan", 1))
        {
            return String(App::commandLine().at(arg.pos + 1)).toInt();
        }
        return CHANNEL_COUNT_DEFAULT;
    }

    /**
     * Sanitizes the given playback interface @a priorityList.
     *
     * Warnings are logged for any unknown drivers and/or playback interfaces encounterd
     * (we don't care whether they are initialized at this point).
     *
     * Duplicate/unsuitable items are removed automatically.
     *
     * @param type          Playback interface type requirement.
     * @param priorityList  Playback interface identity keys (delimited with ';').
     *
     * @return  Sanitized list of playback interfaces in the order given.
     */
    QStringList parseInterfacePriority(IDriver::PlaybackInterfaceType type, String priorityList)
    {
        priorityList = priorityList.lower();  // Identity keys are always lowercase.

        QStringList list = priorityList.split(';', QString::SkipEmptyParts);

        // Resolve identity keys (drivers presently allow aliases...) and prune any
        // unknown drivers.
        QMutableStringListIterator it(list);
        while(it.hasNext())
        {
            DotPath idKey(String(it.next()).strip());

            // Resolve driver identity key aliases.
            if(idKey.segmentCount() > 1)
            {
                if(IDriver const *driver = tryFindDriver(idKey.firstSegment()))
                {
                    idKey = DotPath(driver->identityKey().split(';').first())
                          / idKey.toStringRef().mid(idKey.firstSegment().length() + 1);
                }
            }

            // Do we know this playback interface?
            if(Record const *foundInterface = tryFindInterface(type, idKey))
            {
                it.setValue(foundInterface->gets("identityKey"));
            }
            else
            {
                // Not suitable.
                it.remove();

                LOG_AUDIO_WARNING("Unknown %s playback interface \"%s\"")
                    << IDriver::playbackInterfaceTypeAsText(type)
                    << idKey;
            }
        }

        list.removeDuplicates();  // Eliminate redundancy.

        return list;
    }

    IPlayer &getPlayer(IDriver::PlaybackInterfaceType type, DotPath const &identityKey)
    {
        if(identityKey.segmentCount() > 1)
        {
            IDriver const &driver = findDriver(identityKey.segment(0));
            IPlayer &player = driver.findPlayer(identityKey.segment(1));
            // Ensure this player is of the expected type.
            if(type != IDriver::AUDIO_ISFX || player.is<ISoundPlayer>())
            {
                return player;
            }
        }
        // Internal bookkeeping error: No such player found!
        throw Error("audio::System::Instance::getPlayer", "Failed to locate " + IDriver::playbackInterfaceTypeAsText(type) + " player for \"" + identityKey + "\"");
    }

    inline IPlayer &getPlayerFor(Record const &ifs)
    {
        return getPlayer(IDriver::PlaybackInterfaceType( ifs.geti("type") ), ifs.gets("identityKey"));
    }

    IDriver &initDriverIfNeeded(IDriver &driver)
    {
        if(!driver.isInitialized())
        {
            LOG_AUDIO_VERBOSE("Initializing audio driver '%s'...") << driver.identityKey().split(';').first();
            driver.initialize();
            if(!driver.isInitialized())
            {
                /// @todo Why, exactly? (log it!) -ds
                LOG_AUDIO_WARNING("Failed initializing audio driver '%s'") << driver.identityKey().split(';').first();
            }
        }
        return driver;
    }

    /**
     * Activate the playback interface associated with the given @a interfaceDef if it is
     * not already activated.
     */
    void activateInterface(Record const &interfaceDef)
    {
        // Have we already activated the associated interface?
        if(interfaceIsActive(interfaceDef))
            return;

        try
        {
            IPlayer &player = getPlayerFor(interfaceDef);

            // If this interface belongs to a driver - ensure that the driver is initialized
            // before activating the interface.
            IDriver *driver = nullptr;
            DotPath const idKey = interfaceDef.gets("identityKey");
            if(idKey.segmentCount() > 1)
            {
                driver = tryFindDriver(idKey.firstSegment());
                if(driver)
                {
                    initDriverIfNeeded(*driver);
                    if(!driver->isInitialized())
                        return;
                }
            }

            ActiveInterface active(const_cast<Record &>(interfaceDef), driver, &player);
            activeInterfaces.append(active); // A copy is made.
        }
        catch(IDriver::UnknownInterfaceError const &er)
        {
            // Log but otherwise ignore this error.
            LOG_AUDIO_ERROR("") << er.asText();
        }
    }

    /**
     * Activate all user-preferred playback interfaces of the given @a type, if they are
     * not already activated.
     */
    void activateInterfaces(IDriver::PlaybackInterfaceType type)
    {
        for(DotPath const idKey : parseInterfacePriority(type, interfacePriority(type)))
        {
            try
            {
                activateInterface(findInterface(type, idKey));
            }
            catch(MissingPlaybackInterfaceError const &er)
            {
                // Log but otherwise ignore this error.
                LOG_AUDIO_ERROR("") << er.asText();
            }
        }
    }

    /**
     * Activate all user-preferred playback interfaces of @em all types, if they are not
     * already activated.
     */
    void activateInterfaces()
    {
        for(dint i = 0; i < IDriver::PlaybackInterfaceTypeCount; ++i)
        {
            activateInterfaces(IDriver::PlaybackInterfaceType( i ));
        }
    }

    /**
     * Perform initialization for music playback.
     */
    void initMusic()
    {
        // Already been here?
        if(musicAvail) return;

        LOG_AUDIO_VERBOSE("Initializing music playback...");

        musicAvail       = false;
        musicCurrentSong = "";
        musicPaused      = false;

        CommandLine &cmdLine = App::commandLine();
        if(cmdLine.has("-nomusic"))
        {
            LOG_AUDIO_NOTE("Music disabled");
            return;
        }

        // Initialize interfaces for music playback.
        dint initialized = 0;
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface &active = activeInterfaces[i];
            if(   active.type() != IDriver::AUDIO_IMUSIC
               && active.type() != IDriver::AUDIO_ICD)
                continue;

            if(active.initialize())
            {
                initialized += 1;
            }
            else
            {
                LOG_AUDIO_WARNING("Failed to initialize \"%s\" for music playback")
                    << active.def().gets("identityKey");
            }
        }

        // Remember whether an interface for music playback initialized successfully.
        musicAvail = initialized >= 1;
        if(musicAvail)
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
        if(!musicAvail) return;
        musicAvail = false;

        // Shutdown interfaces.
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface &active = activeInterfaces[i];
            if(   active.type() == IDriver::AUDIO_ICD
               || active.type() == IDriver::AUDIO_IMUSIC)
            {
                active.deinitialize();
            }
        }
    }

    /**
     * Perform initialization for sound playback.
     */
    void initSound()
    {
        // Already initialized?
        if(soundAvail) return;

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
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface &active = activeInterfaces[i];
            if(active.type() != IDriver::AUDIO_ISFX)
                continue;

            if(active.initialize())
            {
                initialized += 1;
            }
            else
            {
                LOG_AUDIO_WARNING("Failed to initialize \"%s\" for sound playback")
                    << active.def().gets("identityKey");
            }
        }

        // Remember whether an interface for sound playback initialized successfully.
        soundAvail = initialized >= 1;

        // Disable environmental audio effects by default.
        worldStage.listener().useEnvironment(false);
    }

    /**
     * Perform deinitialization for sound playback.
     */
    void deinitSound()
    {
        // Not initialized?
        if(!soundAvail) return;

        soundAvail = false;

        // Shutdown active interfaces.
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface &active = activeInterfaces[i];
            if(active.type() == IDriver::AUDIO_ISFX)
            {
                active.deinitialize();
            }
        }
    }

    bool musicAvail = false;                 ///< @c true if one or more interfaces are initialized for music playback.
    bool soundAvail = false;                 ///< @c true if one or more interfaces are initialized for sound playback.

    bool musicPaused = false;
    String musicCurrentSong;
    bool musicNeedSwitchBufferFile = false;  ///< @c true= choose a new file name for the buffered playback file when asked. */

    Stage worldStage;

    SampleCache sampleCache;
    std::unique_ptr<Mixer> mixer;

    Instance(Public *i) : Base(i)
    {
        theAudioSystem = thisPublic;

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
    }

    ~Instance()
    {
        App::app().audienceForGameUnload() -= this;

        theAudioSystem = nullptr;
    }
    
    String composeMusicBufferFilename(String const &ext = "")
    {
        // Switch the name of the buffered song file?
        static dint currentBufFile = 0;
        if(musicNeedSwitchBufferFile)
        {
            currentBufFile ^= 1;
            musicNeedSwitchBufferFile = false;
        }

        // Compose the name.
        return MUSIC_BUFFEREDFILE + String::number(currentBufFile) + ext;
    }

    /**
     * Destroys and then recreates the  Mixer according to the current mode.
     */
    void initMixer()
    {
        /// @todo Ensure existing channels have been released! -ds

        // Replace the mixer (we'll reconfigure).
        mixer.reset(new Mixer);
        mixer->makeTrack("music").setTitle("Music");
        mixer->makeTrack("fx"   ).setTitle("Effects");

        /// @todo Defer channel construction until asked to play. Need to handle channel
        /// lifetime and positioning mode switches dynamically. -ds
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface &active = activeInterfaces[i];
            switch(active.type())
            {
            case IDriver::AUDIO_ICD:
            case IDriver::AUDIO_IMUSIC:
                (*mixer)["music"].addChannel(active.makeChannel());
                break;

            case IDriver::AUDIO_ISFX:
                if((*mixer)["fx"].channelCount() == 0)
                {
                    worldStage.listener().useEnvironment(sfx3D);

                    dint const maxChannels = de::clamp(1, maxSoundChannels(), CHANNEL_COUNT_MAX);
                    dint numStereo = sfx3D ? CHANNEL_2DCOUNT : maxChannels;  // The rest will be 3D.
                    for(dint i = 0; i < maxChannels; ++i)
                    {
                        Positioning const positioning = numStereo-- > 0 ? StereoPositioning : AbsolutePositioning;
                        Channel *channel = active.makeChannel();
                        if(!channel)
                        {
                            dint const numAvailable = (*mixer)["fx"].channelCount();
                            LOG_AUDIO_WARNING("\"%s\" failed creating new Channel (for sound effects)"
                                              ". Sound playback will be ")
                                << active.def().gets("identityKey")
                                << (numAvailable > 0 ? String("limited to %1 channels").arg(numAvailable)
                                                     : "unavailable");
                            break;
                        }
                        if(!channel->as<SoundChannel>().format(positioning, sfxBits / 8, sfxRate))
                        {
                            LOG_AUDIO_WARNING("\"%s\" failed configuring Channel format")
                                << active.def().gets("identityKey");
                            break;
                        }

                        // Add the new channel to the available channels for the "fx" track.
                        (*mixer)["fx"].addChannel(channel);
                    }

                    LOG_AUDIO_NOTE("Using %i sound channels") << (*mixer)["fx"].channelCount();
                }
                break;

            default: break;
            }
        }
    }

    /**
     * Returns the total number of sound channels currently playing a/the sound associated
     * with the given @a soundId and/or @a emitter.
     */
    dint countSoundChannelsPlaying(dint soundId, SoundEmitter *emitter = nullptr) const
    {
        dint count = 0;
        (*mixer)["fx"].forAllChannels([&soundId, &emitter, &count] (Channel &base)
        {
            auto &ch = base.as<SoundChannel>();

            if(!ch.isPlaying()) return LoopContinue;
            if(emitter && ch.emitter() != emitter) return LoopContinue;
            if(soundId && ch.samplePtr()->soundId != soundId) return LoopContinue;

            // Once playing, repeating sounds don't stop.
            /*if(ch.playingMode() == Channel::Looping)
            {
                // Check time. The flag is updated after a slight delay (only at refresh).
                dint const ticsToDelay = ch.samplePtr()->numSamples / dfloat( ch.buffer().freq ) * TICSPERSEC;
                if(Timer_Ticks() - ch.startTime() < ticsToDelay)
                    return LoopContinue;
            }*/
 
            count += 1;
            return LoopContinue;
        });
        return count;
    }

    void getSoundChannelPriorities(Listener *listener, QList<dfloat> &prios) const
    {
        dint idx = 0;
        (*mixer)["fx"].forAllChannels([&listener, &prios] (Channel &base)
        {
            auto &ch = base.as<SoundChannel>();
            if(!ch.isPlaying())
            {
                prios.append(SFX_LOWEST_PRIORITY);
            }
            else
            {
                ddouble vec[3], *origin = nullptr;
                if(!(ch.flags() & SFXCF_NO_ORIGIN))
                {
                    ch.origin().decompose(vec);
                    origin = vec;
                }

                prios.append(Sound::ratePriority(ch.volume(), nullptr/*emitter*/, origin,
                                                 ch.startTime(), listener));
            }
            return LoopContinue;
        });
    }

    /**
     * Stop all sound channels currently playing a/the sound associated with the given
     * sound @a group. If an @a emitter is specified, only stop sounds emitted by it.
     *
     * @param group    Sound group identifier.
     * @param emitter  If not @c nullptr the referenced sound's emitter must match.
     *
     * @return  Number of channels stopped.
     */
    dint stopSoundChannelsWithSoundGroup(dint group, SoundEmitter *emitter)
    {
        dint stopCount = 0;
        (*mixer)["fx"].forAllChannels([&group, &emitter, &stopCount] (Channel &base)
        {
            auto &ch = base.as<SoundChannel>();

            if(!ch.isPlaying()) return LoopContinue;
            if(ch.samplePtr()->group != group) return LoopContinue;
            if(emitter && ch.emitter() != emitter) return LoopContinue;

            // This channel must be stopped!
            ch.stop();
            stopCount += 1;
            return LoopContinue;
        });
        return stopCount;
    }

    /**
     * Stop all sound channels currently playing a/the sound with the specified @a emitter.
     *
     * @param emitter       If not @c nullptr the sound's emitter must match; otherwise
     * stop @em all sounds using @em any emitter.
     *
     * @param clearEmitter  If @c true, clear the sound -> emitter association for any
     * matching Sounds that are stopped.
     *
     * @return  Number of channels stopped.
     */
    dint stopSoundChannelsWithEmitter(SoundEmitter *emitter = nullptr, bool clearEmitter = true)
    {
        dint stopCount = 0;
        (*mixer)["fx"].forAllChannels([&emitter, &clearEmitter, &stopCount] (Channel &base)
        {
            auto &ch = base.as<SoundChannel>();

            //if(!ch.isPlaying()) return LoopContinue;

            if(!ch.emitter() || (!emitter || ch.emitter() == emitter))
            {
                // This channel must be stopped!.
                ch.stop();
                stopCount += 1;

                if(clearEmitter)
                {
                    ch.setEmitter(nullptr);
                }
            }
            return LoopContinue;
        });
        return stopCount;
    }

    /**
     * Stop all sound channels currently playing a/the sound with a lower priority rating.
     *
     * @param soundId      If > 0, the currently playing sound must be associated with
     * this identifier; otherwise @em all sounds are stopped.
     *
     * @param emitter      If not @c nullptr the referenced sound's emitter must match.
     *
     * @param defPriority  If >= 0, the currently playing sound must have a lower priority
     * than this to be stopped. Returns -1 if the sound @a id has a lower priority than
     * a currently playing sound.
     *
     * @return  Number of channels stopped.
     */
    dint stopSoundChannelsWithLowerPriority(dint soundId, SoundEmitter *emitter, dint defPriority)
    {
        dint stopCount = 0;
        (*mixer)["fx"].forAllChannels([&soundId, &emitter, &defPriority, &stopCount] (Channel &base)
        {
            auto &ch = base.as<SoundChannel>();

            if(!ch.isPlaying()) return LoopContinue;
            
            if(   (soundId && ch.samplePtr()->soundId != soundId)
               || (emitter && ch.emitter() != emitter))
            {
                return LoopContinue;
            }

            // Can it be stopped?
            if(ch.mode() == Channel::OnceDontDelete)
            {
                // The emitter might get destroyed...
                ch.setEmitter(nullptr);
                ch.setFlags(ch.flags() | (SFXCF_NO_UPDATE | SFXCF_NO_ORIGIN));
                return LoopContinue;
            }

            // Check the priority.
            if(defPriority >= 0)
            {
                dint oldPrio = ::defs.sounds[ch.samplePtr()->soundId].geti("priority");
                if(oldPrio < defPriority)  // Old is more important.
                {
                    stopCount = -1;
                    return LoopAbort;  /// @todo Really?? -ds
                }
            }

            // This channel must be stopped!
            ch.stop();
            stopCount += 1;
            return LoopContinue;
        });
        return stopCount;
    }

    /**
     * Attempt to find a vacant SoundChannel suitable for playing a new sound with the
     * format specified.
     *
     * @param positioning  Positioning mode for environmental audio (if enabled).
     * @param bytesPer     Number of bytes per sample.
     * @param rate         Playback frequence/rate in Hz.
     *
     * @param soundId   If > 0, the channel must currently be loaded with a/the sound
     * associated with this identifier.
     */
    SoundChannel *vacantSoundChannel(Positioning positioning, dint bytesPer, dint rate,
        dint soundId) const
    {
        SoundChannel *found = nullptr;  // None suitable.
        (*mixer)["fx"].forAllChannels([&positioning, &bytesPer, &rate, &soundId, &found]
                                      (Channel &base)
        {
            auto &ch = base.as<SoundChannel>();

            if(ch.isPlaying()) return LoopContinue;

            if(   ch.positioning() != positioning 
               || ch.bytes()       != bytesPer
               || ch.rate()        != rate)
            {
                return LoopContinue;
            }

            // What about the sample?
            if(soundId > 0)
            {
                if(!ch.samplePtr() || ch.samplePtr()->soundId != soundId)
                    return LoopContinue;
            }
            else if(soundId == 0)
            {
                // We're trying to find a channel with no sample already loaded.
                if(ch.samplePtr())
                    return LoopContinue;
            }

            // This is perfect, take this!
            found = &ch;
            return LoopAbort;
        });
        return found;
    }

    dint playMusicFile(String const &virtualOrNativePath, bool looped = false)
    {
        DENG2_ASSERT(musicAvail);

        if(virtualOrNativePath.isEmpty())
            return 0;

        // Relative paths are relative to the native working directory.
        String const path  = (NativePath::workPath() / NativePath(virtualOrNativePath).expand()).withSeparators('/');
        LOG_AUDIO_VERBOSE("Attempting to play music file \"%s\"")
            << NativePath(virtualOrNativePath).pretty();

        try
        {
            std::unique_ptr<FileHandle> hndl(&App_FileSystem().openFile(path, "rb"));
            dint didPlay = (*mixer)["music"].forAllChannels([this, &hndl, &looped] (Channel &base)
            {
                if(!base.is<MusicChannel>())
                    return LoopContinue;

                auto &ch = base.as<MusicChannel>();

                // Does this channel support buffered playback?
                if(ch.canPlayBuffer())
                {
                    try
                    {
                        // Buffer the data using the driver's own facility.
                        dsize const len = hndl->length();
                        hndl->read((duint8 *) ch.songBuffer(len), len);
                        ch.play(looped ? Channel::Looping : Channel::OnceDontDelete);
                        return LoopAbort;  // Success!
                    }
                    catch(Error const &)
                    {}
                }

                // Does this channel support playback from a native file?
                if(ch.canPlayFile())
                {
                    try
                    {
                        // Write the data to disk and play from there.
                        String const bufPath = composeMusicBufferFilename();

                        dsize len = hndl->length();
                        auto *buf = (duint8 *)M_Malloc(len);
                        hndl->read(buf, len);
                        F_Dump(buf, len, bufPath.toUtf8().constData());
                        M_Free(buf); buf = nullptr;
                        ch.bindFile(bufPath);
                        ch.play(looped ? Channel::Looping : Channel::OnceDontDelete);
                        return LoopAbort;  // Success!
                    }
                    catch(Error const &)
                    {}
                }

                return LoopContinue;
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
        DENG2_ASSERT(musicAvail);

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

            auto didPlay = (*mixer)["music"].forAllChannels([&srcFile, &looped] (Channel &base)
            {
                if(auto *ch = base.maybeAs<MusicChannel>())
                {
                    if(ch->canPlayFile())
                    {
                        try
                        {
                            ch->bindFile(srcFile);
                            ch->play(looped ? Channel::Looping : Channel::OnceDontDelete);
                            return LoopAbort;  // Success!
                        }
                        catch(Error const &)
                        {}
                    }
                }
                return LoopContinue;
            });
            if(didPlay) return 1;
        }

        return (*mixer)["music"].forAllChannels([this, &lump, &looped] (Channel &base)
        {
            if(!base.is<MusicChannel>())
                return LoopContinue;

            auto &ch = base.as<MusicChannel>();

            // Does this channel offer buffered playback?
            if(ch.canPlayBuffer())
            {
                try
                {
                    // Buffer the data using the driver's own facility.
                    std::unique_ptr<FileHandle> hndl(&App_FileSystem().openLump(lump));
                    dsize const length  = hndl->length();
                    hndl->read((duint8 *) ch.songBuffer(length), length);
                    App_FileSystem().releaseFile(hndl->file());

                    ch.play(looped ? Channel::Looping : Channel::OnceDontDelete);
                    return LoopAbort;  // Success!
                }
                catch(Error const &)
                {}
            }

            // Does this channel offer playback from a native file?
            if(ch.canPlayFile())
            {
                // Write the data to disk and play from there.
                String const fileName = composeMusicBufferFilename();
                if(F_DumpFile(lump, fileName.toUtf8().constData()))
                {
                    try
                    {
                        ch.bindFile(fileName);
                        ch.play(looped ? Channel::Looping : Channel::OnceDontDelete);
                        return LoopAbort;  // Success!
                    }
                    catch(Error const &)
                    {}
                }
            }

            return LoopContinue;
        });
    }

    dint playMusicCDTrack(dint cdTrack, bool looped)
    {
        // Assume CD track 0 is not valid.
        if(cdTrack == 0) return 0;

        // Try each of the available channels until one is successful.
        return (*mixer)["music"].forAllChannels([&cdTrack, &looped] (Channel &base)
        {
            if(auto *ch = base.maybeAs<CdChannel>())
            {
                try
                {
                    ch->bindTrack(cdTrack);
                    ch->play(looped ? Channel::Looping : Channel::OnceDontDelete);
                    return LoopAbort;  // Success!
                }
                catch(Error const &)
                {}
            }
            return LoopContinue;
        });
    }

    /**
     * Used by the high-level sound interface to play sounds on the local system.
     *
     * @note If both @a emitter and @a origin are NULL the sound will always be played with
     * @ref SterePositioning (centered).
     *
     * @param sample   Sample to play (must be stored persistently! No copy is made).
     * @param volume   Volume at which the sample should be played.
     * @param freq     Relative and modifies the sample's rate.
     * @param emitter  Sound stage SoundEmitter (of the sound, if any (may be @c nullptr).
     * @param origin   Sound stage position where the sound originated, if any (may be @c nullptr).
     * @param flags    Additional flags (@ref soundFlags).
     *
     * @return  @c true, if a sound is started.
     */
    bool playSound(sfxsample_t const &sample, dfloat volume, dfloat frequency,
        SoundEmitter *emitter, ddouble const *origin, dint flags)
    {
        if(!soundAvail) return false;
        if(sample.size == 0 || volume <= 0) return false;

        sfxinfo_t const &soundDef = ::runtimeDefs.sounds[sample.soundId];

        // Stop all other sounds with the same emitter?
        if(emitter && worldStage.exclusion() == Stage::OnePerEmitter)
        {
            if(stopSoundChannelsWithLowerPriority(0, emitter, soundDef.priority) < 0)
            {
                // Something with a higher priority is playing, can't start now.
                LOG_AUDIO_MSG("Not playing sound (id:%i emitter:%i) prio:%i because overridden")
                    << sample.soundId << emitter->thinker.id
                    << soundDef.priority;

                return false;
            }
        }

        // Determine the final attributes of the sound to be played.
        Positioning const positioning = (sfx3D && (emitter || origin)) ? AbsolutePositioning : StereoPositioning;
        dfloat const priority         = Sound::ratePriority(volume, emitter, origin,
                                                            Timer_Ticks(), &worldStage.listener());

        dfloat lowPrio = 0;

        QList<dfloat> channelPrios;
        channelPrios.reserve((*mixer)["fx"].channelCount());

        // Ensure there aren't already too many channels playing this sample.
        if(soundDef.channels > 0)
        {
            // The decision to stop channels is based on priorities.
            getSoundChannelPriorities(&worldStage.listener(), channelPrios);

            dint count = countSoundChannelsPlaying(sample.soundId);
            while(count >= soundDef.channels)
            {
                // Stop the lowest priority sound of the playing instances, again noting
                // sounds that are more important than us.
                dint idx = 0;
                SoundChannel *selCh = nullptr;
                (*mixer)["fx"].forAllChannels([&sample, &priority, &channelPrios, &selCh,
                                               &lowPrio, &idx] (Channel &base)
                {
                    auto &ch = base.as<SoundChannel>();
                    dfloat const chPriority = channelPrios[idx++];

                    if(ch.isPlaying())
                    {
                        if(   ch.samplePtr()->soundId == sample.soundId
                           && (priority >= chPriority && (!selCh || chPriority <= lowPrio)))
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
         * Pick a channel for the sound. We will do our best to play the sound, cancelling
         * existing ones if need be. The ideal choice is a free channel that is already
         * loaded with the sample, in the correct format and mode.
         */
        self.allowChannelRefresh(false);

        // First look through the stopped channels. At this stage we're very picky: only
        // the perfect choice will be good enough.
        SoundChannel *selCh = vacantSoundChannel(positioning, sample.bytesPer,
                                                 sample.rate, sample.soundId);

        if(!selCh)
        {
            // Perhaps there is a vacant channel (with any sample, but preferably one with
            // no sample already loaded).
            selCh = vacantSoundChannel(positioning, sample.bytesPer, sample.rate, 0);
        }

        if(!selCh)
        {
            // Try any non-playing channel in the correct format.
            selCh = vacantSoundChannel(positioning, sample.bytesPer, sample.rate, -1);
        }

        if(!selCh)
        {
            // A perfect channel could not be found.
            // We must use a channel with the wrong format or decide which one of the playing
            // ones gets stopped.

            if(channelPrios.isEmpty())
            {
                getSoundChannelPriorities(&worldStage.listener(), channelPrios);
            }

            // All channels with a priority less than or equal to ours can be stopped.
            SoundChannel *prioCh = nullptr;
            dint idx = 0;
            (*mixer)["fx"].forAllChannels([&positioning, &priority, &channelPrios,
                                           &selCh, &prioCh, &lowPrio, &idx] (Channel &base)
            {
                auto &ch = base.as<SoundChannel>();
                dfloat const chPriority = channelPrios[idx++];

                if(ch.isValid())
                {
                    // Sample buffer must be configured for the right mode.
                    if(positioning == ch.positioning())
                    {
                        if(!ch.isPlaying())
                        {
                            // This channel is not playing, we'll take it!
                            selCh = &ch;
                            return LoopAbort;
                        }

                        // Are we more important than this sound?
                        // We want to choose the lowest priority sound.
                        if(priority >= chPriority && (!prioCh || chPriority <= lowPrio))
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
            self.allowChannelRefresh();
            LOG_AUDIO_XVERBOSE("Failed to find suitable channel for sample id:%i") << sample.soundId;
            return false;
        }

        SoundChannel &channel = *selCh;
        DENG2_ASSERT(channel.isValid());

        // The sound may need to be reformatted.
        channel.format(positioning, sample.bytesPer, sample.rate);
        channel.setFlags(channel.flags() & ~(SFXCF_NO_ORIGIN | SFXCF_NO_ATTENUATION | SFXCF_NO_UPDATE));
        channel.setVolume(volume);
        channel.setFrequency(frequency);
        if(!emitter && !origin)
        {
            channel.setFlags(channel.flags() | SFXCF_NO_ORIGIN);
            channel.setEmitter(nullptr);
        }
        else
        {
            channel.setEmitter(emitter);
            if(origin)
            {
                channel.setOrigin(Vector3d(origin));
            }
        }
        if(flags & SF_NO_ATTENUATION)
        {
            // The sound can be heard from any distance.
            channel.setFlags(channel.flags() | SFXCF_NO_ATTENUATION);
        }

        // Update listener properties.
        getSoundPlayer().listener(SFXLP_UPDATE, 0);

        // Load in the sample if needed.
        DENG2_ASSERT(channel.isValid());
        channel.load(sample);

        // Start playing.
        channel.play(  (flags & SF_REPEAT)    ? Channel::Looping
                     : (flags & SF_DONT_STOP) ? Channel::OnceDontDelete
                                              : Channel::Once);

        // Streaming of playback data and channel updates may now continue.
        self.allowChannelRefresh();

        // Sound successfully started.
        return true;
    }

    void updateMusicVolumeIfChanged()
    {
        if(!musicAvail) return;

        static dint oldMusVolume = -1;
        if(musVolume != oldMusVolume)
        {
            oldMusVolume = musVolume;

            // Set volume of all active interfaces.
            dfloat newVolume = musVolume / 255.0f;
            (*mixer)["music"].forAllChannels([&newVolume] (Channel &ch)
            {
                ch.setVolume(newVolume);
                return LoopContinue;
            });
        }
    }

    void updateUpsampleRateIfChanged()
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
                initMixer();

                // The cache just became useless, clear it.
                sampleCache.clear();
            }
            old16Bit = sfx16Bit;
            oldRate  = sfxSampleRate;
        }
    }

    void updateSoundPositioningIfChanged()
    {
        static dint old3DMode = false;

        if(old3DMode == sfx3D) return;  // No change.

        LOG_AUDIO_VERBOSE("Switching to %s sound positioning...") << (old3DMode ? "stereo" : "3D");

        // Re-create the channel Mixer.
        initMixer();

        if(old3DMode)
        {
            // Disable environmental audio effects - we're going stereo.
            worldStage.listener().useEnvironment(false);
        }
        old3DMode = sfx3D;
    }

    /**
     * Returns the currently active, primary SoundPlayer.
     */
    ISoundPlayer &getSoundPlayer() const
    {
        // The primary interface is the first one.
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface const &active = activeInterfaces[i];
            if(active.def().geti("type") == IDriver::AUDIO_ISFX)
            {
                DENG2_ASSERT(active.player);
                return active.player->as<ISoundPlayer>();
            }
        }
        /// Internal Error: No suitable sound player is available.
        throw Error("audio::System::Instance::getSoundPlayer", "No SoundPlayer available");
    }

    void aboutToUnloadGame(game::Game const &)
    {
        self.reset();
    }

    DENG2_PIMPL_AUDIENCE(FrameBegins)
    DENG2_PIMPL_AUDIENCE(FrameEnds)
    DENG2_PIMPL_AUDIENCE(MidiFontChange)
};

DENG2_AUDIENCE_METHOD(System, FrameBegins)
DENG2_AUDIENCE_METHOD(System, FrameEnds)
DENG2_AUDIENCE_METHOD(System, MidiFontChange)

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

    String const midiFontPath(musMidiFontPath);
    os << TABBED("Music sound font:", midiFontPath.isEmpty() ? "None" : midiFontPath);
    os << TABBED("Music source priority:", MusicSourceAsText(musSourcePriority));

    os << _E(T`) "Playback interface priority:\n";
    for(dint i = d->activeInterfaces.count(); i--> 0; )
    {
        Instance::ActiveInterface &active = d->activeInterfaces[i];
        os << _E(Ta) _E(l) "  " << IDriver::playbackInterfaceTypeAsText(active.type()) << ": "
           << _E(.) _E(Tb) << active.def().gets("identityKey") << "\n";
    }

    return str.rightStrip();

#undef TABBED
}

Mixer &System::mixer() const
{
    DENG2_ASSERT(d->mixer.get() != nullptr);
    return *d->mixer;
}

SampleCache &System::sampleCache() const
{
    return d->sampleCache;
}

dint System::upsampleFactor(dint rate) const
{
    //LOG_AS("audio::System");

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

void System::resetStage(StageId stageId)
{
    LOG_AS("audio::System");

    if(stageId == WorldStage)
    {
        d->worldStage.removeAllSounds(); // Does nothing about playback (or refresh).
    }
}

Stage /*const*/ &System::worldStage() const
{
    return d->worldStage;
}

bool System::musicPlaybackAvailable() const
{
    return d->musicAvail;
}

dint System::musicVolume() const
{
    return musVolume;
}

bool System::musicIsPlaying() const
{
    //LOG_AS("audio::System");
    return mixer()["music"].forAllChannels([] (Channel &ch)
    {
        return ch.isPlaying();
    });
}

void System::stopMusic()
{
    if(!d->musicAvail) return;

    LOG_AS("audio::System");
    d->musicCurrentSong = "";

    // Stop all currently playing music channels.
    mixer()["music"].forAllChannels([] (Channel &ch)
    {
        ch.stop();
        return LoopContinue;
    });
}

void System::pauseMusic(bool doPause)
{
    if(!d->musicAvail) return;

    LOG_AS("audio::System");
    d->musicPaused = !d->musicPaused;

    // Pause all currently playing music channels.
    mixer()["music"].forAllChannels([&doPause] (Channel &ch)
    {
        if(doPause) ch.pause();
        else        ch.resume();
        return LoopContinue;
    });
}

bool System::musicIsPaused() const
{
    return d->musicPaused;
}

dint System::playMusic(Record const &definition, bool looped)
{
    if(!d->musicAvail) return false;

    LOG_AS("audio::System");
    LOG_AUDIO_MSG("Playing song \"%s\"%s...") << definition.gets("id") << (looped ? " looped" : "");

    // We will not restart the currently playing song.
    if(definition.gets("id") == d->musicCurrentSong && musicIsPlaying())
    {
        //LOG_AUDIO_MSG("..already playing (must stop to restart)");
        return false;
    }

    // Stop the currently playing song.
    stopMusic();

    // Switch to an unused file buffer if asked.
    d->musicNeedSwitchBufferFile = true;

    // This is the song we're playing now.
    d->musicCurrentSong = definition.gets("id");

    // Determine the music source, order preferences.
    dint source[3];
    source[0] = musSourcePriority;
    switch(musSourcePriority)
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

bool System::soundPlaybackAvailable() const
{
    return d->soundAvail;
}

dint System::soundVolume() const
{
    return sfxVolume;
}

bool System::soundIsPlaying(StageId stageId, dint soundId, SoundEmitter *emitter) const
{
    if(stageId == WorldStage)
    {
        // Logical sounds tell us whether a/the referenced sound is being played currently.
        // We don't care whether it is audible or not.
        return d->worldStage.soundIsPlaying(soundId, emitter);
    }
    return false;  // Not playing.
}

bool System::playSound(StageId stageId, dint soundIdAndFlags, SoundEmitter *emitter,
    coord_t const *origin,  dfloat volume)
{
    LOG_AS("audio::System");

    if(stageId == WorldStage)
    {
        // Cache the waveform resource associated with @a soundId (if necessary) so that
        // we can determine it's length.
        if(sfxsample_t const *sample = d->sampleCache.cache(soundIdAndFlags & ~DDSF_FLAG_MASK))
        {
            // Ignore zero length waveforms.
            /// @todo Shouldn't we still stop others though? -ds
            duint const length = sample->milliseconds();
            if(length > 0)
            {
                bool const repeat = (soundIdAndFlags & DDSF_REPEAT) || Def_SoundIsRepeating(sample->soundId);

                Sound sound;
                sound.id      = sample->soundId;
                sound.emitter = emitter;
                sound.looping = repeat;
                sound.endTime = Timer_RealMilliseconds() + (repeat ? 1 : length);
                d->worldStage.addSound(sound);  // A copy is made.
            }
        }
    }

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
    sfxinfo_t const *info = Def_GetSoundInfo(soundId, &freq, &volume);
    if(!info) return false;  // Hmm? This ID is not defined.

    bool const repeat = (soundIdAndFlags & DDSF_REPEAT) || Def_SoundIsRepeating(soundId);

    // Check the distance (if applicable).
    if(emitter || origin)
    {
        if(!(info->flags & SF_NO_ATTENUATION) && !(soundIdAndFlags & DDSF_NO_ATTENUATION))
        {
            // If origin is too far, don't even think about playing the sound.
            if(!d->worldStage.listener().inAudibleRangeOf(emitter ? emitter->origin : origin))
                return false;
        }
    }

    // Load the sample.
    sfxsample_t *sample = d->sampleCache.cache(soundId);
    if(!sample)
    {
        if(d->soundAvail)
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
        d->stopSoundChannelsWithSoundGroup(info->group, (info->flags & SF_GLOBAL_EXCLUDE) ? nullptr : emitter);
    }

    // Let's play it.
    dint flags = 0;
    flags |= (((info->flags & SF_NO_ATTENUATION) || (soundIdAndFlags & DDSF_NO_ATTENUATION)) ? SF_NO_ATTENUATION : 0);
    flags |= (repeat ? SF_REPEAT : 0);
    flags |= ((info->flags & SF_DONT_STOP) ? SF_DONT_STOP : 0);
    return d->playSound(*sample, volume, freq, emitter, origin, flags);
}

void System::stopSound(StageId stageId, dint soundId, SoundEmitter *emitter, dint flags)
{
    LOG_AS("audio::System");

    // Are we performing any special stop behaviors?
    if(stageId == WorldStage)
    {
        if(emitter && flags)
        {
            // Sector-based sound stopping.
            if(emitter->thinker.id)
            {
                /// @var emitter is a map-object.
                emitter = &Mobj_Sector((mobj_t *)emitter)->soundEmitter();
            }
            else
            {
                // The head of the chain is the sector. Find it.
                while(emitter->thinker.prev)
                {
                    emitter = (SoundEmitter *)emitter->thinker.prev;
                }
            }

            // Stop sounds emitted by the Sector's emitter?
            if(flags & SSF_SECTOR)
            {
                stopSound(stageId, soundId, emitter);
            }

            // Stop sounds emitted by Sector-linked (plane/wall) emitters?
            if(flags & SSF_SECTOR_LINKED_SURFACES)
            {
                // Process the rest of the emitter chain.
                while((emitter = (SoundEmitter *)emitter->thinker.next))
                {
                    // Stop sounds from this emitter.
                    stopSound(stageId, soundId, emitter);
                }
            }
            return;
        }
    }

    // No special stop behavior.
    d->stopSoundChannelsWithLowerPriority(soundId, emitter, -1);

    if(stageId == WorldStage)
    {
        // Update logical sound bookkeeping.
        if(soundId <= 0 && !emitter)
        {
            d->worldStage.removeAllSounds();
        }
        else if(soundId) // > 0
        {
            d->worldStage.removeSoundsById(soundId);
        }
        else
        {
            d->worldStage.removeSoundsWithEmitter(*emitter);
        }
    }
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

void System::reset()
{
    LOG_AS("audio::System");
    LOG_AUDIO_VERBOSE("Reseting...");

    if(d->soundAvail)
    {
        // Stop all currently playing sound channels.
        mixer()["fx"].forAllChannels([] (Channel &ch)
        {
            ch.stop();
            return LoopContinue;
        });

        // Force an Environment update for all channels.
        d->worldStage.listener().setTrackedMapObject(nullptr);
        d->worldStage.listener().setTrackedMapObject(getListenerMob());

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
        d->updateSoundPositioningIfChanged();
        d->updateUpsampleRateIfChanged();

        // Should we purge the cache (to conserve memory)?
        d->sampleCache.maybeRunPurge();
    }

    d->worldStage.setExclusion(sfxOneSoundPerEmitter ? Stage::OnePerEmitter : Stage::DontExclude);
    d->worldStage.maybeRunSoundPurge();
}

void System::endFrame()
{
    LOG_AS("audio::System");

    /// @todo Should observe. -ds
    d->worldStage.listener().setTrackedMapObject(getListenerMob());

    // Instruct currently playing Channels to write any effective Environment changes if
    // necessary (from the configured Listener of the Stage they are playing on).
    if(sfx3D && !BusyMode_Active())
    {
        mixer()["fx"].forAllChannels([] (Channel &base)
        {
            base.as<SoundChannel>().updateEnvironment();
            return LoopContinue;
        });
    }

    // Notify interested parties.
    DENG2_FOR_AUDIENCE2(FrameEnds, i) i->systemFrameEnds(*this);
}

void System::worldMapChanged()
{
    /// @todo Should observe. -ds
    d->worldStage.listener().setTrackedMapObject(getListenerMob());
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
    d->activateInterfaces();

    // Initialize sound playback.
    try
    {
        d->initSound();
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

    // Prepare the mixer.
    d->initMixer();

    // Print a summary of the active configuration to the log.
    LOG_AUDIO_MSG("%s") << description();
}

void System::deinitPlayback()
{
    LOG_AS("audio::System");

    // Clear the waveform data cache.
    d->sampleCache.clear();

    // Reset the mixer (and stop the channel refresh thread(s) if running).
    d->mixer.reset();

    d->deinitSound();
    d->deinitMusic();

    // Finally, unload the drivers.
    d->unloadDrivers();
}

void System::allowChannelRefresh(bool allow)
{
    for(dint i = d->activeInterfaces.count(); i--> 0; )
    {
        d->activeInterfaces[i].allowRefresh(allow);
    }
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
        LOG_SCR_MSG("No audio drivers are currently loaded");
        return true;
    }

    String list;
    dint numDrivers = 0;
    System::get().forAllDrivers([&list, &numDrivers] (System::IDriver const &driver)
    {
        if(!list.isEmpty()) list += "\n";

        list += String(_E(0)
                       _E(Ta) "%1%2 "
                       _E(Tb) _E(2) "%3")
                  .arg(driver.isInitialized() ? _E(B) _E(b) : "")
                  .arg(driver.identityKey().split(';').first())
                  .arg(driver.title());

        numDrivers += 1;
        return LoopContinue;
    });

    LOG_SCR_MSG(_E(b) "Loaded Audio Drivers (%i):") << numDrivers;
    LOG_SCR_MSG(_E(R) "\n");
    LOG_SCR_MSG("") << list;
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
        LOG_SCR_MSG("") << driver->description();
        return true;
    }
    LOG_SCR_WARNING("Unknown audio driver \"%s\"") << driverId;
    return false;
}

/**
 * Console command for playing a (local) sound effect.
 */
D_CMD(PlaySound)
{
    DENG2_UNUSED(src);
    LOG_AS("playsound (Cmd)");

#ifndef DENG2_DEBUG
    if(!System::get().soundPlaybackAvailable())
    {
        LOG_SCR_ERROR("Sound playback is not available");
        return false;
    }
#endif

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
    String const soundId(argv[1]);
    dint const soundNum = ::defs.getSoundNum(soundId);
    if(soundNum <= 0)
    {
        LOG_SCR_WARNING("Unknown sound \"%s\"") << soundId;
        return true;
    }

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
        _api_S.LocalSoundAtVolumeFrom(soundNum, nullptr, fixedPos, volume);
    }
    else
    {
        _api_S.LocalSoundAtVolume(soundNum, nullptr, volume);
    }

    return true;
}

/**
 * CCmd: Play a music track.
 */
D_CMD(PlayMusic)
{
    DENG2_UNUSED(src);
    LOG_AS("playmusic (Cmd)");

#ifndef DENG2_DEBUG
    if(!System::get().musicPlaybackAvailable())
    {
        LOG_SCR_ERROR("Music playback is not available");
        return false;
    }
#endif

    bool const looped = true;

    if(argc == 2)
    {
        // Play a file associated with the referenced music definition.
        if(Record const *definition = ::defs.musics.tryFind("id", argv[1]))
        {
            return System::get().playMusic(*definition, looped);
        }
        LOG_SCR_WARNING("Music '%s' not defined") << argv[1];
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

#ifdef DENG2_DEBUG
D_CMD(InspectMixer)
{
    DENG2_UNUSED3(src, argc, argv);
    Mixer &mixer = System::get().mixer();
    LOG_MSG(_E(b) "Mixer (%i tracks):") << mixer.trackCount();
    LOG_MSG(_E(R) "\n");
    mixer.forAllTracks([] (Mixer::Track &track)
    {
        LOG_MSG("%s : \"%s\" (%i channels)")
            << track.id() << track.title() << track.channelCount();
        return LoopContinue;
    });
    return true;
}
#endif

static void musicMidiFontChanged()
{
    System::get().updateMusicMidiFont();
}

void System::consoleRegister()  // static
{
    Listener::consoleRegister();

    LOG_AS("audio::System");

    // Drivers:
    C_CMD("listaudiodrivers",   nullptr, ListDrivers);
    C_CMD("inspectaudiodriver", "s",     InspectDriver);

    // Sound:
    C_VAR_INT     ("sound-16bit",         &sfx16Bit,              0, 0, 1);
    C_VAR_INT     ("sound-3d",            &sfx3D,                 0, 0, 1);
    C_VAR_BYTE    ("sound-overlap-stop",  &sfxOneSoundPerEmitter, 0, 0, 1);
    C_VAR_INT     ("sound-rate",          &sfxSampleRate,         0, 11025, 44100);
    C_VAR_INT     ("sound-volume",        &sfxVolume,             0, 0, 255);

    C_CMD_FLAGS("playsound",  nullptr, PlaySound,  CMDF_NO_DEDICATED);

    // Music:
    C_VAR_CHARPTR2("music-soundfont",     &musMidiFontPath,       0, 0, 0, musicMidiFontChanged);
    C_VAR_INT     ("music-source",        &musSourcePriority,     0, 0, 2);
    C_VAR_INT     ("music-volume",        &musVolume,             0, 0, 255);

    C_CMD_FLAGS("pausemusic", nullptr, PauseMusic, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("playmusic",  nullptr, PlayMusic,  CMDF_NO_DEDICATED);
    C_CMD_FLAGS("stopmusic",  "",      StopMusic,  CMDF_NO_DEDICATED);

    // Debug:
    C_VAR_INT     ("sound-info",          &showMixerInfo,         0, 0, 1);

    DENG2_DEBUG_ONLY(
        C_CMD("inspectaudiomixer", nullptr, InspectMixer);
    );
}

}  // namespace audio

using namespace audio;

// Music: ---------------------------------------------------------------------------

void S_PauseMusic(dd_bool paused)
{
    audio::System::get().pauseMusic(paused);
}

void S_StopMusic()
{
    audio::System::get().stopMusic();
}

dd_bool S_StartMusicNum(dint musicId, dd_bool looped)
{
    if(musicId >= 0 && musicId < ::defs.musics.size())
    {
        return audio::System::get().playMusic(::defs.musics[musicId], looped);
    }
    return false;
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
    return (dd_bool) audio::System::get().soundIsPlaying(audio::WorldStage, soundId, (SoundEmitter *)emitter);
}

void S_StopSound2(dint soundId, mobj_t *emitter, dint flags)
{
    audio::System::get().stopSound(audio::WorldStage, soundId, (SoundEmitter *)emitter, flags);
}

void S_StopSound(dint soundId, mobj_t *emitter)
{
    S_StopSound2(soundId, emitter, 0/*flags*/);
}

dint S_LocalSoundAtVolumeFrom(dint soundIdAndFlags, mobj_t *emitter, coord_t *origin, dfloat volume)
{
    return audio::System::get().playSound(audio::LocalStage, soundIdAndFlags, (SoundEmitter *)emitter, origin, volume);
}

dint S_LocalSoundAtVolume(dint soundIdAndFlags, mobj_t *emitter, dfloat volume)
{
    return S_LocalSoundAtVolumeFrom(soundIdAndFlags, emitter, nullptr, volume);
}

dint S_LocalSoundFrom(dint soundIdAndFlags, coord_t *origin)
{
    return S_LocalSoundAtVolumeFrom(soundIdAndFlags, nullptr, origin, 1/*max volume*/);
}

dint S_LocalSound(dint soundIdAndFlags, mobj_t *emitter)
{
    return S_LocalSoundAtVolumeFrom(soundIdAndFlags, emitter, nullptr, 1/*max volume*/);
}

dint S_StartSoundAtVolume(dint soundIdAndFlags, mobj_t *emitter, dfloat volume)
{
    return audio::System::get().playSound(audio::WorldStage, soundIdAndFlags, (SoundEmitter *)emitter, nullptr, volume);
}

dint S_StartSoundEx(dint soundIdAndFlags, mobj_t *emitter)
{
    return S_StartSoundAtVolume(soundIdAndFlags, emitter, 1/*max volume*/);
}

dint S_StartSound(dint soundIdAndFlags, mobj_t *emitter)
{
    return S_StartSoundEx(soundIdAndFlags, emitter);
}

dint S_ConsoleSound(dint soundIdAndFlags, mobj_t *emitter, dint targetConsole)
{
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
