/** @file system.cpp  System module for audio playback.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#define DENG_NO_API_MACROS_SOUND

#include "audio/system.h"

#include "api_sound.h"
#include "audio/drivers/dummydriver.h"
#include "audio/drivers/plugindriver.h"
#include "audio/drivers/sdlmixerdriver.h"
#include "audio/listener.h"
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
#include "def_main.h"  // ::defs
#include "clientapp.h"

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
static MusicSource musSourcePriority = MUSP_EXT;

static ::audio::System *theAudioSystem;

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

// --------------------------------------------------------------------------------------

String Channel::typeAsText(Type type)  // static
{
    switch(type)
    {
    case Cd:    return "CD";
    case Music: return "Music";
    case Sound: return "SFX";

    default: DENG2_ASSERT(!"audio::Channel::typeAsText: Unknown Type"); break;
    }
    return "(unknown)";
}

// --------------------------------------------------------------------------------------

String MusicSourceAsText(MusicSource source)  // static
{
    switch(source)
    {
    case MUSP_MUS: return "MUS lumps";
    case MUSP_EXT: return "External files";
    case MUSP_CD:  return "CD";

    default: DENG2_ASSERT(!"audio::MusicSourceAsText: Unknown Source"); break;
    }
    return "(invalid)";
}

// --------------------------------------------------------------------------------------

String IDriver::statusAsText() const
{
    switch(status())
    {
    case Loaded:      return "Loaded";
    case Initialized: return "Initialized";

    default: DENG2_ASSERT(!"audio::IDriver::statusAsText: Invalid status"); break;
    }
    return "(invalid)";
}

String IDriver::description() const
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
            pSummary += " - " + Channel::typeAsText(Channel::Type(rec.geti("channelType")))
                      + ": " _E(>) + rec.gets("identityKey") + _E(<);
        }
        desc += "\n" _E(.)_E(.) + pSummary;
    }

    // Finally, the high-level status of the driver.
    desc += "\n" _E(D)_E(b) "Status: " _E(.) + statusAsText();

    return desc;
}

// --------------------------------------------------------------------------------------

/**
 * @todo Simplify architecture - load the "dummy" driver, always. -ds
 */
DENG2_PIMPL(System)
, DENG2_OBSERVES(Stage, Addition)
, DENG2_OBSERVES(App, GameUnload)
{
    /// Configuration settings for this module.
    SettingsRegister settings;

    /**
     * All loaded audio drivers (if a driver can't be loaded it will be discarded).
     */
    struct Drivers : public QList<IDriver *>
    {
        ~Drivers() { DENG2_ASSERT(isEmpty()); }

        void clear()  // Shadow QList::clear()
        {
            qDeleteAll(*this);
            QList::clear();
        }

        IDriver &find(String identityKey) const
        {
            if(IDriver *driver = tryFind(identityKey)) return *driver;
            /// @throw System::MissingDriverError  Unknown driver identifier specified.
            throw System::MissingDriverError("audio::System::Instance::Drivers::find", "Unknown driver \"" + identityKey + "\"");
        }

        IDriver *tryFind(String identityKey) const
        {
            identityKey = identityKey.toLower();  // Symbolic identity keys are lowercase.

            for(IDriver *driver : (*this))
            for(QString const &idKey : driver->identityKey().split(';'))
            {
                if(idKey == identityKey)
                    return driver;
            }
            return nullptr;
        }
    } drivers;

    /**
     * All known (playback) interfaces (registered during driver install).
     */
    struct KnownInterfaces
    {
        /// The referenced interface is missing. @ingroup errors
        DENG2_ERROR(MissingError);

        /// Interfaces are grouped by their associated logical Channel::Type.
        QMap<String /*key: identityKey*/, Record> group[Channel::TypeCount];

        /**
         * Clear the database of known playback interfaces.
         */
        void clear()
        {
            for(auto &set : group) { set.clear(); }
        }

        /**
         * Register a playback interface by placing a copy of it's @a interfaceDef into
         * the database.
         *
         * @return  The indexed copy of @a interfaceDef, for caller convenience.
         */
        Record &insert(Record const &interfaceDef)
        {
            dint channelType = interfaceDef.geti("channelType");
            DENG2_ASSERT(channelType >= 0 && channelType < Channel::TypeCount);
            return group[channelType].insert(interfaceDef.gets("identityKey"), interfaceDef).value();
        }

        /**
         * Locate the one playback interface definition which is associated with the given
         * @a identityKey and @a channelType identifiers.
         */
        Record &find(DotPath const &identityKey, Channel::Type channelType) const
        {
            if(auto *found = tryFind(identityKey, channelType)) return *found;
            /// @throw MissingError  Unknown identity key & Channel::Type pair specified.
            throw MissingError("audio::System::Instance::KnownInterfaces::find", "Unknown " + Channel::typeAsText(channelType) + " interface \"" + identityKey + "\"");
        }

        /**
         * Lookup the one playback interface definition which is associated with the given
         * @a identityKey and @a channelType identifiers. @c nullptr is returned if unable
         * to locate a definition with the reference given.
         */
        Record *tryFind(DotPath const &identityKey, Channel::Type channelType) const
        {
            DENG2_ASSERT(dint(channelType) >= 0 && dint(channelType) < Channel::TypeCount);
            auto found = group[dint(channelType)].find(identityKey.toStringRef().toLower());
            if(found != group[dint(channelType)].end()) return const_cast<Record *>(&found.value());
            return nullptr;
        }
    } interfaces;

    struct ActiveInterface
    {
        bool _needInit    = true;
        bool _initialized = false;

        Record *_def;
        IDriver *_driver;
        IChannelFactory *_channelFactory;

        ActiveInterface(Record &def, IChannelFactory &channelFactory, IDriver *driver = nullptr)
            : _def(&def), _driver(driver), _channelFactory(&channelFactory)
        {}

        Record &def() const
        {
            DENG2_ASSERT(_def);
            return *_def;
        }

        inline Channel::Type channelType() const
        {
            return Channel::Type( def().geti("channelType") );
        }

        void initialize()
        {
            // Been here already?
            if(!_needInit) return;

            _needInit    = false;
            _initialized = false;
            try
            {
                if(_driver)
                {
                    _driver->initInterface(def().gets("identityKey"));
                }
                _initialized = true;
            }
            catch(Error const &er)
            {
                LOG_AUDIO_WARNING("Failed initializing \"%s\": ")
                    << def().gets("identityKey")
                    << er.asText();
            }
        }

        void deinitialize()
        {
            // Been here already?
            if(!_initialized) return;

            if(_driver)
            {
                _driver->deinitInterface(def().gets("identityKey"));
            }
            _initialized = false;
            _needInit    = true;
        }

        // Note: Drivers retain ownership of channels.
        Channel *makeChannel()
        {
            initialize();  // If we haven't already.

            DENG2_ASSERT(_channelFactory);
            return _channelFactory->makeChannel(channelType());
        }
    };

    struct ActiveInterfaces : public QList<ActiveInterface>
    {
        ~ActiveInterfaces() { DENG2_ASSERT(isEmpty()); }

        bool has(Record const &interfaceDef)
        {
            for(ActiveInterface const &active : (*this))
            {
                if(&active.def() == &interfaceDef) return true;
            }
            return false;
        }
    } activeInterfaces;  //< Initialization order.

    /**
     * Attempt to "install" the given audio @a driver (ownership is given), by first
     * validating it to ensure the metadata is well-formed and then ensuring the driver
     * is compatibile with any preexisting (logging any issues detected in the process).
     *
     * If successful @a driver is added to the collection of loaded @var drivers and the
     * playback interfaces it provides are registered into the db of known @var interfaces.
     *
     * If @a driver cannot be installed it will simply be deleted (it's of no use to us)
     * and no further action is taken.
     */
    void installDriver(IDriver *driver)
    {
        if(!driver) return;

        // Have we already installed (and taken ownership of) this driver?
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
                                         " identity key \"%s\" (must be unique) - cannot install driver \"%s\"")
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

        // Validate playback interfaces and register in the known interface db.
        for(Record const &rec : driver->listInterfaces())
        {
            DotPath const idKey(rec.gets("identityKey"));
            auto const channelType = Channel::Type( rec.geti("channelType") );

            // Interface identity keys must be well-formed.
            if(idKey.segmentCount() < 2 || idKey.firstSegment() != driver->identityKey().split(';').first())
            {
                LOGDEV_AUDIO_WARNING("Playback interface identity key \"%s\" for driver \"%s\""
                                     " is malformed (expected \"<driverIdentityKey>.<interfaceIdentityKey>\")"
                                     " - cannot register interface")
                    << idKey << driver->identityKey();
                continue;
            }

            // Interface identity keys must be unique.
            if(interfaces.tryFind(idKey, channelType))
            {
                LOGDEV_AUDIO_WARNING("A playback interface with identity key \"%s\" already"
                                     " exists (must be unique) - cannot register interface")
                    << idKey;
                continue;
            }

            // Seems legit...
            interfaces.insert(rec);  // A copy is made.
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
            if(IDriver *driver = active._driver)
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

        // Clear the known interface database.
        interfaces.clear();

        // Finally, unload all the drivers.
        drivers.clear();
    }

    void loadDrivers()
    {
        DENG2_ASSERT(activeInterfaces.isEmpty());
        DENG2_ASSERT(drivers.isEmpty());
        DENG2_ASSERT(!App::commandLine().has("-nosound"));

        // Firstly - built-in drivers.
        installDriver(new DummyDriver);
#ifndef DENG_DISABLE_SDLMIXER
        installDriver(new SdlMixerDriver);
#endif

        // Secondly - plugin drivers.
        Library_forAll([this] (LibraryFile &libFile)
        {
            if(libFile.name().beginsWith("audio_", Qt::CaseInsensitive))
            {
                if(auto *driver = PluginDriver::interpretFile(&libFile))
                {
                    installDriver(driver);  // Takes ownership.
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
    String interfacePriority(Channel::Type type)
    {
        String list;

        String arg;
        switch(type)
        {
        case Channel::Cd:    arg = "-icd";    break;
        case Channel::Music: arg = "-imusic"; break;
        case Channel::Sound: arg = "-isfx";   break;
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
     * @param channelType   Common Channel::Type identifier.
     * @param priorityList  Playback interface identity keys (delimited with ';').
     *
     * @return  Sanitized list of playback interfaces in the order given.
     */
    QStringList parseInterfacePriority(Channel::Type channelType, String priorityList)
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
                if(IDriver const *driver = drivers.tryFind(idKey.firstSegment()))
                {
                    idKey = DotPath(driver->identityKey().split(';').first())
                          / idKey.toStringRef().mid(idKey.firstSegment().length() + 1);
                }
            }

            // Do we know this playback interface?
            if(Record const *foundInterface = interfaces.tryFind(idKey, channelType))
            {
                it.setValue(foundInterface->gets("identityKey"));
            }
            else
            {
                // Not suitable.
                it.remove();

                LOG_AUDIO_WARNING("Unknown %s playback interface \"%s\"")
                    << Channel::typeAsText(channelType) << idKey;
            }
        }

        list.removeDuplicates();  // Eliminate redundancy.

        return list;
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
        if(activeInterfaces.has(interfaceDef))
            return;

        IChannelFactory *channelFactory = nullptr;

        // If this interface belongs to a driver - ensure that the driver is initialized
        // before activating the interface.
        IDriver *driver = nullptr;
        DotPath const idKey = interfaceDef.gets("identityKey");
        if(idKey.segmentCount() > 1)
        {
            driver = drivers.tryFind(idKey.firstSegment());
            if(driver)
            {
                initDriverIfNeeded(*driver);
                if(!driver->isInitialized())
                    return;

                channelFactory = &driver->channelFactory();
            }
        }

        if(!channelFactory)
        {
            LOG_AS("audio::System");
            LOG_AUDIO_ERROR("Failed to locate Channel factory for \"%s\" - cannot activate interface") << idKey;
            return;
        }

        ActiveInterface active(const_cast<Record &>(interfaceDef), *channelFactory, driver);
        activeInterfaces.append(active); // A copy is made.
    }

    /**
     * Activate all user-preferred playback interfaces with the given @a channelType, if
     * they are not already activated.
     */
    void activateInterfaces(Channel::Type channelType)
    {
        for(DotPath const idKey : parseInterfacePriority(channelType, interfacePriority(channelType)))
        {
            try
            {
                activateInterface(interfaces.find(idKey, channelType));
            }
            catch(KnownInterfaces::MissingError const &er)
            {
                // Log but otherwise ignore this error.
                LOG_AUDIO_ERROR("") << er.asText();
            }
        }
    }

    /**
     * Activate all user-preferred playback interfaces, for @em all channel types, if they
     * are not already activated.
     */
    void activateInterfaces()
    {
        for(dint i = 0; i < Channel::TypeCount; ++i)
        {
            activateInterfaces(Channel::Type( i ));
        }
    }

    bool musicAvail = false;                 ///< @c true if one or more interfaces are initialized for music playback.
    bool soundAvail = false;                 ///< @c true if one or more interfaces are initialized for sound playback.

    bool musicPaused = false;
    String musicCurrentSong;
    bool musicNeedSwitchBufferFile = false;  ///< @c true= choose a new file name for the buffered playback file when asked. */

    Stage localStage;
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

        localStage.audienceForAddition() += this;
        worldStage.audienceForAddition() += this;
    }

    ~Instance()
    {
        localStage.audienceForAddition() -= this;
        worldStage.audienceForAddition() -= this;

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
     * Destroy and then recreate the Mixer according to the current config.
     */
    void initMixer()
    {
        /// @todo Ensure existing channels have been released! -ds

        // Replace the mixer (we'll reconfigure).
        mixer.reset(new Mixer);
        mixer->makeTrack("music").setTitle("Music");
        mixer->makeTrack("fx"   ).setTitle("Effects");

        bool const noMusic = App::commandLine().has("-nomusic");
        if(noMusic) LOG_AUDIO_NOTE("Music disabled");

        bool const noSound = App::commandLine().has("-nosfx");
        if(noSound) LOG_AUDIO_NOTE("Sound effects disabled");

        /// @todo Defer channel construction until asked to play. Need to handle channel
        /// lifetime and positioning mode switches dynamically. -ds
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface &active = activeInterfaces[i];
            switch(active.channelType())
            {
            case Channel::Cd:
            case Channel::Music:
                if(!noMusic)
                {
                    (*mixer)["music"].addChannel(active.makeChannel());
                }
                break;

            case Channel::Sound:
                if(!noSound && (*mixer)["fx"].channelCount() == 0)
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

        musicAvail = (*mixer)["music"].channelCount() >= 1;
        soundAvail = (*mixer)["fx"   ].channelCount() >= 1;
    }

    /**
     * Returns the total number of sound channels currently playing a/the sound associated
     * with the given @a effectId and/or @a emitter.
     */
    dint countSoundChannelsPlaying(dint effectId, SoundEmitter *emitter = nullptr) const
    {
        dint count = 0;
        (*mixer)["fx"].forAllChannels([&effectId, &emitter, &count] (Channel &base)
        {
            auto &ch = base.as<SoundChannel>();

            if(!ch.isPlaying()) return LoopContinue;
            if(emitter  && ch.sound()->emitter()  != emitter)  return LoopContinue;
            if(effectId && ch.sound()->effectId() != effectId) return LoopContinue;

            // Once playing, repeating sounds don't stop.
            /*if(ch.playingMode() == Looping)
            {
                // Check time. The flag is updated after a slight delay (only at refresh).
                dint const ticsToDelay = ch.sound()->numSamples / dfloat( ch.buffer().freq ) * TICSPERSEC;
                if(Timer_Ticks() - ch.startTime() < ticsToDelay)
                    return LoopContinue;
            }*/

            count += 1;
            return LoopContinue;
        });
        return count;
    }

    void getSoundChannelPriorities(QList<dfloat> &prios) const
    {
        dint idx = 0;
        (*mixer)["fx"].forAllChannels([this, &prios] (Channel &base)
        {
            auto &ch = base.as<SoundChannel>();
            if(!ch.isPlaying())
            {
                prios.append(SFX_LOWEST_PRIORITY);
            }
            else
            {
                DENG2_ASSERT(ch.sound());
                Sound const &sound = *ch.sound();

                /// @todo Use Listener of the Channel. -ds
                Listener &listener = self.worldStage().listener();
                prios.append(listener.rateSoundPriority(ch.startTime(), ch.volume(),
                                                        sound.flags(), sound.origin()));
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
        (*mixer)["fx"].forAllChannels([this, &group, &emitter, &stopCount] (Channel &base)
        {
            auto &ch = base.as<SoundChannel>();

            if(!ch.isPlaying())
                return LoopContinue;

            DENG2_ASSERT(ch.sound());
            if(emitter && ch.sound()->emitter() != emitter)
                return LoopContinue;

            sfxinfo_t const *soundDef = Def_GetSoundInfo(ch.sound()->effectId());
            DENG2_ASSERT(soundDef);
            if(soundDef->group != group)
                return LoopContinue;

            // This channel must be stopped!
            ch.stop();
            stopCount += 1;
            return LoopContinue;
        });
        return stopCount;
    }

#if 0
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
    dint stopSoundChannelsWithEmitter(SoundEmitter *emitter = nullptr)
    {
        dint stopCount = 0;
        (*mixer)["fx"].forAllChannels([&emitter, &stopCount] (Channel &base)
        {
            auto &ch = base.as<SoundChannel>();

            if(!ch.isPlaying()) return LoopContinue;

            DENG2_ASSERT(ch.sound());
            Sound &sound = *ch.sound();
            if(!sound.emitter() || (!emitter || sound.emitter() == emitter))
            {
                // This channel must be stopped!.
                ch.stop();
                stopCount += 1;
            }
            return LoopContinue;
        });
        return stopCount;
    }
#endif

    /**
     * Stop all sound channels currently playing a/the sound with a lower priority rating.
     *
     * @param effectId     If > 0, the currently playing sound must be associated with
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
    dint stopSoundChannelsWithLowerPriority(dint effectId, SoundEmitter *emitter, dint defPriority)
    {
        dint stopCount = 0;
        (*mixer)["fx"].forAllChannels([&effectId, &emitter, &defPriority, &stopCount] (Channel &base)
        {
            auto &ch = base.as<SoundChannel>();

            if(!ch.isPlaying()) return LoopContinue;

            if(   (effectId && ch.sound()->effectId() != effectId)
               || (emitter  && ch.sound()->emitter()  != emitter))
            {
                return LoopContinue;
            }

            // Can it be stopped?
            if(ch.mode() == OnceDontDelete)
            {
                ch.suspend();
                return LoopContinue;
            }

            // Check the priority.
            if(defPriority >= 0)
            {
                dint oldPrio = ::defs.sounds[ch.sound()->effectId()].geti("priority");
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
     * @param effectId     If > 0, the channel must currently be loaded with a/the sound
     * associated with this identifier.
     */
    SoundChannel *vacantSoundChannel(Positioning positioning, dint bytesPer, dint rate,
        dint effectId) const
    {
        SoundChannel *found = nullptr;  // None suitable.
        (*mixer)["fx"].forAllChannels([&positioning, &bytesPer, &rate, &effectId, &found]
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
            if(effectId > 0)
            {
                if(!ch.sound() || ch.sound()->effectId() != effectId)
                    return LoopContinue;
            }
            else if(effectId == 0)
            {
                // We're trying to find a channel with no sound loaded.
                if(ch.sound())
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
                        ch.play(looped ? Looping : OnceDontDelete);
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
                        ch.play(looped ? Looping : OnceDontDelete);
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
                            ch->play(looped ? Looping : OnceDontDelete);
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

                    ch.play(looped ? Looping : OnceDontDelete);
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
                        ch.play(looped ? Looping : OnceDontDelete);
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
                    ch->play(looped ? Looping : OnceDontDelete);
                    return LoopAbort;  // Success!
                }
                catch(Error const &)
                {}
            }
            return LoopContinue;
        });
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
            // Disable environmental effects in the world soundstage - we're going stereo.
            worldStage.listener().useEnvironment(false);
        }
        old3DMode = sfx3D;
    }

    /**
     * Called whenever a new Sound is added to a soundstage.
     * Our job is to find a suitable Channel on which to play it.
     */
    void stageSoundAdded(Stage &stage, Sound &sound)
    {
        // Playback cannot begin while in busy mode...
        if(DoomsdayApp::app().busyMode().isActive())
            return;

        // Skip playback if it won't be heard.
        if(sfxVolume <= 0 || sound.volume() <= 0)
            return;

        // Skip playback if this is too far from the Listener?
        if(&stage == &worldStage
           && !sound.flags().testFlag(SoundFlag::NoOrigin)
           && !sound.flags().testFlag(SoundFlag::NoVolumeAttenuation))
        {
            if(!stage.listener().inAudibleRangeOf(sound.emitter() ? sound.emitter()->origin : sound.origin()))
                return;
        }

        // Don't attempt to play zero-length sound resources.
        sfxsample_t const &sample = *sampleCache.cache(sound.effectId());
        if(sample.size == 0) return;

        // Sound definitions can be used to override playback behavior/modifiers.
        dfloat frequency = 1;
        sfxinfo_t const &soundDef = *Def_GetSoundInfo(sound.effectId(), &frequency);

        // Random frequency alteration? 
        if(!sfxNoRndPitch)
        {
            // Multipliers chosen to match original sound code.
            if(soundDef.flags & SF_RANDOM_SHIFT)
            {
                frequency += (RNG_RandFloat() - RNG_RandFloat()) * (7.0f / 255);
            }
            if(soundDef.flags & SF_RANDOM_SHIFT2)
            {
                frequency += (RNG_RandFloat() - RNG_RandFloat()) * (15.0f / 255);
            }
        }

        // If the sound has an exclusion group, either all or the same emitter's iterations
        // of this sound will stop.
        if(soundDef.group)
        {
            stopSoundChannelsWithSoundGroup(soundDef.group, (soundDef.flags & SF_GLOBAL_EXCLUDE) ? nullptr : sound.emitter());
        }

        if(!soundAvail)
            return;

        // Determine playback behavior.
        PlayingMode const mode =   sound.flags().testFlag(SoundFlag::Repeat) ? Looping
                                 : (soundDef.flags & SF_DONT_STOP)           ? OnceDontDelete
                                                                             : Once;
        if(&stage == &worldStage)
        {
            // Stop all other sounds with the same emitter?
            if(sound.emitter() && stage.exclusion() == Stage::OnePerEmitter)
            {
                if(stopSoundChannelsWithLowerPriority(0, sound.emitter(), soundDef.priority) < 0)
                {
                    // Something with a higher priority is playing, can't start now.
                    LOG_AUDIO_MSG("Not playing sound (id:%i emitter:%i) prio:%i because overridden")
                        << sample.effectId << sound.emitter()->thinker.id
                        << soundDef.priority;
                    return;
                }
            }
        }

        // Determine positioning model.
        Positioning const positioning = (sfx3D && !sound.flags().testFlag(NoOrigin)) ? AbsolutePositioning : StereoPositioning;

        dfloat const priority = worldStage.listener()
            .rateSoundPriority(Timer_Ticks(), sound.volume(), sound.flags(), sound.origin());

        dfloat lowPrio = 0;
        QList<dfloat> channelPrios;
        channelPrios.reserve((*mixer)["fx"].channelCount());

        // Ensure there aren't already too many channels playing this sample.
        if(soundDef.channels > 0)
        {
            // The decision to stop channels is based on priorities.
            getSoundChannelPriorities(channelPrios);

            dint count = countSoundChannelsPlaying(sample.effectId);
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
                        if(   ch.sound()->effectId() == sample.effectId
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
                        << sample.effectId;
                    return;
                }

                // Stop this one.
                count--;
                selCh->stop();
            }
        }

        // Hit count tells how many times the cached sound has been used.
        sampleCache.hit(sample.effectId);

        /*
         * Pick a channel for the sound. We will do our best to play the sound, cancelling
         * existing ones if need be. The ideal choice is a free channel that is already
         * loaded with the sample, in the correct format and mode.
         */
        self.allowChannelRefresh(false);

        // First look through the stopped channels. At this stage we're very picky: only
        // the perfect choice will be good enough.
        SoundChannel *selCh = vacantSoundChannel(positioning, sample.bytesPer,
                                                 sample.rate, sample.effectId);

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
                getSoundChannelPriorities(channelPrios);
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
            LOG_AUDIO_XVERBOSE("Failed to find a suitable Channel for sample id:%i") << sample.effectId;
            return;
        }
        SoundChannel &channel = *selCh;

        // The channel may need to be reformatted.
        {
            channel.format(positioning, sample.bytesPer, sample.rate);
            DENG2_ASSERT(channel.isValid());
            channel.load(sample);
        }

        // Initialize playback modifiers and start playing.
        channel.setFrequency(frequency)
               .setVolume(sound.volume());
        channel.play(mode);

        // Streaming of playback data and channel updates may now continue.
        self.allowChannelRefresh();
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

::audio::System &System::get()
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
        os << _E(Ta) _E(l) "  " << Channel::typeAsText(active.channelType()) << ": "
           << _E(.) _E(Tb) << active.def().gets("identityKey") << "\n";
    }

    return str.rightStrip();

#undef TABBED
}

dint System::upsampleFactor(dint rate) const
{
    //LOG_AS("audio::System");
    bool mustUpsample = mixer()["fx"].forAllChannels([] (Channel &ch)
    {
        return !ch.anyRateAccepted();
    });
    if(mustUpsample)
    {
        return de::max(1, ::sfxRate / rate);
    }
    return 1;
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

Stage /*const*/ &System::stage(StageId stageId) const
{
    switch(stageId)
    {
    case LocalStage: return d->localStage;
    case WorldStage: return d->worldStage;

    default: DENG2_ASSERT(!"audio::System::stage: Unknown StageId"); break;
    }
    throw Error("audio::System::stage", "Unknown sound Stage");
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

void System::stopSound(StageId stageId, dint effectId, SoundEmitter *emitter, dint flags)
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
                stopSound(stageId, effectId, emitter);
            }

            // Stop sounds emitted by Sector-linked (plane/wall) emitters?
            if(flags & SSF_SECTOR_LINKED_SURFACES)
            {
                // Process the rest of the emitter chain.
                while((emitter = (SoundEmitter *)emitter->thinker.next))
                {
                    // Stop sounds from this emitter.
                    stopSound(stageId, effectId, emitter);
                }
            }
            return;
        }
    }

    // No special stop behavior.
    d->stopSoundChannelsWithLowerPriority(effectId, emitter, -1);

    // Update logical sound bookkeeping.
    if(effectId <= 0 && !emitter)
    {
        stage(stageId).removeAllSounds();
    }
    else if(effectId) // > 0
    {
        stage(stageId).removeSoundsById(effectId);
    }
    else
    {
        stage(stageId).removeSoundsWithEmitter(*emitter);
    }
}

dint System::driverCount() const
{
    return d->drivers.count();
}

IDriver const *System::tryFindDriver(String driverIdKey) const
{
    return d->drivers.tryFind(driverIdKey);
}

IDriver const &System::findDriver(String driverIdKey) const
{
    return d->drivers.find(driverIdKey);
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

    stopMusic();

    if(d->soundAvail)
    {
        // Stop all currently playing sound channels.
        mixer()["fx"].forAllChannels([] (Channel &ch)
        {
            ch.stop();
            return LoopContinue;
        });
    }

    // Force an Environment update for all channels.
    worldStage().listener().setTrackedMapObject(nullptr);
    worldStage().listener().setTrackedMapObject(getListenerMob());

    // Clear the sample cache.
    sampleCache().clear();
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
        sampleCache().maybeRunPurge();
    }

    worldStage().setExclusion(sfxOneSoundPerEmitter ? Stage::OnePerEmitter : Stage::DontExclude);

    for(dint i = 0; i < StageCount; ++i)
    {
        stage(StageId(i)).maybeRunSoundPurge();
    }
}

void System::endFrame()
{
    LOG_AS("audio::System");

    /// @todo Should observe. -ds
    worldStage().listener().setTrackedMapObject(getListenerMob());

    // Instruct currently playing Channels to write any effective Environment and/or sound
    // positioning mode changes if necessary (from the configured Listener of the Stage they
    // are playing on).
    if(d->soundAvail && !BusyMode_Active())
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
    worldStage().listener().setTrackedMapObject(getListenerMob());
}

void System::initPlayback()
{
    LOG_AS("audio::System");

    CommandLine &cmdLine = App::commandLine();
    if(cmdLine.has("-noaudio") || cmdLine.has("-nosound"))
    {
        LOG_AUDIO_NOTE("All audio playback is disabled");
        return;
    }

    LOG_AUDIO_VERBOSE("Initializing for playback...");

    // Disable random pitch changes?
    sfxNoRndPitch = cmdLine.has("-norndpitch");

    d->musicAvail = false;
    d->soundAvail = false;
    d->musicCurrentSong = "";
    d->musicPaused      = false;

    // Load all the available audio drivers.
    d->loadDrivers();

    // Activate playback interfaces specified in Config.
    d->activateInterfaces();

    // (Re)Init the waveform data cache.
    sampleCache().clear();

    // Disable environmental audio effects in the world soundstage by default.
    worldStage().listener().useEnvironment(false);

    // Prepare the mixer.
    d->initMixer();

    // Tell audio drivers about our soundfont config.
    /// @todo Should be handled automatically (e.g., when a Channel is added to the Mixer). -ds
    updateMusicMidiFont();

    // Print a summary of the active configuration to the log.
    LOG_AUDIO_MSG("%s") << description();
}

void System::deinitPlayback()
{
    LOG_AS("audio::System");

    // Clear the waveform data cache.
    sampleCache().clear();

    // Deinitialize active playback interfaces.
    for(dint i = d->activeInterfaces.count(); i--> 0; )
    {
        d->activeInterfaces[i].deinitialize();
    }
    d->musicAvail = false;
    d->soundAvail = false;

    // Finally, unload the drivers.
    d->unloadDrivers();
}

void System::allowChannelRefresh(bool allow)
{
    for(IDriver *driver : d->drivers)
    {
        driver->allowRefresh(allow);
    }
}

/**
 * Console command for logging a summary of the loaded audio drivers.
 */
D_CMD(ListDrivers)
{
    DENG2_UNUSED3(src, argc, argv);
    LOG_AS("listaudiodrivers (Cmd)");

    if(ClientApp::audioSystem().driverCount() <= 0)
    {
        LOG_SCR_MSG("No audio drivers are currently loaded");
        return true;
    }

    String list;
    dint numDrivers = 0;
    ClientApp::audioSystem().forAllDrivers([&list, &numDrivers] (IDriver const &driver)
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
    if(IDriver const *driver = ClientApp::audioSystem().tryFindDriver(driverId))
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

    if(argc < 2)
    {
        LOG_SCR_NOTE("Usage: %s (id) (volume) at (x) (y) (z)") << argv[0];
        LOG_SCR_MSG("(volume) must be in 0..1, but may be omitted");
        LOG_SCR_MSG("'at (x) (y) (z)' may also be omitted");
        LOG_SCR_MSG("The sound is always played locally");
        return true;
    }

    SoundParams params;

    // The first argument is the sound ID.
    params.effectId = ::defs.getSoundNum(String(argv[1]));
    if(params.effectId <= 0)
    {
        LOG_SCR_WARNING("Unknown sound \"%s\"") << String(argv[1]);
        return false;
    }

    // The next argument may be a volume.
    dint p = 0;
    if(argc >= 3 && String(argv[2]).compareWithoutCase("at"))
    {
        params.volume = de::clamp<dfloat>(0, String(argv[2]).toFloat(), 1);
        p = 3;
    }
    else
    {
        p = 2;
    }

    // The next argument may be soundstage coordinates.
    if(argc >= p + 4 && !String(argv[p]).compareWithoutCase("at"))
    {
        params.flags &= ~SoundFlag::NoOrigin;
        params.origin = Vector3d(String(argv[p + 1]).toDouble(),
                                 String(argv[p + 2]).toDouble(),
                                 String(argv[p + 3]).toDouble());
    }

    ClientApp::audioSystem().localStage().playSound(params);
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
    if(!ClientApp::audioSystem().musicPlaybackAvailable())
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
            return ClientApp::audioSystem().playMusic(*definition, looped);
        }
        LOG_SCR_WARNING("Music '%s' not defined") << argv[1];
        return false;
    }

    if(argc == 3)
    {
        // Play a file referenced directly.
        if(!qstricmp(argv[1], "lump"))
        {
            return ClientApp::audioSystem().playMusicLump(App_FileSystem().lumpNumForName(argv[2]), looped);
        }
        else if(!qstricmp(argv[1], "file"))
        {
            return ClientApp::audioSystem().playMusicFile(argv[2], looped);
        }
        else if(!qstricmp(argv[1], "cd"))
        {
            return ClientApp::audioSystem().playMusicCDTrack(String(argv[2]).toInt(), looped);
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
    ClientApp::audioSystem().stopMusic();
    return true;
}

D_CMD(PauseMusic)
{
    DENG2_UNUSED3(src, argc, argv);
    ClientApp::audioSystem().pauseMusic(!ClientApp::audioSystem().musicIsPaused());
    return true;
}

#ifdef DENG2_DEBUG
D_CMD(InspectMixer)
{
    DENG2_UNUSED3(src, argc, argv);
    Mixer &mixer = ClientApp::audioSystem().mixer();
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
    ClientApp::audioSystem().updateMusicMidiFont();
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

using namespace ::audio;

//- Music: ------------------------------------------------------------------------------

void S_PauseMusic(dd_bool paused)
{
    ClientApp::audioSystem().pauseMusic(paused);
}

void S_StopMusic()
{
    ClientApp::audioSystem().stopMusic();
}

dd_bool S_StartMusicNum(dint songId, dd_bool looped)
{
    if(songId >= 0 && songId < ::defs.musics.size())
    {
        return ClientApp::audioSystem().playMusic(::defs.musics[songId], looped);
    }
    return false;
}

dd_bool S_StartMusic(char const *musicId, dd_bool looped)
{
    dint songId = ::defs.getMusicNum(musicId);
    if(songId < 0)
    {
        if(songId && !String(musicId).isEmpty())
        {
            LOG_AS("S_StartMusic");
            LOG_AUDIO_WARNING("Music \"%s\" not defined, cannot start playback") << musicId;
        }
        return false;
    }
    return S_StartMusicNum(songId, looped);
}

//- Sound: ------------------------------------------------------------------------------

dd_bool S_SoundIsPlaying(dint effectId, mobj_t *emitter)
{
    return (dd_bool) ClientApp::audioSystem().worldStage().soundIsPlaying(effectId, (SoundEmitter *)emitter);
}

void S_StopSound2(dint effectId, mobj_t *emitter, dint flags)
{
    ClientApp::audioSystem().stopSound(::audio::WorldStage, effectId, (SoundEmitter *)emitter, flags);
}

void S_StopSound(dint effectId, mobj_t *emitter)
{
    S_StopSound2(effectId, emitter, 0/*flags*/);
}

void S_LocalSoundAtVolumeFrom(dint effectIdAndFlags, mobj_t *emitter, coord_t *origin, dfloat volume)
{
    SoundParams params;
    params.effectId = (effectIdAndFlags & ~DDSF_FLAG_MASK);
    params.origin   = origin ? Vector3d(origin) : Vector3d();
    params.volume   = volume;

    if(origin)                                 params.flags &= ~SoundFlag::NoOrigin;
    if(effectIdAndFlags & DDSF_REPEAT)         params.flags |=  SoundFlag::Repeat;
    if(effectIdAndFlags & DDSF_NO_ATTENUATION) params.flags |=  SoundFlag::NoVolumeAttenuation;

    ClientApp::audioSystem().localStage().playSound(params, (SoundEmitter *)emitter);
}

void S_LocalSoundAtVolume(dint effectIdAndFlags, mobj_t *emitter, dfloat volume)
{
    S_LocalSoundAtVolumeFrom(effectIdAndFlags, emitter, nullptr, volume);
}

void S_LocalSoundFrom(dint effectIdAndFlags, coord_t *origin)
{
    S_LocalSoundAtVolumeFrom(effectIdAndFlags, nullptr, origin, 1/*max volume*/);
}

void S_LocalSound(dint effectIdAndFlags, mobj_t *emitter)
{
    S_LocalSoundAtVolumeFrom(effectIdAndFlags, emitter, nullptr, 1/*max volume*/);
}

void S_StartSoundAtVolume(dint effectIdAndFlags, mobj_t *emitter, dfloat volume)
{
    SoundParams params;
    params.effectId = (effectIdAndFlags & ~DDSF_FLAG_MASK);
    params.volume   = volume;

    //if(origin)                                params.flags &= ~SoundFlag::NoOrigin;
    if(effectIdAndFlags & DDSF_REPEAT)         params.flags |=  SoundFlag::Repeat;
    if(effectIdAndFlags & DDSF_NO_ATTENUATION) params.flags |=  SoundFlag::NoVolumeAttenuation;

    ClientApp::audioSystem().worldStage().playSound(params, (SoundEmitter *)emitter);
}

void S_StartSoundEx(dint effectIdAndFlags, mobj_t *emitter)
{
    S_StartSoundAtVolume(effectIdAndFlags, emitter, 1/*max volume*/);
}

void S_StartSound(dint effectIdAndFlags, mobj_t *emitter)
{
    S_StartSoundEx(effectIdAndFlags, emitter);
}

void S_ConsoleSound(dint effectIdAndFlags, mobj_t *emitter, dint targetConsole)
{
    // If it's for us, we can hear it.
    if(targetConsole == consolePlayer)
    {
        S_LocalSound(effectIdAndFlags, emitter);
    }
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
