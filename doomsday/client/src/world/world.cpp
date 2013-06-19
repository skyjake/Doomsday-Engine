/** @file world.cpp World.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <map>
#include <utility>

#include <QMap>
#include <QtAlgorithms>

#include <de/memoryzone.h>

#include <de/Error>
#include <de/Log>
#include <de/Time>

#include "de_base.h"
#include "de_console.h"
#include "de_defs.h"
#include "de_play.h"
#include "de_filesys.h"

#include "Materials"

#include "Plane"
#include "Sector"

#include "audio/s_main.h"
#include "edit_map.h"
#include "network/net_main.h"

#include "render/r_main.h" // R_ResetViewer

#ifdef __CLIENT__
#  include "client/cl_frame.h"
#  include "client/cl_player.h"

#  include "render/lumobj.h"
#  include "render/r_shadow.h"
#  include "render/rend_bias.h"
#  include "render/rend_decor.h"
#  include "render/rend_fakeradio.h"
#  include "render/rend_list.h"
#  include "render/rend_main.h"
#  include "render/sky.h"
#  include "render/vlight.h"
#endif

#ifdef __SERVER__
#  include "server/sv_pool.h"
#endif

#include "world/thinkers.h"

#include "world/world.h"

using namespace de;

/**
 * Observes the progress of a map conversion and records any issues/problems that
 * are encountered in the process. When asked, compiles a human-readable report
 * intended to assist mod authors in debugging their maps.
 *
 * @todo Consolidate with the missing material reporting done elsewhere -ds
 */
class MapConversionReporter :
DENG2_OBSERVES(Map, UnclosedSectorFound),
DENG2_OBSERVES(Map, OneWayWindowFound)
{
    /// Record "unclosed sectors".
    /// Sector => world point relatively near to the problem area.
    typedef std::map<Sector *,  Vector2d> UnclosedSectorMap;

    /// Record "one-way window lines".
    /// Line => Sector the back side faces.
    typedef std::map<Line *,  Sector *> OneWayWindowMap;

    /// Maximum number of warnings to output (of each type) about any problems
    /// encountered during the build process.
    static int const maxWarningsPerType = 10;

public:
    MapConversionReporter() {}

    inline int unclosedSectorCount() const { return (int)_unclosedSectors.size(); }
    inline int oneWayWindowCount() const { return (int)_oneWayWindows.size(); }

    void writeLog()
    {
        if(int numToLog = maxWarnings(unclosedSectorCount()))
        {
            UnclosedSectorMap::const_iterator it = _unclosedSectors.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
            {
                LOG_WARNING("Sector #%d is unclosed near %s.")
                    << it->first->indexInMap() << it->second.asText();
            }

            if(numToLog < unclosedSectorCount())
                LOG_INFO("(%d more like this)") << (unclosedSectorCount() - numToLog);
        }

        if(int numToLog = maxWarnings(oneWayWindowCount()))
        {
            OneWayWindowMap::const_iterator it = _oneWayWindows.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
            {
                LOG_VERBOSE("Line #%d seems to be a One-Way Window (back faces sector #%d).")
                    << it->first->indexInMap() << it->second->indexInMap();
            }

            if(numToLog < oneWayWindowCount())
                LOG_INFO("(%d more like this)") << (oneWayWindowCount() - numToLog);
        }
    }

protected:
    // Observes Partitioner UnclosedSectorFound.
    void unclosedSectorFound(Sector &sector, Vector2d const &nearPoint)
    {
        _unclosedSectors.insert(std::make_pair(&sector, nearPoint));
    }

    // Observes Partitioner OneWayWindowFound.
    void oneWayWindowFound(Line &line, Sector &backFacingSector)
    {
        _oneWayWindows.insert(std::make_pair(&line, &backFacingSector));
    }

private:
    static inline int maxWarnings(int issueCount)
    {
#ifdef DENG_DEBUG
        return issueCount; // No limit.
#else
        return de::min(issueCount, maxWarningsPerType);
#endif
    }

    UnclosedSectorMap _unclosedSectors;
    OneWayWindowMap   _oneWayWindows;
};

boolean ddMapSetup;
timespan_t ddMapTime;

// Should we be caching successfully loaded maps?
//static byte mapCache = true; // cvar

static char const *mapCacheDir = "mapcache/";

static inline lumpnum_t markerLumpNumForPath(String path)
{
    return App_FileSystem().lumpNumForName(path);
}

static String composeUniqueMapId(de::File1 &markerLump)
{
    return String("%1|%2|%3|%4")
              .arg(markerLump.name().fileNameWithoutExtension())
              .arg(markerLump.container().name().fileNameWithoutExtension())
              .arg(markerLump.container().hasCustom()? "pwad" : "iwad")
              .arg(Str_Text(App_CurrentGame().identityKey()))
              .toLower();
}

/// Determine the identity key for maps loaded from the specified @a sourcePath.
static String cacheIdForMap(String const &sourcePath)
{
    DENG_ASSERT(!sourcePath.isEmpty());

    ushort id = 0;
    for(int i = 0; i < sourcePath.size(); ++i)
    {
        id ^= sourcePath.at(i).unicode() << ((i * 3) % 11);
    }

    return String("%1").arg(id, 4, 16);
}

namespace de {

DENG2_PIMPL(World)
{
    /**
     * Information about a map in the cache.
     */
    struct CacheRecord
    {
        Uri mapUri;                 ///< Unique identifier for the map.
        //String path;                ///< Path to the cached map data.
        //bool dataAvailable;
        //bool lastLoadAttemptFailed;
    };

    typedef QMap<String, CacheRecord> Records;

    /// Current map.
    Map *map;

    /// Map cache records.
    Records records;

    Instance(Public *i) : Base(i), map(0)
    {}

    void notifyMapChange()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(MapChange, i)
        {
            i->currentMapChanged();
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
        return mapCacheDir + String(Str_Text(App_CurrentGame().identityKey()))
               / sourcePath.fileNameWithoutExtension() + '-' + cacheIdForMap(sourcePath);
    }

    /**
     * Try to locate a cache record for a map by URI.
     *
     * @param uri  Map identifier.
     *
     * @return  Pointer to the found CacheRecord; otherwise @c 0.
     */
    CacheRecord *findCacheRecord(Uri const &uri) const
    {
        Records::const_iterator found = records.find(uri.resolved());
        if(found != records.end())
        {
            return const_cast<CacheRecord *>(&found.value());
        }
        return 0; // Not found.
    }

    /**
     * Create a new CacheRecord for the map. If an existing record is found it
     * will be returned instead (becomes a no-op).
     *
     * @param uri  Map identifier.
     *
     * @return  Possibly newly-created CacheRecord.
     */
    CacheRecord &createCacheRecord(Uri const &uri)
    {
        // Do we have an existing record for this?
        if(CacheRecord *record = findCacheRecord(uri))
            return *record;

        // Prepare a new record.
        CacheRecord rec;
        rec.mapUri = uri;

        // Compose the cache directory path.
        /*lumpnum_t markerLumpNum = App_FileSystem().lumpNumForName(uri.path().toString().toLatin1().constData());
        if(markerLumpNum >= 0)
        {
            File1 &lump = App_FileSystem().nameIndex().lump(markerLumpNum);
            String cacheDir = cachePath(lump.container().composePath());

            rec.path = cacheDir + lump.name() + ".dcm";
        }*/

        return records.insert(uri.resolved(), rec).value();
    }

    /**
     * Attempt JIT conversion of the map data with the help of a plugin.
     *
     * @return  Pointer to the converted Map; otherwise @c 0.
     */
    Map *convertMap(Uri const &uri)
    {
        // Record this map if we haven't already.
        /*CacheRecord &record =*/ createCacheRecord(uri);

        // We require a map converter for this.
        if(!Plug_CheckForHook(HOOK_MAP_CONVERT))
            return 0;

        //LOG_DEBUG("Attempting \"%s\"...") << uri;

        lumpnum_t markerLumpNum = markerLumpNumForPath(uri.path());
        if(markerLumpNum < 0)
            return 0;

        // Initiate the conversion process.
        MPE_Begin(reinterpret_cast<uri_s const *>(&uri));
        Map *newMap = MPE_Map();

        // Configure a reporter to observe the conversion process.
        MapConversionReporter reporter;
        newMap->audienceForOneWayWindowFound   += reporter;
        newMap->audienceForUnclosedSectorFound += reporter;

        // Ask each converter in turn whether the map format is recognizable
        // and if so to interpret and transfer it to us via the runtime map
        // editing interface.
        if(!DD_CallHooks(HOOK_MAP_CONVERT, 0, (void *) reinterpret_cast<uri_s const *>(&uri)))
            return 0;

        // A converter signalled success.

        // End the conversion process (if not already).
        MPE_End();

        // Output a human-readable log of any issues encountered in the process.
        reporter.writeLog();

        // Take ownership of the map.
        newMap = MPE_TakeMap();

        if(!newMap->endEditing())
        {
            // Darn, not usable? Clean up...
            delete newMap;
            return 0;
        }

        // Generate the old unique map id.
        File1 &markerLump       = App_FileSystem().nameIndex().lump(markerLumpNum);
        String uniqueId         = composeUniqueMapId(markerLump);
        QByteArray uniqueIdUtf8 = uniqueId.toUtf8();
        newMap->setOldUniqueId(uniqueIdUtf8.constData());

        // Are we caching this map?
        /*if(mapCache)
        {
            // Ensure the destination directory exists.
            F_MakePath(rec.cachePath.toUtf8().constData());

            // Cache the map!
            DAM_MapWrite(newMap, rec.cachePath);
        }*/

        return newMap;
    }

#if 0
    /**
     * Returns @c true iff data for the map is available in the cache.
     */
    bool isCacheDataAvailable(CacheRecord &rec)
    {
        if(DAM_MapIsValid(Str_Text(&rec.path), markerLumpNum()))
        {
            rec.dataAvailable = true;
        }
        return rec.dataAvailable;
    }

    /**
     * Attempt to load data for the map from the cache.
     *
     * @see isCachedDataAvailable()
     *
     * @return  Pointer to the loaded Map; otherwise @c 0.
     */
    Map *loadMapFromCache(Uri const &uri)
    {
        // Record this map if we haven't already.
        CacheRecord &rec = createCacheRecord(uri);

        Map *map = DAM_MapRead(Str_Text(&rec.path));
        if(!map) return 0;

        map->_uri = rec.mapUri;
        return map;
    }
#endif

    /**
     * Attempt to load the associated map data.
     *
     * @return  Pointer to the loaded map; otherwise @c 0.
     */
    Map *loadMap(Uri const &uri/*, bool forceRetry = false*/)
    {
        LOG_MSG("Loading map \"%s\"...") << uri;
        LOG_AS("World::loadMap");

        // Record this map if we haven't already.
        /*CacheRecord &rec =*/ createCacheRecord(uri);

        //if(rec.lastLoadAttemptFailed && !forceRetry)
        //    return false;

        //rec.lastLoadAttemptFailed = false;

        // Load from cache?
        /*if(mapCache && rec.dataAvailable)
        {
            if(Map *map = loadMapFromCache(uri))
                return map;

            rec.lastLoadAttemptFailed = true;
            return 0;
        }*/

        // Try a JIT conversion with the help of a plugin.
        if(Map *map = convertMap(uri))
            return map;

        LOG_WARNING("Failed conversion of \"%s\".") << uri;
        //rec.lastLoadAttemptFailed = true;
        return 0;
    }

    /**
     * Replace the current map with @a map.
     */
    void changeMap(Map *newMap)
    {
        // This is now the current map (if any).
        map = newMap;

        if(!map) return;

#define TABBED(A, B) "\n" _E(Ta) "  " << A << " " _E(Tb) << B

        LOG_INFO(_E(D) "Current map elements:" _E(.))
                << TABBED("Vertexes",  map->vertexCount())
                << TABBED("Lines",     map->lineCount())
                << TABBED("Sectors",   map->sectorCount())
                << TABBED("BSP Nodes", map->bspNodeCount())
                << TABBED("BSP Leafs", map->bspLeafCount())
                << TABBED("Segments",  map->segmentCount());

        // See what MapInfo says about this map.
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s const *>(&map->uri()));
        if(!mapInfo)
        {
            // Use the default def instead.
            Uri defaultMapUri("*", RC_NULL);
            mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&defaultMapUri));
        }

        if(mapInfo)
        {
            map->_globalGravity = mapInfo->gravity;
            map->_ambientLightLevel = mapInfo->ambient * 255;
        }
        else
        {
            // No map info found -- apply defaults.
            map->_globalGravity = 1.0f;
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
        Sky_Configure(skyDef);
#endif

        // Init the thinker lists (public and private).
        map->thinkers().initLists(0x1 | 0x2);

        // Must be called before we go any further.
        P_InitUnusedMobjList();

        // Must be called before any mobjs are spawned.
        map->initNodePiles();

#ifdef __CLIENT__
        // Prepare the client-side data.
        if(isClient)
        {
            map->initClMobjs();
        }
        Cl_ResetFrame();
        map->reinitClMobjs();
        Cl_InitPlayers(); // Player data, too.

        // Spawn generators for the map.
        /// @todo Defer until after finalization.
        P_PtcInitForMap();
#endif

        // The game may need to perform it's own finalization now that the
        // "current" map has changed.
        if(gx.FinalizeMapChange)
        {
            gx.FinalizeMapChange(reinterpret_cast<uri_s const *>(&map->uri()));
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
                BspLeaf &bspLeaf = map->bspLeafAt(mo->origin);
#ifdef __CLIENT__
                if(mo->origin[VZ] >= bspLeaf.sector().floor().visHeight() &&
                   mo->origin[VZ] <  bspLeaf.sector().ceiling().visHeight() - 4)
#else
                if(mo->origin[VZ] >= bspLeaf.sector().floor().height() &&
                   mo->origin[VZ] <  bspLeaf.sector().ceiling().height() - 4)
#endif
                {
                    ddpl.inVoid = false;
                }
            }
        }

        /// @todo Refactor away:
        foreach(Sector *sector, map->sectors())
        {
            sector->updateSoundEmitterOrigin();
#ifdef __CLIENT__
            map->updateMissingMaterialsForLinesOfSector(*sector);
            S_MarkSectorReverbDirty(sector);
#endif
        }

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
        map->buildSurfaceLists();
        P_MapSpawnPlaneParticleGens();

        Time begunPrecacheAt;
        Rend_CacheForMap();
        App_Materials().processCacheQueue();
        LOG_INFO(String("Precaching completed in %1 seconds.").arg(begunPrecacheAt.since(), 0, 'g', 2));

        RL_DeleteLists();
        R_InitRendPolyPools();

        Rend_UpdateLightModMatrix();
        Rend_DecorInitForMap();
        Rend_RadioInitForMap();

        R_InitObjlinkBlockmapForMap();
        R_InitShadowProjectionListsForMap(); // Projected mobj shadows.
        LO_InitForMap(); // Lumobj management.
        VL_InitForMap(); // Converted vlights (from lumobjs) management.

        // Tell shadow bias to initialize the bias light sources.
        SB_InitForMap(map->oldUniqueId());

        // Restart all material animations.
        App_Materials().restartAllAnimations();
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
        String cmd = "init-" + map->uri().resolved();
        if(Con_IsValidCommand(cmd.toUtf8().constData()))
        {
            Con_Executef(CMDS_SCRIPT, false, "%s", cmd.toUtf8().constData());
        }

        // Reset map time.
        ddMapTime = 0;

        // Now that the setup is done, let's reset the timer so that it will
        // appear that no time has passed during the setup.
        DD_ResetTimer();

        // Make sure that the next frame doesn't use a filtered viewer.
        R_ResetViewer();

#ifdef __CLIENT__
        // Clear any input events that might have accumulated during setup.
        DD_ClearEvents();

        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;
#endif

        Z_PrintStatus();

        // Inform interested parties that the "current" map has changed.
        notifyMapChange();
    }
};

World::World() : d(new Instance(this))
{}

void World::consoleRegister() // static
{
    //C_VAR_BYTE("map-cache", &mapCache, 0, 0, 1);
    Map::consoleRegister();
}

bool World::hasMap() const
{
    return d->map != 0;
}

Map &World::map() const
{
    if(d->map)
    {
        return *d->map;
    }
    /// @throw MapError Attempted with no map loaded.
    throw MapError("World::map", "No map is currently loaded");
}

bool World::changeMap(de::Uri const &uri)
{
    // As the memory zone does not provide the mechanisms to prepare another
    // map in parallel we must free the current map first.
    /// @todo The memory zone would still be useful if the purge and tagging
    /// mechanisms allowed more fine grained control. It is no longer useful
    /// for allocating memory used elsewhere so it should be repurposed for
    /// this usage specifically.
    delete d->map; d->map = 0;
    Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);

    // Are we just unloading the current map?
    if(uri.isEmpty()) return true;

    // A new map is about to be setup.
    ddMapSetup = true;

    d->changeMap(d->loadMap(uri));

    // We've finished setting up the map.
    ddMapSetup = false;

    return d->map != 0;
}

void World::reset()
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

void World::update()
{
#ifdef __CLIENT__
    P_UpdateParticleGens(); // Defs might've changed.
#endif

    // Reset the archived map cache (the available maps may have changed).
    d->records.clear();

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

} // namespace de
