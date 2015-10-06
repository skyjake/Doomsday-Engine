/** @file serverapp.cpp  Server application (Singleton).
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "serverapp.h"

#include "serverplayer.h"
#include "server/sv_sound.h"

#include "api_sound.h"

#include "api_map.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "Sector"

#include "dd_main.h"
#if WIN32
#  include "dd_winit.h"
#elif UNIX
#  include "dd_uinit.h"
#endif
#include "dd_def.h"
#include "dd_loop.h"
#include "def_main.h"
#include "sys_system.h"
#include <doomsday/filesys/fs_main.h>
#include <doomsday/resource/wav.h>
#include <de/Error>
#include <de/Garbage>
#include <de/Log>
#include <de/c_wrapper.h>
#include <de/memoryzone.h>
#include <de/timer.h>
#include <QDebug>
#include <QHash>
#include <QMultiHash>
#include <QMutableHashIterator>
#include <QNetworkProxyFactory>
#include <QtAlgorithms>
#include <cstdlib>
#include <cstring>

using namespace de;

static ServerApp *serverAppSingleton = 0;

static void handleAppTerminate(char const *msg)
{
    qFatal("Application terminated due to exception:\n%s\n", msg);
}

struct WaveformData
{
    dint soundId    = 0;  ///< Id number of the sound.
    dint rate       = 0;  ///< Samples per second.
    dint numSamples = 0;  ///< Number of samples.
    dint bytesPer   = 0;  ///< Bytes per sample (1 or 2).

    inline bool isRepeating() const {
        return Def_SoundIsRepeating(soundId);
    }

    inline dsize size() const {
        return bytesPer * numSamples;
    }
};

/**
 * LogicSounds are used to track currently playing sounds on a logical level (irrespective
 * of whether playback is available, or if the sounds are actually audible to anyone).
 */
struct LogicSound
{
    bool repeat     = false;
    mobj_t *emitter = nullptr;
    duint endTime   = 0;

    bool inline isPlaying(duint nowTime) const {
        return (repeat || endTime > nowTime);
    }
};

DENG2_PIMPL(ServerApp)
, DENG2_OBSERVES(Plugins, PublishAPI)
{
    QScopedPointer<ServerSystem> serverSystem;
    QScopedPointer<ResourceSystem> resourceSys;
    WorldSystem worldSys;
    InFineSystem infineSys;

    typedef QMultiHash<dint /*key: soundId*/, LogicSound *> LogicalSoundHash;
    typedef QMutableHashIterator<dint /*key: soundId*/, LogicSound *> MutableLogicalSoundHashIterator;
    LogicalSoundHash logicalSounds;

    typedef QHash<dint /*(key: soundId*/, WaveformData> WaveformDataHash;
    WaveformDataHash waveformData;

    Instance(Public *i) : Base(i)
    {
        serverAppSingleton = thisPublic;

        DoomsdayApp::plugins().audienceForPublishAPI() += this;
    }

    ~Instance()
    {
        clearLogicalSounds();
        Sys_Shutdown();
        DD_Shutdown();
    }

    void clearLogicalSounds()
    {
        qDeleteAll(logicalSounds);
        logicalSounds.clear();
    }

    /**
     * Lookup WaveformData for a sound sample associated with @a soundId.
     *
     * @param soundId  Unique sound identifier.
     *
     * @return  Associated WaveformData if found; otherwise @c nullptr.
     */
    WaveformData const *tryFindWaveformData(dint soundId)
    {
        // Lookup info for this sound.
        sfxinfo_t const *info = Def_GetSoundInfo(soundId);
        if(!info)
        {
            // Ignore invalid sound IDs.
            if(soundId > 0)
            {
                LOG_AUDIO_WARNING("Ignoring sound id:%i (missing sfxinfo_t)") << soundId;
            }
            return nullptr;
        }

        // Have we already cached this?
        auto const existing = waveformData.constFind(soundId);
        if(existing != waveformData.cend()) return &existing.value();

        // Attempt to cache this now.
        LOG_AUDIO_VERBOSE("Caching sample '%s' (id:%i)...") << info->id << soundId;

        dint bytesPer   = 0;
        dint rate       = 0;
        dint numSamples = 0;

        /**
         * Figure out where to get the sample data for this sound. It might be from a
         * data file such as a WAD or external sound resources. The definition and the
         * configuration settings will help us in making the decision.
         */
        bool found = false;

        /// Has an external sound file been defined?
        /// @note Path is relative to the base path.
        if(!Str_IsEmpty(&info->external))
        {
            String const searchPath = App_BasePath() / String(Str_Text(&info->external));

            // Try loading.
            if(void *data = WAV_Load(searchPath.toUtf8().constData(), &bytesPer, &rate, &numSamples))
            {
                Z_Free(data);
                found = true;
                bytesPer /= 8;  // Was returned as bits.
            }
        }

        // If external didn't succeed, let's try the default resource dir.
        if(!found)
        {
            /**
             * If the sound has an invalid lumpname, search external anyway. If the
             * original sound is from a PWAD, we won't look for an external resource
             * (probably a custom sound).
             *
             * @todo should be a cvar.
             */
            if(info->lumpNum < 0 || !App_FileSystem().lump(info->lumpNum).container().hasCustom())
            {
                try
                {
                    String const foundPath =
                        App_BasePath() / App_FileSystem().findPath(de::Uri(info->lumpName, RC_SOUND),
                                                                   RLF_DEFAULT, App_ResourceClass(RC_SOUND));

                    if(void *data = WAV_Load(foundPath.toUtf8().constData(), &bytesPer, &rate, &numSamples))
                    {
                        // Loading was successful.
                        Z_Free(data);
                        found = true;
                        bytesPer /= 8;  // Was returned as bits.
                    }
                }
                catch(FS1::NotFoundError const &)
                {}  // Ignore this error.
            }
        }

        // No sample loaded yet?
        if(!found)
        {
            // Try loading from the lump.
            if(info->lumpNum < 0)
            {
                LOG_AUDIO_WARNING("Failed to locate lump resource '%s' for sample '%s'")
                    << info->lumpName << info->id;
                return nullptr;
            }

            File1 &lump = App_FileSystem().lump(info->lumpNum);
            if(WAV_Recognize(lump))
            {
                // Load as WAV, then.
                if(void *data = WAV_MemoryLoad((byte const *) lump.cache(), lump.size(),
                                                &bytesPer, &rate, &numSamples))
                {
                    Z_Free(data);
                    found = true;
                    bytesPer /= 8;  // Was returned as bits.
                }
                lump.unlock();
            }

            if(!found)
            {
                // Abort...
                LOG_AUDIO_WARNING("Unknown WAV format in lump '%s'") << info->lumpName;
                return nullptr;
            }
        }

        if(found)  // Loaded!
        {
            // Insert/replace existing database record for this sound sample.
            WaveformData rec;
            rec.soundId    = soundId;
            rec.rate       = rate;
            rec.numSamples = numSamples;
            rec.bytesPer   = bytesPer;
            return &waveformData.insert(soundId, rec).value();  // A copy is made.
        }

        // Probably an old-fashioned DOOM sample.
        if(info->lumpNum >= 0)
        {
            File1 /*const &*/lump = App_FileSystem().lump(info->lumpNum);
            if(lump.size() > 8)
            {
                duint8 hdr[8];
                lump.read(hdr, 0, 8);
                lump.unlock();

                dint const head = DD_SHORT(*(dshort const *) (hdr));

                rate       = DD_SHORT(*(dshort const *) (hdr + 2));
                numSamples = de::max(0, DD_LONG(*(dint const *) (hdr + 4)));
                bytesPer   = 1;  // 8-bit.

                if(head == 3 && numSamples > 0 && (unsigned) numSamples <= lump.size() - 8)
                {
                    // Insert/replace existing database record for this sound sample.
                    WaveformData rec;
                    rec.soundId    = soundId;
                    rec.rate       = rate;
                    rec.numSamples = numSamples;
                    rec.bytesPer   = bytesPer;
                    return &waveformData.insert(soundId, rec).value();  // A copy is made.
                }
            }
        }

        LOG_AUDIO_WARNING("Unknown lump '%s' sound format") << info->lumpName;
        return nullptr;
    }

    void publishAPIToPlugin(::Library *plugin)
    {
        DD_PublishAPIs(plugin);
    }

#ifdef UNIX
    void printVersionToStdOut()
    {
        printf("%s\n", String("%1 %2")
               .arg(DOOMSDAY_NICENAME)
               .arg(DOOMSDAY_VERSION_FULLTEXT)
               .toLatin1().constData());
    }

    void printHelpToStdOut()
    {
        printVersionToStdOut();
        printf("Usage: %s [options]\n", self.commandLine().at(0).toLatin1().constData());
        printf(" -iwad (dir)  Set directory containing IWAD files.\n");
        printf(" -file (f)    Load one or more PWAD files at startup.\n");
        printf(" -game (id)   Set game to load at startup.\n");
        printf(" --version    Print current version.\n");
        printf("For more options and information, see \"man doomsday-server\".\n");
    }
#endif
};

ServerApp::ServerApp(int &argc, char **argv)
    : TextApp(argc, argv)
    , DoomsdayApp([] () -> Player * { return new ServerPlayer; })
    , d(new Instance(this))
{   
    novideo = true;

    // Override the system locale (affects number/time formatting).
    QLocale::setDefault(QLocale("en_US.UTF-8"));

    // Use the host system's proxy configuration.
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Metadata.
    setMetadata("Deng Team", "dengine.net", "Doomsday Server", DOOMSDAY_VERSION_BASE);
    setUnixHomeFolderName(".doomsday");

    setTerminateFunc(handleAppTerminate);

    d->serverSystem.reset(new ServerSystem);
    addSystem(*d->serverSystem);

    d->resourceSys.reset(new ResourceSystem);
    addSystem(*d->resourceSys);

    addSystem(d->worldSys);
    //addSystem(d->infineSys);

    // We must presently set the current game manually (the collection is global).
    setGame(games().nullGame());
}

ServerApp::~ServerApp()
{
    d.reset();

    // Now that everything is shut down we can forget about the singleton instance.
    serverAppSingleton = 0;
}

void ServerApp::initialize()
{
    Libdeng_Init();

#ifdef UNIX
    // Some common Unix command line options.
    if(commandLine().has("--version") || commandLine().has("-version"))
    {
        d->printVersionToStdOut();
        ::exit(0);
    }
    if(commandLine().has("--help") || commandLine().has("-h") || commandLine().has("-?"))
    {
        d->printHelpToStdOut();
        ::exit(0);
    }
#endif

    if(!CommandLine_Exists("-stdout"))
    {
        // In server mode, stay quiet on the standard outputs.
        LogBuffer::get().enableStandardOutput(false);
    }

    Def_Init();

    // Load the server's packages.
    addInitPackage("net.dengine.base");
    initSubsystems();

    // Initialize.
#if WIN32
    if(!DD_Win32_Init())
    {
        throw Error("ServerApp::initialize", "DD_Win32_Init failed");
    }
#elif UNIX
    if(!DD_Unix_Init())
    {
        throw Error("ServerApp::initialize", "DD_Unix_Init failed");
    }
#endif

    plugins().loadAll();

    DD_FinishInitializationAfterWindowReady();
}

ServerApp &ServerApp::app()
{
    DENG2_ASSERT(serverAppSingleton != 0);
    return *serverAppSingleton;
}

ServerSystem &ServerApp::serverSystem()
{
    return *app().d->serverSystem;
}

InFineSystem &ServerApp::infineSystem()
{
    return app().d->infineSys;
}

ResourceSystem &ServerApp::resourceSystem()
{
    return *app().d->resourceSys;
}

WorldSystem &ServerApp::worldSystem()
{
    return app().d->worldSys;
}

// Remote sound scheduling: -------------------------------------------------------------

Ranged ServerApp::soundVolumeAttenuationRange()
{
    return Ranged(256, 2025);
}

void ServerApp::clearAllLogicalSounds()
{
    d->clearLogicalSounds();
}

bool ServerApp::logicalSoundIsPlaying(dint soundId, mobj_t *emitter) const
{
    duint const nowTime = Timer_RealMilliseconds();
    if(soundId)
    {
        auto it = d->logicalSounds.constFind(soundId);
        while(it != d->logicalSounds.constEnd() && it.key() == soundId)
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
        auto it = d->logicalSounds.constBegin();
        while(it != d->logicalSounds.constEnd())
        {
            LogicSound const &lsound = *it.value();
            if(lsound.emitter == emitter && lsound.isPlaying(nowTime))
                return true;

            ++it;
        }
    }
    return false;
}

dint ServerApp::stopLogicalSound(dint soundId, mobj_t *emitter)
{
    dint numStopped = 0;
    Instance::MutableLogicalSoundHashIterator it(d->logicalSounds);
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
 * The sound is entered into the list of playing sounds. Called when a 'world class' sound
 * is started (regardless of whether it's actually started on the remote system).
 *
 * @todo Why do we cache sound samples and/or care to know the length of the samples? It
 * is entirely possible that the Client is using a different set of samples so using this
 * information on server side for scheduling of remote playback events? is not logical. -ds
 */
void ServerApp::startLogicalSound(dint soundIdAndFlags, mobj_t *emitter)
{
    dint const soundId = (soundIdAndFlags & ~DDSF_FLAG_MASK);

    // Only start a logical sound if WaveformData associated with the given @a soundId is
    // available (necessary for scheduling remote playback, which needs to know the length
    // of the sound in seconds).
    if(WaveformData const *sample = d->tryFindWaveformData(soundId))
    {
        bool const repeat = (soundIdAndFlags & DDSF_REPEAT) || sample->isRepeating();

        duint length = (1000 * sample->numSamples) / sample->rate;
        if(repeat)
        {
            length = de::min<duint>(length, 1);
        }

        // Ignore zero length sounds.
        /// @todo Shouldn't we still stop others, though? -ds
        if(!length) return;

        auto *ls = new LogicSound;
        ls->emitter = emitter;
        ls->repeat  = repeat;
        ls->endTime = Timer_RealMilliseconds() + length;
        d->logicalSounds.insert(soundId, ls);
    }
}

// Sound API: ---------------------------------------------------------------------------
/*
 * On Server side the sound API provides an interface to a remote event scheduler which
 * instructs connected Clients on when (and how) to play both sounds and music (note that
 * presently music is handled entirely on the Client side).
 *
 * @todo The premise behind this functionality is fundamentally flawed in that it assumes
 * that the same samples are used by both the Client and the Server, and that the latter
 * schedules remote playback of the former (determined by examining sample lengths on the
 * Server's side). -ds
 */

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

dint S_StartSound(dint soundIdAndFlags, mobj_t *emitter)
{
    // The sound is audible to everybody.
    Sv_Sound(soundIdAndFlags, emitter, SVSF_TO_ALL);
    ServerApp::app().startLogicalSound(soundIdAndFlags, emitter);

    // We don't play sounds locally on server side.
    return false;
}

dint S_StartSoundEx(dint soundIdAndFlags, mobj_t *emitter)
{
    Sv_Sound(soundIdAndFlags, emitter, SVSF_TO_ALL | SVSF_EXCLUDE_ORIGIN);
    ServerApp::app().startLogicalSound(soundIdAndFlags, emitter);

    // We don't play sounds locally on server side.
    return false;
}

dint S_StartSoundAtVolume(dint soundIdAndFlags, mobj_t *emitter, dfloat volume)
{
    // The sound is audible to everybody.
    Sv_SoundAtVolume(soundIdAndFlags, emitter, volume, SVSF_TO_ALL);
    ServerApp::app().startLogicalSound(soundIdAndFlags, emitter);

    // We don't play sounds locally on server side.
    return false;
}

dint S_ConsoleSound(dint soundIdAndFlags, mobj_t *emitter, dint targetConsole)
{
    Sv_Sound(soundIdAndFlags, emitter, targetConsole);
    return true;
}

void S_StopSound2(dint soundId, mobj_t *emitter, dint flags)
{
    // Are we performing any special stop behaviors?
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
            _api_S.StopSound(soundId, (mobj_t *)secEmitter);
        }

        // Stop sounds emitted by Sector-linked (plane/wall) emitters?
        if(flags & SSF_SECTOR_LINKED_SURFACES)
        {
            // Process the rest of the emitter chain.
            while((secEmitter = (SoundEmitter *)secEmitter->thinker.next))
            {
                // Stop sounds from this emitter.
                _api_S.StopSound(soundId, (mobj_t *)secEmitter);
            }
        }
        return;
    }

    // No special stop behavior.

    // Notify the LSM.
    if(ServerApp::app().stopLogicalSound(soundId, emitter))
    {
        // We are responsible for instructing connected Clients when to stop sounds.
        // The LSM will tell us if a sound was stopped somewhere in the world.
        Sv_StopSound(soundId, emitter);
    }
}

void S_StopSound(dint soundId, mobj_t *emitter)
{
    S_StopSound2(soundId, emitter, 0/*flags*/);
}

dd_bool S_SoundIsPlaying(dint soundId, mobj_t *emitter)
{
    // Use the logic sound hash to determine whether the referenced sound is being
    // played currently. We don't care whether its audible or not.
    return (dd_bool) ServerApp::app().logicalSoundIsPlaying(soundId, emitter);
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
