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

#include <de/memoryzone.h>
#include <de/timer.h>

#include <de/Error>

#include "de_base.h"
#include "de_console.h"
#include "de_defs.h"
#include "de_play.h"

#include "Materials"

#include "Plane"
#include "Sector"

#include "audio/s_main.h"
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

boolean ddMapSetup;

namespace de {

DENG2_PIMPL(World)
{
    /// Current map.
    Map *map;

    Instance(Public *i) : Base(i), map(0)
    {}
};

World::World() : d(new Instance(this))
{}

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
    LOG_MSG("Loading map \"%s\"...") << uri;

    if(isServer)
    {
        // Whenever the map changes, remote players must tell us when
        // they're ready to begin receiving frames.
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

    if((d->map = App_MapArchive().loadMap(uri)))
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

        // Update everything again. Its possible that after loading we
        // now have more HOMs to fix, etc..
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
            // In very long-running games, gameTime will become so large that it cannot be
            // accurately converted to 35 Hz integer tics. Thus it needs to be reset back
            // to zero.
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

        float startTime = Timer_Seconds();
        Rend_CacheForMap();
        App_Materials().processCacheQueue();
        VERBOSE( Con_Message("Precaching took %.2f seconds.", Timer_Seconds() - startTime) )
#endif

        S_SetupForChangedMap();

        // Map setup has been completed.

        // Run any commands specified in Map Info.
        Uri mapUri = d->map->uri();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&mapUri));
        if(mapInfo && mapInfo->execute)
        {
            Con_Execute(CMDS_SCRIPT, mapInfo->execute, true, false);
        }

        // Run the special map setup command, which the user may alias to do something useful.
        String cmd = "init-" + mapUri.resolved();
        if(Con_IsValidCommand(cmd.toUtf8().constData()))
        {
            Con_Executef(CMDS_SCRIPT, false, "%s", cmd.toUtf8().constData());
        }

#ifdef __CLIENT__
        // Clear any input events that might have accumulated during the
        // setup period.
        DD_ClearEvents();
#endif

        // Now that the setup is done, let's reset the tictimer so it'll
        // appear that no time has passed during the setup.
        DD_ResetTimer();

        // Kill all local commands and determine the invoid status of players.
        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];
            ddplayer_t *ddpl = &plr->shared;

            //clients[i].numTics = 0;

            // Determine if the player is in the void.
            ddpl->inVoid = true;
            if(ddpl->mo)
            {
                BspLeaf *bspLeaf = d->map->bspLeafAtPoint(ddpl->mo->origin);
                if(bspLeaf)
                {
                    /// @todo $nplanes
                    if(ddpl->mo->origin[VZ] >= bspLeaf->sector().floor().visHeight() &&
                       ddpl->mo->origin[VZ] < bspLeaf->sector().ceiling().visHeight() - 4)
                    {
                        ddpl->inVoid = false;
                    }
                }
            }
        }

        // Reset the map tick timer.
        ddMapTime = 0;

        // We've finished setting up the map.
        ddMapSetup = false;

#ifdef __CLIENT__
        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;
#endif

        DENG2_FOR_AUDIENCE(MapChange, i) i->currentMapChanged();

        Z_PrintStatus();
        return; }

    default:
        throw Error("World::setupMap", QString("Unknown mode %1").arg(mode));
    }
}

void World::clearMap()
{
    d->map = 0;
}

} // namespace de
