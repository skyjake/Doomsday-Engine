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

#include "api_sound.h"
#include "audio/drivers/dummydriver.h"
#include "audio/drivers/plugindriver.h"
#include "audio/drivers/sdlmixerdriver.h"
#include "audio/channel.h"
#include "audio/mus.h"
#include "audio/samplecache.h"

#include "api_map.h"
#include "world/p_players.h"
#include "world/thinkers.h"
#include "Sector"
#include "SectorCluster"

#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <doomsday/defs/music.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <de/App>
#include <de/LibraryFile>
#include <de/timer.h>
#include <QMap>
#include <QMultiHash>
#include <QMutableHashIterator>
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

/// @todo should be a cvars:
static dint sfxDistMin = 256;  ///< No distance attenuation this close.
static dint sfxDistMax = 2025;
static bool sfxNoRndPitch;

// Console variables:
static dint sfxVolume = 255 * 2/3;
static dint sfx16Bit;
static dint sfxSampleRate = 11025;
static dint sfx3D;
static byte sfxOneSoundPerEmitter;  //< @c false= Traditional Doomsday behavior: allow sounds to overlap.
static dfloat sfxReverbStrength = 0.5f;

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
    return DD_Player(::displayPlayer)->publicData().mo;
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
                  .arg(identityKey());

    // Summarize playback interfaces.
    QList<Record> const interfaces = listInterfaces();
    if(!interfaces.isEmpty())
    {
        desc += "\n" _E(D)_E(b) "Playback interfaces:";

        String pSummary;
        for(Record const &rec : interfaces)
        {
            if(!pSummary.isEmpty()) pSummary += "\n" _E(0);
            pSummary += " - " + System::playbackInterfaceTypeAsText(PlaybackInterfaceType(rec.geti("type")))
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
    PlaybackInterfaceMap interfaces[PlaybackInterfaceTypeCount];

    struct ActiveInterface
    {
        Record *_def     = nullptr;
        IPlayer *_player = nullptr;
        IDriver *_driver = nullptr;

        Record &def() const {
            DENG2_ASSERT(_def);
            return *_def;
        }

        IPlayer &player() const {
            DENG2_ASSERT(_player);
            return *_player;
        }

        template <typename Type>
        Type &playerAs() const {
            if(!dynamic_cast<Type *>(&player())) {
                throw Error("ActiveInterface::player<Type>", "Player is not compatible with Type");
            }
            return *dynamic_cast<Type *>(&player());
        }

        bool hasDriver() const {
            return _driver != nullptr;
        }

        IDriver &driver() const {
            DENG2_ASSERT(_driver);
            return *_driver;
        }
    };
    QList<ActiveInterface> activeInterfaces;  //< Initialization order.

    Record &findInterface(PlaybackInterfaceType type, DotPath const &identityKey)
    {
        if(auto *found = tryFindInterface(type, identityKey)) return *found;
        /// @throw MissingPlaybackInterfaceError  Unknown type & identity key pair specified.
        throw MissingPlaybackInterfaceError("audio::System::Instance::findInterface", "Unknown interface identity key \"" + identityKey + "\"");
    }

    Record *tryFindInterface(PlaybackInterfaceType type, DotPath const &identityKey)
    {
        DENG2_ASSERT(dint(type) >= 0 && dint(type) < PlaybackInterfaceTypeCount);
        auto found = interfaces[dint(type)].find(identityKey.toStringRef().toLower());
        if(found != interfaces[dint(type)].end()) return &found.value();
        return nullptr;
    }

    Record &addInterface(Record const &rec)
    {
        dint type = rec.geti("type");
        DENG2_ASSERT(type >= 0 && type < PlaybackInterfaceTypeCount);
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
            auto const type = PlaybackInterfaceType( rec.geti("type") );

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
            if(!active.hasDriver()) continue;

            IDriver *driver = &active.driver();
            if(!reverseInitOrder.contains(driver))
                reverseInitOrder.prepend(driver);
        }
        for(IDriver *driver : reverseInitOrder)
        {
            LOG_AUDIO_VERBOSE("Deinitializing audio driver '%s'...") << driver->identityKey();
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

        // Firstly - built-in audio drivers.
        addDriver(new DummyDriver);
#ifndef DENG_DISABLE_SDLMIXER
        addDriver(new SdlMixerDriver);
#endif

        // Secondly - plugin audio drivers.
        Library_forAll([this] (LibraryFile &libFile)
        {
            if(PluginDriver::recognize(libFile))
            {
                LOG_AUDIO_VERBOSE("Loading plugin audio driver '%s'...") << libFile.name();
                if(auto *driver = PluginDriver::newFromLibrary(libFile))
                {
                    addDriver(driver);
                }
                else
                {
                    LOG_AUDIO_WARNING("Failed loading plugin audio driver '%s'") << libFile.name();
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
     */
    String interfacePriority(PlaybackInterfaceType type)
    {
        // Presently the audio driver configuration is inferred and/or specified using
        // command line options.
        /// @todo Store this information persistently (in Config). -ds

        String list;

        String arg;
        switch(type)
        {
        case AUDIO_ICD:    arg = "-icd";    break;
        case AUDIO_IMUSIC: arg = "-imusic"; break;
        case AUDIO_ISFX:   arg = "-isfx";   break;
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
    QStringList parseInterfacePriority(PlaybackInterfaceType type, String priorityList)
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
                    << System::playbackInterfaceTypeAsText(type)
                    << idKey;
            }
        }

        list.removeDuplicates();  // Eliminate redundancy.

        return list;
    }

    IPlayer &getPlayer(PlaybackInterfaceType type, DotPath const &identityKey)
    {
        if(identityKey.segmentCount() > 1)
        {
            IDriver const &driver = findDriver(identityKey.segment(0));
            IPlayer &player = driver.findPlayer(identityKey.segment(1));
            // Ensure this player is of the expected type.
            if(   (type == AUDIO_ICD    && player.is<ICdPlayer>())
               || (type == AUDIO_IMUSIC && player.is<IMusicPlayer>())
               || (type == AUDIO_ISFX   && player.is<ISoundPlayer>()))
            {
                return player;
            }
        }
        // Internal bookkeeping error: No such player found!
        throw Error("audio::System::Instance::getPlayer", "Failed to locate " + self.playbackInterfaceTypeAsText(type) + " player for \"" + identityKey + "\"");
    }

    inline IPlayer &getPlayerFor(Record const &ifs)
    {
        return getPlayer(PlaybackInterfaceType( ifs.geti("type") ), ifs.gets("identityKey"));
    }

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

            ActiveInterface active;
            active._def    = const_cast<Record *>(&interfaceDef);
            active._player = &player;
            active._driver = driver;
            activeInterfaces.append(active);  // A copy is made.
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
    void activateInterfaces(PlaybackInterfaceType type)
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
        for(dint i = 0; i < PlaybackInterfaceTypeCount; ++i)
        {
            activateInterfaces(PlaybackInterfaceType( i ));
        }
    }

    /**
     * Logical sounds are used to track currently playing sounds on a purely logical level
     * in relevant sound stage (irrespective of whether playback interfaces are available,
     * or if the sounds will actually be audible to anyone).
     *
     * Presently used for sounds playing in the WorldStage only.
     */
    struct LogicalSounds
    {
        static duint const PURGE_INTERVAL = 2000;  ///< 2 seconds

        struct LogicSound
        {
            bool repeat;
            duint endTime;
            mobj_t *emitter;

            LogicSound(duint endTime, bool repeat = false, mobj_t *emitter = nullptr)
                : repeat(repeat)
                , endTime(endTime)
                , emitter(emitter)
            {}

            /**
             * Returns @c true if the sound is currently playing relative to @a nowTime.
             */
            bool isPlaying(duint nowTime) const { return (repeat || endTime > nowTime); }
        };

        ~LogicalSounds()
        {
            clear();
        }

        void clear()
        {
            qDeleteAll(sounds);
            sounds.clear();
        }

        /**
         * Determines whether a logical sound is currently playing, irrespective of whether
         * it is audible or not.
         */
        bool isPlaying(dint soundId, mobj_t *emitter) const
        {
            duint const nowTime = Timer_RealMilliseconds();
            if(soundId)
            {
                auto it = sounds.constFind(soundId);
                while(it != sounds.constEnd() && it.key() == soundId)
                {
                    LogicSound const &sound = *it.value();
                    if(sound.emitter == emitter && sound.isPlaying(nowTime))
                        return true;

                    ++it;
                }
            }
            else if(emitter)
            {
                // Check if the emitter is playing any sound.
                auto it = sounds.constBegin();
                while(it != sounds.constEnd())
                {
                    LogicSound const &sound = *it.value();
                    if(sound.emitter == emitter && sound.isPlaying(nowTime))
                        return true;

                    ++it;
                }
            }
            return false;
        }

        /**
         * The sound is removed from the list of playing sounds. To be called whenever
         * a/the associated sound is stopped, regardless of whether it was actually playing
         * on the local system.
         *
         * @note Use @a soundId == 0 and @a emitter == nullptr to stop @em everything.
         *
         * @return  Number of sounds stopped.
         */
        dint stop(dint soundId, mobj_t *emitter)
        {
            dint numStopped = 0;
            MutableLogicSoundHashIterator it(sounds);
            while(it.hasNext())
            {
                it.next();

                LogicSound const &sound = *it.value();
                if(soundId)
                {
                    if(it.key() != soundId) continue;
                }
                else if(emitter)
                {
                    if(sound.emitter != emitter) continue;
                }

                delete &sound;
                it.remove();
                numStopped += 1;
            }
            return numStopped;
        }

        void start(dint soundIdAndFlags, mobj_t *emitter)
        {
            dint const soundId = (soundIdAndFlags & ~DDSF_FLAG_MASK);

            // Cache the sound sample associated with @a soundId (if necessary)
            // so that we can determine it's length.
            if(sfxsample_t const *sample = System::get().sampleCache().cache(soundId))
            {
                bool const repeat = (soundIdAndFlags & DDSF_REPEAT) || Def_SoundIsRepeating(soundId);

                duint length = sample->milliseconds();
                if(repeat && length > 1) length = 1;

                // Ignore zero length sounds.
                /// @todo Shouldn't we still stop others though? -ds
                if(!length) return;

                // Only one sound per emitter?
                if(emitter && oneSoundPerEmitter)
                {
                    // Stop all other sounds.
                    stop(0, emitter);
                }

                sounds.insert(soundId, new LogicSound(Timer_RealMilliseconds() + length,
                                                      repeat, emitter));
            }
        }

        /**
         * Remove stopped logical sounds if a purge is due.
         */
        void maybeRunPurge()
        {
            // Too soon?
            duint const nowTime = Timer_RealMilliseconds();
            if(nowTime - lastPurge < PURGE_INTERVAL) return;

            // Peform the purge now.
            LOGDEV_AUDIO_XVERBOSE("Purging logic sound hash...");
            lastPurge = nowTime;

            // Check all sounds in the hash.
            MutableLogicSoundHashIterator it(sounds);
            while(it.hasNext())
            {
                it.next();
                LogicSound &sound = *it.value();
                if(!sound.repeat && sound.endTime < nowTime)
                {
                    // This has stopped.
                    delete &sound;
                    it.remove();
                }
            }
        }

        void setOnePerSoundEmitter(bool yes = true)
        {
            oneSoundPerEmitter = yes;
        }

    private:
        typedef QMultiHash<dint /*key: soundId*/, LogicSound *> LogicSoundHash;
        typedef QMutableHashIterator<dint /*key: soundId*/, LogicSound *> MutableLogicSoundHashIterator;
        LogicSoundHash sounds;

        duint lastPurge         = 0;
        bool oneSoundPerEmitter = false;  ///< Set when a new audio frame begins.

    } logicalSounds;

    bool musicAvail = false;                 ///< @c true if one or more interfaces are initialized for music playback.
    bool soundAvail = false;                 ///< @c true if one or more interfaces are initialized for sound playback.

    bool musicPaused = false;
    String musicCurrentSong;
    bool musicNeedSwitchBufferFile = false;  ///< @c true= choose a new file name for the buffered playback file when asked. */

    mobj_t *worldStageListener               = nullptr;
    SectorCluster *worldStageListenerCluster = nullptr;

    std::unique_ptr<Channels> channels;
    SampleCache sampleCache;

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
            dint didPlay = false;
            for(dint i = activeInterfaces.count(); i--> 0; )
            {
                ActiveInterface const &active = activeInterfaces[i];
                if(active.def().geti("type") != AUDIO_IMUSIC)
                    continue;

                auto &musicPlayer = active.playerAs<IMusicPlayer>();

                // Does this interface offer buffered playback?
                if(musicPlayer.canPlayBuffer())
                {
                    // Buffer the data using the driver's own facility.
                    dsize const len = hndl->length();
                    hndl->read((duint8 *) musicPlayer.songBuffer(len), len);

                    if(didPlay = musicPlayer.play(looped))
                        break;
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

                    if(didPlay = musicPlayer.playFile(bufPath.toUtf8().constData(), looped))
                        break;
                }
            }

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

            for(dint i = activeInterfaces.count(); i--> 0; )
            {
                ActiveInterface const &active = activeInterfaces[i];
                if(active.def().geti("type") != AUDIO_IMUSIC)
                    continue;

                auto &musicPlayer = active.playerAs<IMusicPlayer>();
                if(musicPlayer.canPlayFile())
                {
                    if(dint result = musicPlayer.playFile(srcFile.toUtf8().constData(), looped))
                        return result;
                }
            }

            return 0;
        }

        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface const &active = activeInterfaces[i];
            if(active.def().geti("type") != AUDIO_IMUSIC)
                continue;

            auto &musicPlayer = active.playerAs<IMusicPlayer>();

            // Does this interface offer buffered playback?
            if(musicPlayer.canPlayBuffer())
            {
                // Buffer the data using the driver's own facility.
                std::unique_ptr<FileHandle> hndl(&App_FileSystem().openLump(lump));
                dsize const length  = hndl->length();
                hndl->read((duint8 *) musicPlayer.songBuffer(length), length);
                App_FileSystem().releaseFile(hndl->file());

                if(dint result = musicPlayer.play(looped))
                    return result;
            }
            // Does this interface offer playback from a native file?
            else if(musicPlayer.canPlayFile())
            {
                // Write the data to disk and play from there.
                String const fileName = composeMusicBufferFilename();
                if(F_DumpFile(lump, fileName.toUtf8().constData()))
                {
                    if(dint result = musicPlayer.playFile(fileName.toUtf8().constData(), looped))
                        return result;
                }
            }
        }

        return false;
    }

    dint playMusicCDTrack(dint track, bool looped)
    {
        // Assume track 0 is not valid.
        if(track != 0)
        {
            for(dint i = activeInterfaces.count(); i--> 0; )
            {
                ActiveInterface const &active = activeInterfaces[i];
                if(active.def().geti("type") == AUDIO_ICD)
                {
                    if(dint result = active.playerAs<ICdPlayer>().play(track, looped))
                        return result;
                }
            }
        }

        return false;
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
        if(::isDedicated || cmdLine.has("-nomusic"))
        {
            LOG_AUDIO_NOTE("Music disabled");
            return;
        }

        // Initialize interfaces for music playback.
        dint initialized = 0;
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface const &active = activeInterfaces[i];
            if(active.def().geti("type") != AUDIO_IMUSIC && active.def().geti("type") != AUDIO_ICD)
                continue;

            if(active.player().initialize())
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
            if(active.def().geti("type") == AUDIO_ICD || active.def().geti("type") == AUDIO_IMUSIC)
            {
                active.player().deinitialize();
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
            ActiveInterface const &active = activeInterfaces[i];
            if(active.def().geti("type") != AUDIO_ISFX)
                continue;

            if(active.player().initialize())
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
        if(soundAvail)
        {
            // This is based on the scientific calculations that if the DOOM marine
            // is 56 units tall, 60 is about two meters.
            //// @todo Derive from the viewheight.
            getSoundPlayer().listener(SFXLP_UNITS_PER_METER, 30);
            getSoundPlayer().listener(SFXLP_DOPPLER, 1.5f);

            disableWorldStageReverb();
        }

        // Prepare the channel map.
        initChannels();
    }

    /**
     * Perform deinitialization for sound playback.
     */
    void deinitSound()
    {
        // Not initialized?
        if(!soundAvail) return;

        soundAvail = false;

        // Clear the sample cache.
        sampleCache.clear();

        // Clear the channel map (and stop the refresh thread if running).
        channels.reset();

        // Shutdown active interfaces.
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface const &active = activeInterfaces[i];
            if(active.def().geti("type") == AUDIO_ISFX)
            {
                active.playerAs<ISoundPlayer>().deinitialize();
            }
        }
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
        if(!soundAvail) return false;

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
        dfloat const myPrio = Sound::ratePriority(self.worldStageListener(), emitter,
                                                  origin, volume, Timer_Ticks());

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

    void disableWorldStageReverb()
    {
        if(!soundAvail) return;

        worldStageListenerCluster = nullptr;

        dfloat rev[4] = { 0, 0, 0, 0 };
        getSoundPlayer().listenerv(SFXLP_REVERB, rev);
        getSoundPlayer().listener(SFXLP_UPDATE, 0);
    }

    /**
     * Returns the 3D origin of the WorldStage sound listener in map space units.
     */
    Vector3d worldStageListenerOrigin() const
    {
        if(worldStageListener)
        {
            auto origin = Vector3d(worldStageListener->origin);
            origin.z += worldStageListener->height - 5;  /// @todo Make it exactly eye-level! (viewheight).
            return origin;
        }
        return Vector3d();
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
            for(dint i = activeInterfaces.count(); i--> 0; )
            {
                ActiveInterface &active = activeInterfaces[i];
                switch(active.def().geti("type"))
                {
                case AUDIO_ICD:    active.playerAs<ICdPlayer   >().setVolume(newVolume); break;
                case AUDIO_IMUSIC: active.playerAs<IMusicPlayer>().setVolume(newVolume); break;
                default: break;
                }
            }
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
                initChannels();

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

        // Re-create the sound Channels.
        initChannels();

        if(old3DMode)
        {
            // Going 2D - ensure reverb is disabled.
            disableWorldStageReverb();
        }
        old3DMode = sfx3D;
    }

    void updateWorldStageListener()
    {
        if(!soundAvail || !sfx3D) return;

        // No volume means no sound.
        if(!sfxVolume) return;

        // Update the listener mob.
        worldStageListener = getListenerMob();
        if(worldStageListener)
        {
            {
                // Origin. At eye-level.
                auto const origin = Vector4f(worldStageListenerOrigin().toVector3f(), 0);
                dfloat vec[4];
                origin.decompose(vec);
                getSoundPlayer().listenerv(SFXLP_POSITION, vec);
            }
            {
                // Orientation. (0,0) will produce front=(1,0,0) and up=(0,0,1).
                dfloat vec[2] = {
                    worldStageListener->angle / (dfloat) ANGLE_MAX * 360,
                    (worldStageListener->dPlayer ? LOOKDIR2DEG(worldStageListener->dPlayer->lookDir) : 0)
                };
                getSoundPlayer().listenerv(SFXLP_ORIENTATION, vec);
            }
            {
                // Velocity. The unit is world distance units per second
                auto const velocity = Vector4f(Vector3d(worldStageListener->mom).toVector3f(), 0) * TICSPERSEC;
                dfloat vec[4];
                velocity.decompose(vec);
                getSoundPlayer().listenerv(SFXLP_VELOCITY, vec);
            }

            // Reverb effects. Has the current sector cluster changed?
            SectorCluster *newCluster = Mobj_ClusterPtr(*worldStageListener);
            if(newCluster && (!worldStageListenerCluster || worldStageListenerCluster != newCluster))
            {
                worldStageListenerCluster = newCluster;

                // It may be necessary to recalculate the reverb properties...
                AudioEnvironmentFactors const &envFactors = worldStageListenerCluster->reverb();
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

    /**
     * Returns the currently active, primary CdPlayer.
     */
    ICdPlayer &getCdPlayer() const
    {
        // The primary interface is the first one.
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface const &active = activeInterfaces[i];
            if(active.def().geti("type") == AUDIO_ICD)
                return active.playerAs<ICdPlayer>();
        }
        /// Internal Error: No suitable cd player is available.
        throw Error("audio::System::Instance::getCdPlayer", "No CdPlayer available");
    }

    /**
     * Returns the currently active, primary MusicPlayer.
     */
    IMusicPlayer &getMusicPlayer() const
    {
        // The primary interface is the first one.
        for(dint i = activeInterfaces.count(); i--> 0; )
        {
            ActiveInterface const &active = activeInterfaces[i];
            if(active.def().geti("type") == AUDIO_IMUSIC)
                return active.playerAs<IMusicPlayer>();
        }
        /// Internal Error: No suitable music player is available.
        throw Error("audio::System::Instance::getMusicPlayer", "No MusicPlayer available");
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
            if(active.def().geti("type") == AUDIO_ISFX)
                return active.playerAs<ISoundPlayer>();
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
        os << _E(Ta) _E(l) "  " << playbackInterfaceTypeAsText(PlaybackInterfaceType( active.def().geti("type") )) << ": "
           << _E(.) _E(Tb) << active.def().gets("identityKey") << "\n";
    }

    return str.rightStrip();

#undef TABBED
}

Channels &System::channels() const
{
    DENG2_ASSERT(d->channels.get() != nullptr);
    return *d->channels;
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

void System::resetSoundStage(SoundStage soundStage)
{
    if(soundStage == WorldStage)
    {
        d->logicalSounds.clear();
    }
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
    for(dint i = d->activeInterfaces.count(); i--> 0; )
    {
        Instance::ActiveInterface const &active = d->activeInterfaces[i];
        switch(active.def().geti("type"))
        {
        case AUDIO_ICD:
            if(active.playerAs<ICdPlayer   >().isPlaying()) return true;
            break;

        case AUDIO_IMUSIC:
            if(active.playerAs<IMusicPlayer>().isPlaying()) return true;
            break;

        default: break;
        }
    }
    return false;
}

void System::stopMusic()
{
    if(!d->musicAvail) return;

    LOG_AS("audio::System");
    d->musicCurrentSong = "";

    // Stop all interfaces.
    for(dint i = d->activeInterfaces.count(); i--> 0; )
    {
        Instance::ActiveInterface const &active = d->activeInterfaces[i];
        switch(active.def().geti("type"))
        {
        case AUDIO_ICD:    active.playerAs<ICdPlayer   >().stop(); break;
        case AUDIO_IMUSIC: active.playerAs<IMusicPlayer>().stop(); break;
        default: break;
        }
    }
}

void System::pauseMusic(bool doPause)
{
    if(!d->musicAvail) return;

    LOG_AS("audio::System");
    d->musicPaused = !d->musicPaused;

    // Pause playback on all interfaces.
    for(dint i = d->activeInterfaces.count(); i--> 0; )
    {
        Instance::ActiveInterface const &active = d->activeInterfaces[i];
        switch(active.def().geti("type"))
        {
        case AUDIO_ICD:    active.playerAs<ICdPlayer   >().pause(doPause); break;
        case AUDIO_IMUSIC: active.playerAs<IMusicPlayer>().pause(doPause); break;
        default: break;
        }
    }
}

bool System::musicIsPaused() const
{
    return d->musicPaused;
}

dint System::playMusic(Record const &definition, bool looped)
{
    if(::isDedicated) return true;

    if(!d->musicAvail) return false;

    LOG_AS("audio::System");
    LOG_AUDIO_MSG("Playing song \"%s\"%s") << definition.gets("id") << (looped ? " looped" : "");
    //LOG_AUDIO_VERBOSE("Current song '%s'") << d->musCurrentSong;

    // We will not restart the currently playing song.
    if(definition.gets("id") == d->musicCurrentSong && musicIsPlaying())
        return false;

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

bool System::soundIsPlaying(SoundStage soundStage, dint soundId, mobj_t *emitter) const
{
    // Use the logic sound hash to determine whether the referenced sound is being
    // played currently. We don't care whether its audible or not.
    return d->logicalSounds.isPlaying(soundId, emitter);

#if 0
    // Use the sound channels to determine whether the referenced sound is actually
    // being played currently.
    return d->channels->isPlaying(soundId, emitter);
#endif
}

bool System::playSound(SoundStage soundStage, dint soundIdAndFlags, mobj_t *emitter,
    coord_t const *origin,  dfloat volume)
{
    LOG_AS("audio::System");

    if(soundStage == WorldStage)
    {
        d->logicalSounds.start(soundIdAndFlags, emitter);
    }

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
    sfxinfo_t const *info = Def_GetSoundInfo(soundId, &freq, &volume);
    if(!info) return false;  // Hmm? This ID is not defined.

    bool const isRepeating = (soundIdAndFlags & DDSF_REPEAT) || Def_SoundIsRepeating(soundId);

    // Check the distance (if applicable).
    if(emitter || origin)
    {
        if(!(info->flags & SF_NO_ATTENUATION) && !(soundIdAndFlags & DDSF_NO_ATTENUATION))
        {
            // If origin is too far, don't even think about playing the sound.
            if(distanceToWorldStageListener(emitter ? emitter->origin : origin) > sfxDistMax)
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
        d->channels->stopGroup(info->group, (info->flags & SF_GLOBAL_EXCLUDE) ? nullptr : emitter);
    }

    // Let's play it.
    dint flags = 0;
    flags |= (((info->flags & SF_NO_ATTENUATION) || (soundIdAndFlags & DDSF_NO_ATTENUATION)) ? SF_NO_ATTENUATION : 0);
    flags |= (isRepeating ? SF_REPEAT : 0);
    flags |= ((info->flags & SF_DONT_STOP) ? SF_DONT_STOP : 0);
    return d->playSound(*sample, volume, freq, emitter, origin, flags);
}

void System::stopSound(SoundStage soundStage, dint soundId, mobj_t *emitter, dint flags)
{
    // Are we performing any special stop behaviors?
    if(soundStage == WorldStage)
    {
        if(emitter && flags)
        {
            // Sector-based sound stopping.

            SoundEmitter *secEmitter = nullptr;
            if(emitter->thinker.id)
            {
                /// @var emitter is a map-object.
                secEmitter = &Mobj_Sector(emitter)->soundEmitter();
            }
            else
            {
                // The head of the chain is the sector. Find it.
                while(emitter->thinker.prev)
                {
                    emitter = (mobj_t *)emitter->thinker.prev;
                }
                secEmitter = (SoundEmitter *)emitter;
            }
            DENG2_ASSERT(secEmitter);

            // Stop sounds emitted by the Sector's emitter?
            if(flags & SSF_SECTOR)
            {
                stopSound(soundStage, soundId, (mobj_t *)secEmitter);
            }

            // Stop sounds emitted by Sector-linked (plane/wall) emitters?
            if(flags & SSF_SECTOR_LINKED_SURFACES)
            {
                // Process the rest of the emitter chain.
                while((secEmitter = (SoundEmitter *)secEmitter->thinker.next))
                {
                    // Stop sounds from this emitter.
                    stopSound(soundStage, soundId, (mobj_t *)secEmitter);
                }
            }
            return;
        }
    }

    // No special stop behavior.
    d->channels->stopWithLowerPriority(soundId, emitter, -1);

    if(soundStage == WorldStage)
    {
        // Stop logical sound tracking.
        d->logicalSounds.stop(soundId, emitter);
    }
}

mobj_t *System::worldStageListener()
{
    return d->worldStageListener;
}

coord_t System::distanceToWorldStageListener(Vector3d const &point) const
{
    if(mobj_t const *listener = getListenerMob())
    {
        return Mobj_ApproxPointDistance(*listener, point);
    }
    return 0;
}

Ranged System::worldStageSoundVolumeAttenuationRange() const
{
    return Ranged(sfxDistMin, sfxDistMax);
}

void System::requestWorldStageListenerUpdate()
{
    // Request a listener reverb update at the end of the frame.
    d->worldStageListenerCluster = nullptr;
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
        requestWorldStageListenerUpdate();

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
        d->updateSoundPositioningIfChanged();
        d->updateUpsampleRateIfChanged();

        // Should we purge the cache (to conserve memory)?
        d->sampleCache.maybeRunPurge();
    }

    d->logicalSounds.setOnePerSoundEmitter(sfxOneSoundPerEmitter);
    d->logicalSounds.maybeRunPurge();
}

void System::endFrame()
{
    LOG_AS("audio::System");

    if(soundPlaybackAvailable() && !BusyMode_Active())
    {
        // Update WorldStage listener properties.
        // If no listener is available - no 3D positioning is done.
        d->worldStageListener = getListenerMob();
        d->updateWorldStageListener();
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

    // Print a summary of the active configuration to the log.
    LOG_AUDIO_MSG("%s") << description();
}

void System::deinitPlayback()
{
    LOG_AS("audio::System");

    d->deinitSound();
    d->deinitMusic();

    d->unloadDrivers();
}

void System::allowSoundRefresh(bool allow)
{
    for(dint i = d->activeInterfaces.count(); i--> 0; )
    {
        Instance::ActiveInterface const &active = d->activeInterfaces[i];
        if(active.def().geti("type") == AUDIO_ISFX)
        {
            active.playerAs<ISoundPlayer>().allowRefresh(allow);
        }
    }
}

void System::worldMapChanged()
{
    // Update who is listening now.
    d->worldStageListener = getListenerMob();
}

String System::playbackInterfaceTypeAsText(PlaybackInterfaceType type)  // static
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
    System::get().requestWorldStageListenerUpdate();
}

static void musicMidiFontChanged()
{
    System::get().updateMusicMidiFont();
}

void System::consoleRegister()  // static
{
    // Drivers:
    C_CMD("listaudiodrivers",   nullptr, ListDrivers);
    C_CMD("inspectaudiodriver", "s",     InspectDriver);

    // Sound effects:
    C_VAR_INT     ("sound-16bit",         &sfx16Bit,              0, 0, 1);
    C_VAR_INT     ("sound-3d",            &sfx3D,                 0, 0, 1);
    C_VAR_BYTE    ("sound-overlap-stop",  &sfxOneSoundPerEmitter, 0, 0, 1);
    C_VAR_INT     ("sound-rate",          &sfxSampleRate,         0, 11025, 44100);
    C_VAR_FLOAT2  ("sound-reverb-volume", &sfxReverbStrength,     0, 0, 1.5f, sfxReverbStrengthChanged);
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
    C_VAR_INT     ("sound-info",          &showSoundInfo,         0, 0, 1);
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
    return (dd_bool) audio::System::get().soundIsPlaying(audio::WorldStage, soundId, emitter);
}

void S_StopSound2(dint soundId, mobj_t *emitter, dint flags)
{
    audio::System::get().stopSound(audio::WorldStage, soundId, emitter, flags);
}

void S_StopSound(dint soundId, mobj_t *emitter)
{
    S_StopSound2(soundId, emitter, 0/*flags*/);
}

dint S_LocalSoundAtVolumeFrom(dint soundIdAndFlags, mobj_t *emitter, coord_t *origin,
    dfloat volume)
{
    return audio::System::get().playSound(audio::LocalStage, soundIdAndFlags, emitter, origin, volume);
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
    return audio::System::get().playSound(audio::WorldStage, soundIdAndFlags, emitter, nullptr, volume);
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
