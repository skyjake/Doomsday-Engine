/** @file worldsystem.cpp  World subsystem.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "world/worldsystem.h"

#include "de_defs.h"
#include "de_play.h"
#include "de_filesys.h"
#include "dd_main.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "ui/progress.h"

#include "audio/s_main.h"
#include "edit_map.h"
#include "network/net_main.h"

#include "Plane"
#include "Sector"
#include "SectorCluster"

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "client/cl_def.h"
#  include "client/cl_frame.h"
#  include "client/cl_player.h"
#  include "edit_bias.h"
#  include "Hand"
#  include "HueCircle"

#  include "Lumobj"
#  include "render/viewports.h" // R_ResetViewer
#  include "render/projector.h"
#  include "render/rend_fakeradio.h"
#  include "render/rend_main.h"
#  include "render/sky.h"
#  include "render/vlight.h"
#endif

#ifdef __SERVER__
#  include "server/sv_pool.h"
#endif

#include "world/thinkers.h"

#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <doomsday/console/exec.h>
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

int validCount = 1; // Increment every time a check is made.

#ifdef __CLIENT__
static float handDistance = 300; //cvar
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
, DENG2_OBSERVES(Map, Deletion)
{
    /// Record "unclosed sectors".
    /// Sector index => world point relatively near to the problem area.
    typedef std::map<int, Vector2i> UnclosedSectorMap;

    /// Record "one-way window lines".
    /// Line index => Sector index the back side faces.
    typedef std::map<int, int> OneWayWindowMap;

    /// Maximum number of warnings to output (of each type) about any problems
    /// encountered during the build process.
    static int const maxWarningsPerType;

public:
    /**
     * Construct a new conversion reporter.
     * @param map
     */
    MapConversionReporter(Map *map = 0) : _map(0)
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

    /// Same as @code setMap(0); @endcode
    inline void clearMap() { setMap(0); }

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
        if(int numToLog = maxWarnings(unclosedSectorCount()))
        {
            String str;

            UnclosedSectorMap::const_iterator it = _unclosedSectors.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
            {
                if(i != 0) str += "\n";
                str += String("Sector #%1 is unclosed near %2")
                           .arg(it->first).arg(it->second.asText());
            }

            if(numToLog < unclosedSectorCount())
                str += String("\n(%1 more like this)").arg(unclosedSectorCount() - numToLog);

            LOG_MAP_WARNING("%s") << str;
        }

        if(int numToLog = maxWarnings(oneWayWindowCount()))
        {
            String str;

            OneWayWindowMap::const_iterator it = _oneWayWindows.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
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
    void mapBeingDeleted(Map const &map)
    {
        DENG2_ASSERT(&map == _map); // sanity check.
        DENG2_UNUSED(map);
        _map = 0;
    }

private:
    inline int unclosedSectorCount() const { return int( _unclosedSectors.size() ); }
    inline int oneWayWindowCount() const   { return int( _oneWayWindows.size() ); }

    static inline int maxWarnings(int issueCount)
    {
#ifdef DENG_DEBUG
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
            _map->audienceForDeletion            += this;
            _map->audienceForOneWayWindowFound   += this;
            _map->audienceForUnclosedSectorFound += this;
        }
        else
        {
            _map->audienceForDeletion            -= this;
            _map->audienceForOneWayWindowFound   -= this;
            _map->audienceForUnclosedSectorFound -= this;
        }
    }

    Map *_map; ///< Map currently being reported on, if any (not owned).
    UnclosedSectorMap _unclosedSectors;
    OneWayWindowMap   _oneWayWindows;
};

int const MapConversionReporter::maxWarningsPerType = 10;

dd_bool ddMapSetup;

// Should we be caching successfully loaded maps?
//static byte mapCache = true; // cvar

static char const *mapCacheDir = "mapcache/";

/// Determine the identity key for maps loaded from the specified @a sourcePath.
static String cacheIdForMap(String const &sourcePath)
{
    DENG2_ASSERT(!sourcePath.isEmpty());

    ushort id = 0;
    for(int i = 0; i < sourcePath.size(); ++i)
    {
        id ^= sourcePath.at(i).unicode() << ((i * 3) % 11);
    }

    return String("%1").arg(id, 4, 16);
}

namespace de {

DENG2_PIMPL(WorldSystem)
{
    Map *map;                   ///< Current map.
    timespan_t time;            ///< World-wide time.
#ifdef __CLIENT__
    QScopedPointer<Hand> hand;  ///< For map editing/manipulation.
#endif

    Instance(Public *i) : Base(i), map(0), time(0) {}

    void notifyMapChange()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(MapChange, i)
        {
            i->worldSystemMapChanged();
        }
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
    Map *convertMap(MapDef const &mapDef, MapConversionReporter *reporter = 0)
    {
        // We require a map converter for this.
        if(!Plug_CheckForHook(HOOK_MAP_CONVERT))
            return 0;

        LOG_DEBUG("Attempting \"%s\"...") << mapDef.composeUri().path();

        if(!mapDef.sourceFile()) return 0;

        // Initiate the conversion process.
        MPE_Begin(0/*dummy*/);

        Map *newMap = MPE_Map();

        // Associate the map with its corresponding definition.
        newMap->setDef(&const_cast<MapDef &>(mapDef));

        if(reporter)
        {
            // Instruct the reporter to begin observing the conversion.
            reporter->setMap(newMap);
        }

        // Ask each converter in turn whether the map format is recognizable
        // and if so to interpret and transfer it to us via the runtime map
        // editing interface.
        if(!DD_CallHooks(HOOK_MAP_CONVERT, 0, const_cast<Id1MapRecognizer *>(&mapDef.recognizer())))
            return 0;

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
    bool haveCachedMap(MapDef &mapDef)
    {
        // Disabled?
        if(!mapCache) return false;
        return DAM_MapIsValid(mapDef.cachePath, mapDef.id());
    }

    /**
     * Attempt to load data for the map from the cache.
     *
     * @see isCachedDataAvailable()
     *
     * @return @c true if loading completed successfully.
     */
    Map *loadMapFromCache(MapDef &mapDef)
    {
        Uri const mapUri = mapDef.composeUri();
        Map *map = DAM_MapRead(mapDef.cachePath);
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
    Map *loadMap(MapDef &mapDef, MapConversionReporter *reporter = 0)
    {
        LOG_AS("WorldSystem::loadMap");

        /*if(mapDef.lastLoadAttemptFailed && !forceRetry)
            return 0;

        // Load from cache?
        if(haveCachedMap(mapDef))
        {
            return loadMapFromCache(mapDef);
        }*/

        // Try a JIT conversion with the help of a plugin.
        Map *map = convertMap(mapDef, reporter);
        if(!map)
        {
            LOG_WARNING("Failed conversion of \"%s\".") << mapDef.composeUri().path();
            //mapDef.lastLoadAttemptFailed = true;
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
        self.audienceForFrameBegin += map;
#endif

        // Print summary information about this map.
        LOG_MAP_NOTE(_E(b) "Current map elements:");
        LOG_MAP_NOTE("%s") << map->elementSummaryAsStyledText();

        // See what MapInfo says about this map.
        ded_mapinfo_t *mapInfo = 0;

        if(MapDef *mapDef = map->def())
        {
            Uri const mapUri = mapDef->composeUri();
            mapInfo = defs.getMapInfo(&mapUri);
        }

        if(!mapInfo)
        {
            // Use the default def instead.
            Uri const defaultMapUri("Maps", Path("*"));
            mapInfo = defs.getMapInfo(&defaultMapUri);
        }

        if(mapInfo)
        {
            map->_globalGravity     = mapInfo->gravity;
            map->_ambientLightLevel = mapInfo->ambient * 255;
        }
        else
        {
            // No map info found -- apply defaults.
            map->_globalGravity     = 1.0f;
            map->_ambientLightLevel = 0;
        }

        map->_effectiveGravity = map->_globalGravity;

#ifdef __CLIENT__
        // Reconfigure the sky.
        ded_sky_t *skyDef = 0;
        if(mapInfo)
        {
            skyDef = Def_GetSky(mapInfo->skyID);
            if(!skyDef) skyDef = &mapInfo->sky;
        }
        theSky->configure(skyDef);
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
        Cl_InitPlayers(); // Player data, too.

        /// @todo Defer initial generator spawn until after finalization.
        map->initGenerators();
#endif

        // The game may need to perform it's own finalization now that the
        // "current" map has changed.
        if(gx.FinalizeMapChange)
        {
            de::Uri mapUri("Maps:", RC_NULL);
            if(map->def()) mapUri = map->def()->composeUri();
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
        for(uint i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];
            ddplayer_t &ddpl = ddPlayers[i].shared;

            plr->extraLight = plr->targetExtraLight = 0;
            plr->extraLightCounter = 0;

            // Determine the "invoid" status.
            ddpl.inVoid = true;

            if(mobj_t *mo = ddpl.mo)
            {
                if(SectorCluster *cluster = Mobj_ClusterPtr(*mo))
                {
#ifdef __CLIENT__
                    if(mo->origin[VZ] >= cluster->visFloor().heightSmoothed() &&
                       mo->origin[VZ] <  cluster->visCeiling().heightSmoothed() - 4)
#else
                    if(mo->origin[VZ] >= cluster->floor().height() &&
                       mo->origin[VZ] <  cluster->ceiling().height() - 4)
#endif
                    {
                        ddpl.inVoid = false;
                    }
                }
            }
        }

#ifdef __CLIENT__
        /// @todo Refactor away:
        foreach(Sector *sector, map->sectors())
        foreach(LineSide *side, sector->sides())
        {
            side->fixMissingMaterials();
        }
#endif

        map->initPolyobjs();
        S_SetupForChangedMap();

#ifdef __SERVER__
        if(isServer)
        {
            // Init server data.
            Sv_InitPools();
        }
#endif

#ifdef __CLIENT__
        map->initLightGrid();
        map->initSkyFix();
        map->buildMaterialLists();
        map->spawnPlaneParticleGens();

        // Precaching from 100 to 200.
        Con_SetProgress(100);
        Time begunPrecacheAt;
        App_ResourceSystem().cacheForCurrentMap();
        App_ResourceSystem().processCacheQueue();
        LOG_RES_VERBOSE("Precaching completed in %.2f seconds") << begunPrecacheAt.since();

        ClientApp::renderSystem().clearDrawLists();
        R_InitRendPolyPools();
        Rend_UpdateLightModMatrix();

        Rend_RadioInitForMap(*map);

        map->initContactBlockmaps();
        R_InitContactLists(*map);
        Rend_ProjectorInitForMap(*map);
        VL_InitForMap(*map); // Converted vlights (from lumobjs).
        map->initBias(); // Shadow bias sources and surfaces.

        // Restart all material animations.
        App_ResourceSystem().restartAllMaterialAnimations();
#endif

        /*
         * Post-change map setup has now been fully completed.
         */

        // Run any commands specified in MapInfo.
        if(mapInfo && mapInfo->execute)
        {
            Con_Execute(CMDS_SCRIPT, mapInfo->execute, true, false);
        }

        // Run the special map setup command, which the user may alias to do
        // something useful.
        if(MapDef *mapDef = map->def())
        {
            String cmd = String("init-") + mapDef->composeUri().path();
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
        DD_ClearEvents();

        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;
#endif

        Z_PrintStatus();

        // Inform interested parties that the "current" map has changed.
        notifyMapChange();
    }

    /// @todo Split this into subtasks (load, make current, cache assets).
    bool changeMap(MapDef *mapDef = 0)
    {
#ifdef __CLIENT__
        if(map)
        {
            // Remove the current map from our audiences.
            /// @todo Map should handle this.
            self.audienceForFrameBegin -= map;
        }
#endif

        // As the memory zone does not provide the mechanisms to prepare another
        // map in parallel we must free the current map first.
        /// @todo The memory zone would still be useful if the purge and tagging
        /// mechanisms allowed more fine grained control. It is no longer useful
        /// for allocating memory used elsewhere so it should be repurposed for
        /// this usage specifically.
#ifdef __CLIENT__
        R_DestroyContactLists();
#endif
        delete map; map = 0;
        Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);

        // Are we just unloading the current map?
        if(!mapDef) return true;

        LOG_MSG("Loading map \"%s\"...") << mapDef->composeUri().path();

        // A new map is about to be set up.
        ddMapSetup = true;

        // Attempt to load in the new map.
        MapConversionReporter reporter;
        Map *newMap = loadMap(*mapDef, &reporter);
        if(newMap)
        {
            // The map may still be in an editable state -- switch to playable.
            bool mapIsPlayable = newMap->endEditing();

            // Cancel further reports about the map.
            reporter.setMap(0);

            if(!mapIsPlayable)
            {
                // Darn. Discard the useless data.
                delete newMap;
                newMap = 0;
            }
        }

        // This becomes the new current map.
        makeCurrent(newMap);

        // We've finished setting up the map.
        ddMapSetup = false;

        // Output a human-readable report of any issues encountered during conversion.
        reporter.writeLog();

        return map != 0;
    }

    struct changemapworker_params_t
    {
        Instance *inst;
        MapDef *mapDef;
    };

    static int changeMapWorker(void *context)
    {
        changemapworker_params_t &p = *static_cast<changemapworker_params_t *>(context);
        int result = p.inst->changeMap(p.mapDef);
        BusyMode_WorkerEnd();
        return result;
    }

#ifdef __CLIENT__
    void updateHandOrigin()
    {
        DENG2_ASSERT(hand != 0 && map != 0);

        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
        hand->setOrigin(viewData->current.origin + viewData->frontVec.xzy() * handDistance);
    }
#endif
};

WorldSystem::WorldSystem() : d(new Instance(this))
{}

void WorldSystem::timeChanged(Clock const &)
{
    // Nothing to do.
}

bool WorldSystem::hasMap() const
{
    return d->map != 0;
}

Map &WorldSystem::map() const
{
    if(d->map)
    {
        return *d->map;
    }
    /// @throw MapError Attempted with no map loaded.
    throw MapError("WorldSystem::map", "No map is currently loaded");
}

bool WorldSystem::changeMap(de::Uri const &mapUri)
{
    MapDef *mapDef = 0;

    if(!mapUri.path().isEmpty())
    {
        mapDef = App_ResourceSystem().mapDef(mapUri);
    }

    // Switch to busy mode (if we haven't already) except when simply unloading.
    if(!mapUri.path().isEmpty() && !BusyMode_Active())
    {
        Instance::changemapworker_params_t parm;
        parm.inst   = d;
        parm.mapDef = mapDef;

        BusyTask task; zap(task);
        /// @todo Use progress bar mode and update progress during the setup.
        task.mode       = BUSYF_ACTIVITY | /*BUSYF_PROGRESS_BAR |*/ BUSYF_TRANSITION | (verbose? BUSYF_CONSOLE_OUTPUT : 0);
        task.name       = "Loading map...";
        task.worker     = Instance::changeMapWorker;
        task.workerData = &parm;

        return CPP_BOOL(BusyMode_RunTask(&task));
    }
    else
    {
        return d->changeMap(mapDef);
    }
}

void WorldSystem::reset()
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t *plr = &ddPlayers[i];
        ddplayer_t *ddpl = &plr->shared;

        // Mobjs go down with the map.
        ddpl->mo = 0;
        // States have changed, the state pointers are unknown.
        ddpl->pSprites[0].statePtr = ddpl->pSprites[1].statePtr = 0;

        //ddpl->inGame = false;
        ddpl->flags &= ~DDPF_CAMERA;

        ddpl->fixedColorMap = 0;
        ddpl->extraLight = 0;
    }

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
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t *plr = &ddPlayers[i];
        ddplayer_t *ddpl = &plr->shared;

        // States have changed, the state pointers are unknown.
        ddpl->pSprites[0].statePtr = ddpl->pSprites[1].statePtr = 0;
    }

    // Update the current map too.
    if(d->map)
    {
        d->map->update();
    }
}

void WorldSystem::advanceTime(timespan_t delta)
{
#ifdef __CLIENT__
    if(clientPaused) return;
#endif
    d->time += delta;
}

timespan_t WorldSystem::time() const
{
    return d->time;
}

#ifdef __CLIENT__
Hand &WorldSystem::hand(coord_t *distance) const
{
    // Time to create the hand?
    if(d->hand.isNull())
    {
        d->hand.reset(new Hand());
        audienceForFrameEnd += *d->hand;
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
    DENG2_FOR_AUDIENCE(FrameBegin, i) i->worldSystemFrameBegins(resetNextViewer);
}

void WorldSystem::endFrame()
{
    if(d->map && !d->hand.isNull())
    {
        d->updateHandOrigin();

        // If the HueCircle is active update the current edit color.
        if(HueCircle *hueCircle = SBE_HueCircle())
        {
            viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
            d->hand->setEditColor(hueCircle->colorAt(viewData->frontVec));
        }
    }

    // Notify interested parties that the current frame has ended.
    DENG2_FOR_AUDIENCE(FrameEnd, i) i->worldSystemFrameEnds();
}

bool WorldSystem::isPointInVoid(Vector3d const &pos) const
{
    // Everything is void if there is no map.
    if(!hasMap()) return true;

    SectorCluster const *cluster = map().clusterAt(pos);
    if(!cluster) return true;

    // Check the planes of the cluster.
    if(cluster->visCeiling().surface().hasSkyMaskedMaterial())
    {
        coord_t const skyCeil = cluster->sector().map().skyFixCeiling();
        if(skyCeil < DDMAXFLOAT && pos.z > skyCeil)
            return true;
    }
    else if(pos.z > cluster->visCeiling().heightSmoothed())
    {
        return true;
    }

    if(cluster->visFloor().surface().hasSkyMaskedMaterial())
    {
        coord_t const skyFloor = cluster->sector().map().skyFixFloor();
        if(skyFloor > DDMINFLOAT && pos.z < skyFloor)
            return true;
    }
    else if(pos.z < cluster->visFloor().heightSmoothed())
    {
        return true;
    }

    return false; // Not in the void.
}

#endif // __CLIENT__

void WorldSystem::consoleRegister() // static
{
    //C_VAR_BYTE ("map-cache", &mapCache, 0, 0, 1);
#ifdef __CLIENT__
    C_VAR_FLOAT("edit-bias-grab-distance", &handDistance, 0, 10, 1000);
#endif
    Map::consoleRegister();
}

} // namespace de
