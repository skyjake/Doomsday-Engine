/** @file serverapp.cpp  The server application.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "serverapp.h"

#include "serverplayer.h"
#include "server/sv_sound.h"

#include "api_audiod_sfx.h"    // sfxsample_t
#include "api_sound.h"
#include "audio/logicsound.h"
#include "audio/samplecache.h"

#include "api_map.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "Sector"

#include "dd_main.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "sys_system.h"
#include "def_main.h"
#if WIN32
#  include "dd_winit.h"
#elif UNIX
#  include "dd_uinit.h"
#endif
#include <de/Log>
#include <de/Error>
#include <de/Garbage>
#include <de/c_wrapper.h>
#include <de/timer.h>
#include <QDebug>
#include <QMultiHash>
#include <QMutableHashIterator>
#include <QNetworkProxyFactory>
#include <QtAlgorithms>
#include <cstdlib>

using namespace de;

static ServerApp *serverAppSingleton = 0;

static void handleAppTerminate(char const *msg)
{
    qFatal("Application terminated due to exception:\n%s\n", msg);
}

DENG2_PIMPL(ServerApp)
, DENG2_OBSERVES(Plugins, PublishAPI)
{
    QScopedPointer<ServerSystem> serverSystem;
    QScopedPointer<ResourceSystem> resourceSys;
    WorldSystem worldSys;
    InFineSystem infineSys;

    typedef QMultiHash<dint /*key: soundId*/, audio::LogicSound *> LogicalSoundHash;
    typedef QMutableHashIterator<dint /*key: soundId*/, audio::LogicSound *> MutableLogicalSoundHashIterator;
    LogicalSoundHash logicalSounds;
    audio::SampleCache sampleCache;

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
            audio::LogicSound const &lsound = *it.value();
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
            audio::LogicSound const &lsound = *it.value();
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

        audio::LogicSound const &lsound = *it.value();
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

    // Cache the sound sample associated with @a soundId (if necessary)
    // so that we can determine it's length.
    if(sfxsample_t *sample = d->sampleCache.cache(soundId))
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

        auto *ls = new audio::LogicSound;
        ls->emitter     = emitter;
        ls->isRepeating = isRepeating;
        ls->endTime     = Timer_RealMilliseconds() + length;
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
 * Server's side) -ds
 */

void S_PauseMusic(dd_bool)
{
    // Stub: We don't play music locally on server side.
}

void S_StopMusic()
{
    // Stub: We don't play music locally on server side.
}

dd_bool S_StartMusicNum(dint, dd_bool)
{
    // We don't play music locally on server side.
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

dd_bool S_SoundIsPlaying(dint soundId, mobj_t *emitter)
{
    // Use the logic sound hash to determine whether the referenced sound is being
    // played currently. We don't care whether its audible or not.
    return (dd_bool) ServerApp::app().logicalSoundIsPlaying(soundId, emitter);
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

    // Notify the LSM.
    if(ServerApp::app().stopLogicalSound(soundId, emitter))
    {
        // In netgames, the server is responsible for telling clients when to stop sounds.
        // The LSM will tell us if a sound was stopped somewhere in the world.
        Sv_StopSound(soundId, emitter);
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

dint S_LocalSoundAtVolumeFrom(dint, mobj_t *, coord_t *, dfloat)
{
    // We don't play sounds locally on server side.
    return false;
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
    // The sound is audible to everybody.
    Sv_Sound(soundIdAndFlags, emitter, SVSF_TO_ALL);
    ServerApp::app().startLogicalSound(soundIdAndFlags, emitter);

    return S_LocalSound(soundIdAndFlags, emitter);
}

dint S_StartSoundEx(dint soundIdAndFlags, mobj_t *emitter)
{
    Sv_Sound(soundIdAndFlags, emitter, SVSF_TO_ALL | SVSF_EXCLUDE_ORIGIN);
    ServerApp::app().startLogicalSound(soundIdAndFlags, emitter);

    return S_LocalSound(soundIdAndFlags, emitter);
}

dint S_StartSoundAtVolume(dint soundIdAndFlags, mobj_t *emitter, dfloat volume)
{
    Sv_SoundAtVolume(soundIdAndFlags, emitter, volume, SVSF_TO_ALL);
    ServerApp::app().startLogicalSound(soundIdAndFlags, emitter);

    // The sound is audible to everybody.
    return S_LocalSoundAtVolume(soundIdAndFlags, emitter, volume);
}

dint S_ConsoleSound(dint soundIdAndFlags, mobj_t *emitter, dint targetConsole)
{
    Sv_Sound(soundIdAndFlags, emitter, targetConsole);

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
