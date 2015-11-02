/** @file worldsystem.cpp  World subsystem.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "world/worldsystem.h"

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "client/cl_def.h"
#  include "client/cl_frame.h"
#  include "client/cl_player.h"
#endif
#ifdef __SERVER__
#  include "serverapp.h"
#  include "server/sv_pool.h"
#endif

#include "api_player.h"
#include "world/p_players.h"

#include "world/p_ticker.h"
#include "world/sky.h"
#include "world/thinkers.h"
#include "world/bindings_world.h"
#include "edit_map.h"
#include "Plane"
#include "Sector"
#include "SectorCluster"
#include "Surface"
#ifdef __CLIENT__
#  include "world/contact.h"
#  include "Hand"
#  include "HueCircle"
#  include "Lumobj"

#  include "gl/gl_main.h"

#  include "audio/mixer.h"

#  include "render/viewports.h"  // R_ResetViewer
#  include "render/rend_fakeradio.h"
#  include "render/rend_main.h"
#  include "render/rendpoly.h"
#  include "MaterialAnimator"

#  include "edit_bias.h"
#  include "ui/progress.h"
#endif

#include "network/net_main.h"

#include "dd_main.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "def_main.h"  // ::defs
#include <doomsday/doomsdayapp.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>
#include <doomsday/defs/mapinfo.h>
#include <de/Error>
#include <de/Log>
#include <de/Time>
#include <de/memoryzone.h>
#include <de/timer.h>
#include <QMap>
#include <QtAlgorithms>
#include <map>
#include <utility>

using namespace de;

dint validCount = 1;  // Increment every time a check is made.

#ifdef __CLIENT__
static dfloat handDistance = 300;  //cvar
#endif

static inline ResourceSystem &resSys()
{
    return App_ResourceSystem();
}

#ifdef __CLIENT__
static inline RenderSystem &rendSys()
{
    return ClientApp::renderSystem();
}
#endif

/**
 * Observes the progress of a map conversion and records any issues/problems that
 * are encountered in the process. When asked, compiles a human-readable report
 * intended to assist mod authors in debugging their maps.
 *
 * @todo Consolidate with the missing material reporting done elsewhere -ds
 */
class MapConversionReporter
: DENG2_OBSERVES(Map, UnclosedSectorFound)
, DENG2_OBSERVES(Map, OneWayWindowFound)
, DENG2_OBSERVES(world::Map, Deletion)
{
    /// Record "unclosed sectors".
    /// Sector index => world point relatively near to the problem area.
    typedef std::map<dint, Vector2i> UnclosedSectorMap;

    /// Record "one-way window lines".
    /// Line index => Sector index the back side faces.
    typedef std::map<dint, dint> OneWayWindowMap;

    /// Maximum number of warnings to output (of each type) about any problems
    /// encountered during the build process.
    static dint const maxWarningsPerType;

public:
    /**
     * Construct a new conversion reporter.
     * @param map
     */
    MapConversionReporter(Map *map = nullptr)
    {
        setMap(map);
    }

    ~MapConversionReporter()
    {
        observeMap(false);
    }

    /**
     * Change the map to be reported on. Note that any existing report data is
     * retained until explicitly cleared.
     */
    void setMap(Map *newMap)
    {
        if(_map != newMap)
        {
            observeMap(false);
            _map = newMap;
            observeMap(true);
        }
    }

    /// @see setMap(), clearReport()
    inline void setMapAndClearReport(Map *newMap)
    {
        setMap(newMap);
        clearReport();
    }

    /// Same as @code setMap(nullptr); @endcode
    inline void clearMap() { setMap(nullptr); }

    /**
     * Clear any existing conversion report data.
     */
    void clearReport()
    {
        _unclosedSectors.clear();
        _oneWayWindows.clear();
    }

    /**
     * Compile and output any existing report data to the message log.
     */
    void writeLog()
    {
        if(dint numToLog = maxWarnings(unclosedSectorCount()))
        {
            String str;

            UnclosedSectorMap::const_iterator it = _unclosedSectors.begin();
            for(dint i = 0; i < numToLog; ++i, ++it)
            {
                if(i != 0) str += "\n";
                str += String("Sector #%1 is unclosed near %2")
                           .arg(it->first).arg(it->second.asText());
            }

            if(numToLog < unclosedSectorCount())
                str += String("\n(%1 more like this)").arg(unclosedSectorCount() - numToLog);

            LOG_MAP_WARNING("%s") << str;
        }

        if(dint numToLog = maxWarnings(oneWayWindowCount()))
        {
            String str;

            OneWayWindowMap::const_iterator it = _oneWayWindows.begin();
            for(dint i = 0; i < numToLog; ++i, ++it)
            {
                if(i != 0) str += "\n";
                str += String("Line #%1 seems to be a One-Way Window (back faces sector #%2).")
                           .arg(it->first).arg(it->second);
            }

            if(numToLog < oneWayWindowCount())
                str += String("\n(%1 more like this)").arg(oneWayWindowCount() - numToLog);

            LOG_MAP_VERBOSE("%s") << str;
        }
    }

protected:
    /// Observes Map UnclosedSectorFound.
    void unclosedSectorFound(Sector &sector, Vector2d const &nearPoint)
    {
        _unclosedSectors.insert(std::make_pair(sector.indexInArchive(), nearPoint.toVector2i()));
    }

    /// Observes Map OneWayWindowFound.
    void oneWayWindowFound(Line &line, Sector &backFacingSector)
    {
        _oneWayWindows.insert(std::make_pair(line.indexInArchive(), backFacingSector.indexInArchive()));
    }

    /// Observes Map Deletion.
    void mapBeingDeleted(world::Map const &map)
    {
        DENG2_ASSERT(&map == _map);  // sanity check.
        DENG2_UNUSED(map);
        _map = nullptr;
    }

private:
    inline dint unclosedSectorCount() const { return dint( _unclosedSectors.size() ); }
    inline dint oneWayWindowCount() const   { return dint( _oneWayWindows.size() ); }

    static inline dint maxWarnings(dint issueCount)
    {
#ifdef DENG2_DEBUG
        return issueCount; // No limit.
#else
        return de::min(issueCount, maxWarningsPerType);
#endif
    }

    void observeMap(bool yes)
    {
        if(!_map) return;

        if(yes)
        {
            _map->audienceForDeletion()          += this;
            _map->audienceForOneWayWindowFound   += this;
            _map->audienceForUnclosedSectorFound += this;
        }
        else
        {
            _map->audienceForDeletion()          -= this;
            _map->audienceForOneWayWindowFound   -= this;
            _map->audienceForUnclosedSectorFound -= this;
        }
    }

    Map *_map = nullptr;  ///< Map currently being reported on, if any (not owned).
    UnclosedSectorMap _unclosedSectors;
    OneWayWindowMap   _oneWayWindows;
};

dint const MapConversionReporter::maxWarningsPerType = 10;

dd_bool ddMapSetup;

// Should we be caching successfully loaded maps?
//static byte mapCache = true; // cvar

static char const *mapCacheDir = "mapcache/";

/// Determine the identity key for maps loaded from the specified @a sourcePath.
static String cacheIdForMap(String const &sourcePath)
{
    DENG2_ASSERT(!sourcePath.isEmpty());

    dushort id = 0;
    for(dint i = 0; i < sourcePath.size(); ++i)
    {
        id ^= sourcePath.at(i).unicode() << ((i * 3) % 11);
    }

    return String("%1").arg(id, 4, 16);
}

DENG2_PIMPL(WorldSystem)
{
    Binder binder;               ///< Doomsday Script bindings for the World.
    Record worldModule;

    Map *map = nullptr;          ///< Current map.
    Record fallbackMapInfo;      ///< Used when no effective MapInfo definition.

    timespan_t time = 0;         ///< World-wide time.
#ifdef __CLIENT__
    std::unique_ptr<Hand> hand;  ///< For map editing/manipulation.
#endif

    Instance(Public *i) : Base(i)
    {
        world::initBindings(binder, worldModule);
        ScriptSystem::get().addNativeModule("World", worldModule);

        // One time init of the fallback MapInfo definition.
        defn::MapInfo(fallbackMapInfo).resetToDefaults();
    }

    /**
     * Compose the relative path (relative to the runtime directory) to the
     * directory of the cache where maps from this source (e.g., the add-on
     * which contains the map) will reside.
     *
     * @param sourcePath  Path to the primary resource file (the source) for
     *                    the original map data.
     *
     * @return  The composed path.
     */
    static Path cachePath(String sourcePath)
    {
        if(sourcePath.isEmpty()) return String();

        // Compose the final path.
        return mapCacheDir + App_CurrentGame().identityKey()
               / sourcePath.fileNameWithoutExtension()
               + '-' + cacheIdForMap(sourcePath);
    }

    /**
     * Attempt JIT conversion of the map data with the help of a plugin. Note that
     * the map is left in an editable state in case the caller wishes to perform
     * any further changes.
     *
     * @param reporter  Reporter which will observe the conversion process.
     *
     * @return  The newly converted map (if any).
     */
    Map *convertMap(res::MapManifest const &mapManifest, MapConversionReporter *reporter = nullptr)
    {
        // We require a map converter for this.
        if(!Plug_CheckForHook(HOOK_MAP_CONVERT))
            return nullptr;

        LOG_DEBUG("Attempting \"%s\"...") << mapManifest.composeUri().path();

        if(!mapManifest.sourceFile()) return nullptr;

        // Initiate the conversion process.
        MPE_Begin(nullptr/*dummy*/);

        Map *newMap = MPE_Map();

        // Associate the map with its corresponding manifest.
        newMap->setManifest(&const_cast<res::MapManifest &>(mapManifest));

        if(reporter)
        {
            // Instruct the reporter to begin observing the conversion.
            reporter->setMap(newMap);
        }

        // Ask each converter in turn whether the map format is recognizable
        // and if so to interpret and transfer it to us via the runtime map
        // editing interface.
        if(!DoomsdayApp::plugins().callAllHooks(HOOK_MAP_CONVERT, 0,
                                                const_cast<Id1MapRecognizer *>(&mapManifest.recognizer())))
            return nullptr;

        // A converter signalled success.

        // End the conversion process (if not already).
        MPE_End();

        // Take ownership of the map.
        return MPE_TakeMap();
    }

#if 0
    /**
     * Returns @c true iff data for the map is available in the cache.
     */
    bool haveCachedMap(res::MapManifest &mapManifest)
    {
        // Disabled?
        if(!mapCache) return false;
        return DAM_MapIsValid(mapManifest.cachePath, mapManifest.id());
    }

    /**
     * Attempt to load data for the map from the cache.
     *
     * @see isCachedDataAvailable()
     *
     * @return  @c true if loading completed successfully.
     */
    Map *loadMapFromCache(MapManifest &mapManifest)
    {
        Uri const mapUri = mapManifest.composeUri();
        Map *map = DAM_MapRead(mapManifest.cachePath);
        if(!map)
            /// Failed to load the map specified from the data cache.
            throw Error("loadMapFromCache", "Failed loading map \"" + mapUri.asText() + "\" from cache");

        map->_uri = mapUri;
        return map;
    }
#endif

    /**
     * Attempt to load the associated map data.
     *
     * @return  The loaded map if successful. Ownership given to the caller.
     */
    Map *loadMap(res::MapManifest const &mapManifest, MapConversionReporter *reporter = nullptr)
    {
        /*if(mapManifest.lastLoadAttemptFailed && !forceRetry)
            return nullptr;

        // Load from cache?
        if(haveCachedMap(mapManifest))
        {
            return loadMapFromCache(mapManifest);
        }*/

        // Try a JIT conversion with the help of a plugin.
        Map *map = convertMap(mapManifest, reporter);
        if(!map)
        {
            LOG_WARNING("Failed conversion of \"%s\".") << mapManifest.composeUri().path();
            //mapManifest.lastLoadAttemptFailed = true;
        }
        return map;
    }

    /**
     * Replace the current map with @a map.
     */
    void makeCurrent(Map *newMap)
    {
        // This is now the current map (if any).
        map = newMap;
        if(!map) return;

        // We cannot make an editable map current.
        DENG2_ASSERT(!map->isEditable());

        // Should we cache this map?
        /*if(mapCache && !haveCachedMap(&map->def()))
        {
            // Ensure the destination directory exists.
            F_MakePath(map->def().cachePath.toUtf8().constData());

            // Cache the map!
            DAM_MapWrite(map);
        }*/

#ifdef __CLIENT__
        // Connect the map to world audiences:
        /// @todo The map should instead be notified when it is made current
        /// so that it may perform the connection itself. Such notification
        /// would also afford the map the opportunity to prepare various data
        /// which is only needed when made current (e.g., caches for render).
        self.audienceForFrameBegin() += map;
#endif

        // Print summary information about this map.
        LOG_MAP_NOTE(_E(b) "Current map elements:");
        LOG_MAP_NOTE("%s") << map->elementSummaryAsStyledText();

        // See what MapInfo says about this map.
        Record const &mapInfo = map->mapInfo();

        map->_ambientLightLevel = mapInfo.getf("ambient") * 255;
        map->_globalGravity     = mapInfo.getf("gravity");
        map->_effectiveGravity  = map->_globalGravity;

#ifdef __CLIENT__
        // Reconfigure the sky.
        defn::Sky skyDef;
        if(Record const *def = ::defs.skies.tryFind("id", mapInfo.gets("skyId")))
        {
            skyDef = *def;
        }
        else
        {
            skyDef = mapInfo.subrecord("sky");
        }
        map->sky().configure(&skyDef);

        // Set up the SkyDrawable to get its config from the map's Sky.
        map->skyAnimator().setSky(&rendSys().sky().configure(&map->sky()));
#endif

        // Init the thinker lists (public and private).
        map->thinkers().initLists(0x1 | 0x2);

        // Must be called before we go any further.
        P_InitUnusedMobjList();

        // Must be called before any mobjs are spawned.
        map->initNodePiles();

#ifdef __CLIENT__
        // Prepare the client-side data.
        Cl_ResetFrame();
        Cl_InitPlayers();  // Player data, too.

        /// @todo Defer initial generator spawn until after finalization.
        map->initGenerators();
#endif

        // The game may need to perform it's own finalization now that the
        // "current" map has changed.
        de::Uri const mapUri = (map->hasManifest() ? map->manifest().composeUri() : de::Uri("Maps:", RC_NULL));
        if(gx.FinalizeMapChange)
        {
            gx.FinalizeMapChange(reinterpret_cast<uri_s const *>(&mapUri));
        }

        if(gameTime > 20000000 / TICSPERSEC)
        {
            // In very long-running games, gameTime will become so large that
            // it cannot be accurately converted to 35 Hz integer tics. Thus it
            // needs to be reset back to zero.
            gameTime = 0;
        }

        // Init player values.
        DoomsdayApp::players().forAll([] (Player &plr)
        {
            ddplayer_t &ddpl = plr.publicData();

            plr.extraLight        = 0;
            plr.targetExtraLight  = 0;
            plr.extraLightCounter = 0;

            // Determine the "invoid" status.
            ddpl.inVoid = true;
            if(mobj_t *mo = ddpl.mo)
            {
                if(SectorCluster *cluster = Mobj_ClusterPtr(*mo))
                {
#ifdef __CLIENT__
                    if(mo->origin[2] >= cluster->visFloor  ().heightSmoothed() &&
                       mo->origin[2] <  cluster->visCeiling().heightSmoothed() - 4)
#else
                    if(mo->origin[2] >= cluster->floor  ().height() &&
                       mo->origin[2] <  cluster->ceiling().height() - 4)
#endif
                    {
                        ddpl.inVoid = false;
                    }
                }
            }

            return LoopContinue;
        });

#ifdef __CLIENT__
        /// @todo Refactor away:
        map->forAllSectors([] (Sector &sector)
        {
            sector.forAllSides([] (LineSide &side)
            {
                side.fixMissingMaterials();
                return LoopContinue;
            });
            return LoopContinue;
        });
#endif

        map->initPolyobjs();

#ifdef __CLIENT__
        ClientApp::audioSystem().worldMapChanged();
#endif

#ifdef __SERVER__
        if(::isServer)
        {
            // Init server data.
            Sv_InitPools();
        }
#endif

#ifdef __CLIENT__
        GL_SetupFogFromMapInfo(mapInfo.accessedRecordPtr());

        map->initLightGrid();
        map->initSkyFix();
        map->buildMaterialLists();
        map->spawnPlaneParticleGens();

        // Precaching from 100 to 200.
        Con_SetProgress(100);
        Time begunPrecacheAt;
        // Sky models usually have big skins.
        rendSys().sky().cacheAssets();
        resSys().cacheForCurrentMap();
        resSys().processCacheQueue();
        LOG_RES_VERBOSE("Precaching completed in %.2f seconds") << begunPrecacheAt.since();

        rendSys().clearDrawLists();
        R_InitRendPolyPools();
        Rend_UpdateLightModMatrix();

        map->initRadio();
        map->initContactBlockmaps();
        R_InitContactLists(*map);
        rendSys().worldSystemMapChanged(*map);
        map->initBias();  // Shadow bias sources and surfaces.

        // Rewind/restart material animators.
        /// @todo Only rewind animators responsible for map-surface contexts.
        resSys().forAllMaterials([] (Material &material)
        {
            return material.forAllAnimators([] (MaterialAnimator &animator)
            {
                animator.rewind();
                return LoopContinue;
            });
        });
#endif

        /*
         * Post-change map setup has now been fully completed.
         */

        // Run any commands specified in MapInfo.
        String execute = mapInfo.gets("execute");
        if(!execute.isEmpty())
        {
            Con_Execute(CMDS_SCRIPT, execute.toUtf8().constData(), true, false);
        }

        // Run the special map setup command, which the user may alias to do
        // something useful.
        if(!mapUri.isEmpty())
        {
            String cmd = String("init-") + mapUri.path();
            if(Con_IsValidCommand(cmd.toUtf8().constData()))
            {
                Con_Executef(CMDS_SCRIPT, false, "%s", cmd.toUtf8().constData());
            }
        }

        // Reset world time.
        time = 0;

        // Now that the setup is done, let's reset the timer so that it will
        // appear that no time has passed during the setup.
        DD_ResetTimer();

#ifdef __CLIENT__
        // Make sure that the next frame doesn't use a filtered viewer.
        R_ResetViewer();

        // Clear any input events that might have accumulated during setup.
        ClientApp::inputSystem().clearEvents();

        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;
#endif

        Z_PrintStatus();

        // Inform interested parties that the "current" map has changed.
        self.notifyMapChange();
    }

#ifdef __CLIENT__
    void updateHandOrigin()
    {
        DENG2_ASSERT(hand != nullptr && map != nullptr);

        viewdata_t const *viewData = &::viewPlayer->viewport();
        hand->setOrigin(viewData->current.origin + viewData->frontVec.xzy() * handDistance);
    }

    DENG2_PIMPL_AUDIENCE(FrameBegin)
    DENG2_PIMPL_AUDIENCE(FrameEnd)
#endif
};

#ifdef __CLIENT__
DENG2_AUDIENCE_METHOD(WorldSystem, FrameBegin)
DENG2_AUDIENCE_METHOD(WorldSystem, FrameEnd)
#endif

WorldSystem::WorldSystem()
    : world::System()
    , d(new Instance(this))
{}

bool WorldSystem::hasMap() const
{
    return d->map != nullptr;
}

Map &WorldSystem::map() const
{
    if(d->map) return *d->map;
    /// @throw MapError Attempted with no map loaded.
    throw MapError("world::System::map", "No map is currently loaded");
}

/// @todo Split this into subtasks (load, make current, cache assets).
bool WorldSystem::changeMap(de::Uri const &mapUri)
{
    LOG_AS("world::System");
    res::MapManifest const *mapManifest = resSys().tryFindMapManifest(mapUri);

#ifdef __SERVER__
    // Whenever the map changes, remote players must tell us when they're
    // ready to begin receiving frames.
    for(dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(DD_Player(i)->isConnected())
        {
            LOG_DEBUG("Client %i marked as 'not ready' to receive frames.") << i;
            DD_Player(i)->ready = false;
        }
    }
#endif

    // Initialize the logical sound manager.
#ifdef __SERVER__
    ServerApp::app().clearAllLogicalSounds();
#else
    ClientApp::audioSystem().resetStage(::audio::WorldStage);

    App_ResourceSystem().purgeCacheQueue();

    if(d->map)
    {
        /// Remove the current map from our audiences. @todo Map should handle this. -ds
        audienceForFrameBegin() -= d->map;

        // Map objects are about to be destroyed.
        /// - Stop all channels using one as emitter.
        ClientApp::audioSystem().mixer()["fx"].forAllChannels([this] (::audio::Channel &base)
        {
            auto &ch = base.as<::audio::SoundChannel>();

            if(ch.emitter() && &Thinker_Map(ch.emitter()->thinker) == d->map)
            {
                // This channel must be stopped!
                ch.stop();
                ch.setEmitter(nullptr);
            }
            return LoopContinue;
        });
        /// - Instruct the Listener to forget the map-object being tracked.
        /// @todo Should observe MapObject deletion. -ds
        ClientApp::audioSystem().worldStage().listener().setTrackedMapObject(nullptr);
    }

    // As the memory zone does not provide the mechanisms to prepare another map in parallel
    // we must free the current map first.
    /// @todo The memory zone would still be useful if the purge and tagging mechanisms
    /// allowed more fine grained control. It is no longer useful for allocating memory
    /// used elsewhere so it should be repurposed for this usage specifically.
    R_DestroyContactLists();
#endif
    delete d->map; d->map = nullptr;
    Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);

    // Are we just unloading the current map?
    if(!mapManifest) return true;

    LOG_MSG("Loading map \"%s\"...") << mapManifest->composeUri().path();

    // A new map is about to be set up.
    ::ddMapSetup = true;

    // Attempt to load in the new map.
    MapConversionReporter reporter;
    Map *newMap = d->loadMap(*mapManifest, &reporter);
    if(newMap)
    {
        // The map may still be in an editable state -- switch to playable.
        bool const mapIsPlayable = newMap->endEditing();

        // Cancel further reports about the map.
        reporter.setMap(nullptr);

        if(!mapIsPlayable)
        {
            // Darn. Discard the useless data.
            delete newMap; newMap = nullptr;
        }
    }

    // This becomes the new current map.
    d->makeCurrent(newMap);

    // We've finished setting up the map.
    ::ddMapSetup = false;

    // Output a human-readable report of any issues encountered during conversion.
    reporter.writeLog();

    return d->map != nullptr;
}

void WorldSystem::reset()
{
    DoomsdayApp::players().forAll([] (Player &plr)
    {
        ddplayer_t &ddpl = plr.publicData();

        // Mobjs go down with the map.
        ddpl.mo            = nullptr;
        ddpl.extraLight    = 0;
        ddpl.fixedColorMap = 0;
        //ddpl.inGame        = false;
        ddpl.flags         &= ~DDPF_CAMERA;

        // States have changed, the state pointers are unknown.
        for(ddpsprite_t &pspr : ddpl.pSprites)
        {
            pspr.statePtr = nullptr;
        }

        return LoopContinue;
    });

#ifdef __CLIENT__
    if(isClient)
    {
        Cl_ResetFrame();
        Cl_InitPlayers();
    }
#endif

    // If a map is currently loaded -- unload it.
    unloadMap();
}

void WorldSystem::update()
{
    DoomsdayApp::players().forAll([] (Player &plr)
    {
        // States have changed, the state pointers are unknown.
        for(ddpsprite_t &pspr : plr.publicData().pSprites)
        {
            pspr.statePtr = nullptr;
        }
        return LoopContinue;
    });

    // Update the current map, also.
    if(d->map)
    {
        d->map->update();
    }
}

Record const &WorldSystem::mapInfoForMapUri(de::Uri const &mapUri) const
{
    // Is there a MapInfo definition for the given URI?
    if(Record const *def = ::defs.mapInfos.tryFind("id", mapUri.compose()))
    {
        return *def;
    }
    // Is there is a default definition (for all maps)?
    if(Record const *def = ::defs.mapInfos.tryFind("id", de::Uri("Maps", Path("*")).compose()))
    {
        return *def;
    }
    // Use the fallback.
    return d->fallbackMapInfo;
}

void WorldSystem::advanceTime(timespan_t delta)
{
#ifdef __CLIENT__
    if(::clientPaused) return;
#endif
    d->time += delta;
}

timespan_t WorldSystem::time() const
{
    return d->time;
}

void WorldSystem::tick(timespan_t elapsed)
{
#ifdef __CLIENT__
    if(d->map)
    {
        d->map->skyAnimator().advanceTime(elapsed);

        if(DD_IsSharpTick())
        {
            d->map->thinkers().forAll(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1, [] (thinker_t *th)
            {
                Mobj_AnimateHaloOcclussion(*reinterpret_cast<mobj_t *>(th));
                return LoopContinue;
            });
        }
    }
#else
    DENG2_UNUSED(elapsed);
#endif
}

#ifdef __CLIENT__
Hand &WorldSystem::hand(coord_t *distance) const
{
    // Time to create the hand?
    if(!d->hand)
    {
        d->hand.reset(new Hand());
        audienceForFrameEnd() += *d->hand;
        if(d->map)
        {
            d->updateHandOrigin();
        }
    }
    if(distance)
    {
        *distance = handDistance;
    }
    return *d->hand;
}

void WorldSystem::beginFrame(bool resetNextViewer)
{
    // Notify interested parties that a new frame has begun.
    DENG2_FOR_AUDIENCE2(FrameBegin, i) i->worldSystemFrameBegins(resetNextViewer);
}

void WorldSystem::endFrame()
{
    if(d->map && d->hand)
    {
        d->updateHandOrigin();

        // If the HueCircle is active update the current edit color.
        if(HueCircle *hueCircle = SBE_HueCircle())
        {
            viewdata_t const *viewData = &viewPlayer->viewport();
            d->hand->setEditColor(hueCircle->colorAt(viewData->frontVec));
        }
    }

    // Notify interested parties that the current frame has ended.
    DENG2_FOR_AUDIENCE2(FrameEnd, i) i->worldSystemFrameEnds();
}

#endif  // __CLIENT__

void WorldSystem::consoleRegister()  // static
{
    //C_VAR_BYTE ("map-cache", &mapCache, 0, 0, 1);
#ifdef __CLIENT__
    C_VAR_FLOAT("edit-bias-grab-distance", &handDistance, 0, 10, 1000);
#endif
    Map::consoleRegister();
}
