/** @file world/world.cpp World.
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

#include <QMap>
#include <QtAlgorithms>

#include <de/memoryzone.h>
#include <de/timer.h>

#include <de/Error>
#include <de/Log>

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

#include "world/world.h"

using namespace de;

boolean ddMapSetup;
timespan_t ddMapTime;

// Should we be caching successfully loaded maps?
static byte mapCache = true; // cvar

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
               / sourcePath.fileName() + '-' + cacheIdForMap(sourcePath);
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
        Records::iterator found = records.find(uri.resolved());
        if(found != records.end())
        {
            return &found.value();
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
     * Attempt to peform a JIT conversion of the map data with the help of a
     * converter plugin.
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

        // Ask each converter in turn whether the map format is recognizable
        // and if so to interpret and transfer it to us via the runtime map
        // editing interface.
        if(!DD_CallHooks(HOOK_MAP_CONVERT, 0, (void *) reinterpret_cast<uri_s const *>(&uri)))
            return 0;

        // A converter signalled success.

        // Were we able to produce a valid map from the data it provided?
        if(!MPE_GetLastBuiltMapResult())
            return 0;

        Map *map = MPE_GetLastBuiltMap();
        DENG_ASSERT(map != 0);

        map->_uri = uri;

        // Generate the unique map id.
        File1 &markerLump       = App_FileSystem().nameIndex().lump(markerLumpNum);
        String uniqueId         = composeUniqueMapId(markerLump);
        QByteArray uniqueIdUtf8 = uniqueId.toUtf8();
        qstrncpy(map->_oldUniqueId, uniqueIdUtf8.constData(), sizeof(map->_oldUniqueId));

        // Are we caching this map?
        /*if(mapCache)
        {
            // Ensure the destination directory exists.
            F_MakePath(rec.cachePath.toUtf8().constData());

            // Cache the map!
            DAM_MapWrite(map, rec.cachePath);
        }*/

        return map;
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
};

World::World() : d(new Instance(this))
{}

void World::consoleRegister() // static
{
    //C_VAR_BYTE("map-cache", &mapCache, 0, 0, 1);
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

bool World::loadMap(de::Uri const &uri)
{
    LOG_AS("World::loadMap");
    LOG_MSG("Loading \"%s\"...") << uri;

    if(isServer)
    {
        // Whenever the map changes, remote players must tell us when they're
        // ready to begin receiving frames.
        for(uint i = 0; i < DDMAXPLAYERS; ++i)
        {
            //player_t *plr = &ddPlayers[i];
            if(/*!(plr->shared.flags & DDPF_LOCAL) &&*/ clients[i].connected)
            {
                LOG_DEBUG("Client %i marked as 'not ready' to receive frames.") << i;
                clients[i].ready = false;
            }
        }
    }

    Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);

    if((d->map = d->loadMap(uri)))
    {
        LOG_INFO("Map elements: %d Vertexes, %d Lines, %d Sectors, %d BSP Nodes, %d BSP Leafs and %d Segments")
            << d->map->vertexCount()  << d->map->lineCount()    << d->map->sectorCount()
            << d->map->bspNodeCount() << d->map->bspLeafCount() << d->map->segmentCount();

        // Call the game's setup routines.
        if(gx.SetupForMapData)
        {
            gx.SetupForMapData(DMU_VERTEX,  d->map->vertexCount());
            gx.SetupForMapData(DMU_LINE,    d->map->lineCount());
            gx.SetupForMapData(DMU_SIDE,    d->map->sideCount());
            gx.SetupForMapData(DMU_SECTOR,  d->map->sectorCount());
        }

        // Do any initialization/error checking work we need to do.
        // Must be called before we go any further.
        P_InitUnusedMobjList();

        // Must be called before any mobjs are spawned.
        d->map->initNodePiles();

#ifdef __CLIENT__
        // Prepare the client-side data.
        if(isClient)
        {
            d->map->initClMobjs();
        }

        Rend_DecorInitForMap();
#endif

        // See what mapinfo says about this map.
        Uri mapUri = d->map->uri();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&mapUri));
        if(!mapInfo)
        {
            Uri defaultMapUri("*", RC_NULL);
            mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&defaultMapUri));
        }

#ifdef __CLIENT__
        ded_sky_t *skyDef = 0;
        if(mapInfo)
        {
            skyDef = Def_GetSky(mapInfo->skyID);
            if(!skyDef)
                skyDef = &mapInfo->sky;
        }
        Sky_Configure(skyDef);
#endif

        // Setup accordingly.
        if(mapInfo)
        {
            d->map->_globalGravity = mapInfo->gravity;
            d->map->_ambientLightLevel = mapInfo->ambient * 255;
        }
        else
        {
            // No map info found, so set some basic stuff.
            d->map->_globalGravity = 1.0f;
            d->map->_ambientLightLevel = 0;
        }

        d->map->_effectiveGravity = d->map->_globalGravity;

#ifdef __CLIENT__
        Rend_RadioInitForMap();
#endif

        d->map->initSkyFix();

        // Init the thinker lists (public and private).
        d->map->initThinkerLists(0x1 | 0x2);

#ifdef __CLIENT__
        if(isClient)
        {
            d->map->clMobjReset();
        }

        // Tell shadow bias to initialize the bias light sources.
        SB_InitForMap(d->map->oldUniqueId());

        // Clear player data, too, since we just lost all clmobjs.
        Cl_InitPlayers();

        RL_DeleteLists();
        Rend_UpdateLightModMatrix();
#endif

        // Invalidate old cmds and init player values.
        for(uint i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];

            /*
            if(isServer && plr->shared.inGame)
                clients[i].runTime = SECONDS_TO_TICKS(gameTime);*/

            plr->extraLight = plr->targetExtraLight = 0;
            plr->extraLightCounter = 0;
        }

        // Make sure that the next frame doesn't use a filtered viewer.
        R_ResetViewer();

#ifdef __CLIENT__
        // Material animations should begin from their first step.
        App_Materials().restartAllAnimations();

        R_InitObjlinkBlockmapForMap();

        LO_InitForMap(); // Lumobj management.
        R_InitShadowProjectionListsForMap(); // Projected mobj shadows.
        VL_InitForMap(); // Converted vlights (from lumobjs) management.

        d->map->initLightGrid();

        R_InitRendPolyPools();
#endif

        // Init Particle Generator links.
        P_PtcInitForMap();

        return true;
    }

    return false;
}

static void resetAllMapPlaneVisHeights(Map &map)
{
    foreach(Sector *sector, map.sectors())
    foreach(Plane *plane, sector->planes())
    {
        plane->resetVisHeight();
    }
}

static void updateAllMapSectors(Map &map)
{
    foreach(Sector *sector, map.sectors())
    {
        sector->updateSoundEmitterOrigin();
#ifdef __CLIENT__
        map.updateMissingMaterialsForLinesOfSector(*sector);
        S_MarkSectorReverbDirty(sector);
#endif
    }
}

void World::setupMap(int mode)
{
    switch(mode)
    {
    case DDSMM_INITIALIZE:
        // A new map is about to be setup.
        ddMapSetup = true;

#ifdef __CLIENT__
        App_Materials().purgeCacheQueue();
#endif
        return;

    case DDSMM_AFTER_LOADING:
        DENG_ASSERT(d->map);

        // Update everything again. Its possible that after loading we now have
        // more HOMs to fix, etc..
        d->map->initSkyFix();

        updateAllMapSectors(*d->map);
        resetAllMapPlaneVisHeights(*d->map);

        d->map->initPolyobjs();
        DD_ResetTimer();
        return;

    case DDSMM_FINALIZE: {
        DENG_ASSERT(d->map);

        if(gameTime > 20000000 / TICSPERSEC)
        {
            // In very long-running games, gameTime will become so large that
            // it cannot be accurately converted to 35 Hz integer tics. Thus it
            // needs to be reset back to zero.
            gameTime = 0;
        }

        // We are now finished with the map entity db.
        EntityDatabase_Delete(d->map->entityDatabase);

#ifdef __SERVER__
        // Init server data.
        Sv_InitPools();
#endif

#ifdef __CLIENT__
        // Recalculate the light range mod matrix.
        Rend_UpdateLightModMatrix();
#endif

        d->map->initPolyobjs();
        P_MapSpawnPlaneParticleGens();

        updateAllMapSectors(*d->map);
        resetAllMapPlaneVisHeights(*d->map);

#ifdef __CLIENT__
        d->map->buildSurfaceLists();

        Time begunPrecacheAt;
        Rend_CacheForMap();
        App_Materials().processCacheQueue();
        LOG_INFO(String("Precaching completed in %1 seconds.").arg(begunPrecacheAt.since(), 0, 'g', 2));
#endif

        S_SetupForChangedMap();

        // Map setup has been completed.

        // Run any commands specified in MapInfo.
        Uri mapUri = d->map->uri();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&mapUri));
        if(mapInfo && mapInfo->execute)
        {
            Con_Execute(CMDS_SCRIPT, mapInfo->execute, true, false);
        }

        // Run the special map setup command, which the user may alias to do
        // something useful.
        String cmd = "init-" + mapUri.resolved();
        if(Con_IsValidCommand(cmd.toUtf8().constData()))
        {
            Con_Executef(CMDS_SCRIPT, false, "%s", cmd.toUtf8().constData());
        }

#ifdef __CLIENT__
        // Clear any input events that might have accumulated during setup.
        DD_ClearEvents();
#endif

        // Now that the setup is done, let's reset the timer so that it will
        // appear that no time has passed during the setup.
        DD_ResetTimer();

        // Determine the invoid status of players.
        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            ddplayer_t &ddpl = ddPlayers[i].shared;

            ddpl.inVoid = true;

            if(mobj_t *mo = ddpl.mo)
            {
                if(BspLeaf *bspLeaf = d->map->bspLeafAtPoint(mo->origin))
                {
                    if(mo->origin[VZ] >= bspLeaf->sector().floor().visHeight() &&
                       mo->origin[VZ] < bspLeaf->sector().ceiling().visHeight() - 4)
                    {
                        ddpl.inVoid = false;
                    }
                }
            }
        }

        // Reset map time.
        ddMapTime = 0;

        // We've finished setting up the map.
        ddMapSetup = false;

#ifdef __CLIENT__
        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;
#endif

        Z_PrintStatus();

        // Inform interested parties that the "current" map has changed.
        d->notifyMapChange();

        return; }

    default:
        throw Error("World::setupMap", QString("Unknown mode %1").arg(mode));
    }
}

void World::clearMap()
{
    d->map = 0;
}

void World::resetMapCache()
{
    d->records.clear();
}

} // namespace de
