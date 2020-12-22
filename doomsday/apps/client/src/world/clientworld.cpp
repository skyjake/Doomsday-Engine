/** @file clientworld.cpp  World subsystem for the Client app.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "world/clientworld.h"

#include "dd_main.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "def_main.h"  // ::defs
#include "api_player.h"
#include "network/net_main.h"
#include "api_mapedit.h"
#include "world/p_players.h"
#include "world/p_ticker.h"
#include "world/sky.h"
#include "edit_map.h"

#include "clientapp.h"
#include "client/cl_def.h"
#include "client/cl_frame.h"
#include "client/cl_player.h"
#include "client/cledgeloop.h"
#include "gl/gl_main.h"
#include "world/contact.h"
#include "world/polyobjdata.h"
#include "world/subsector.h"
#include "world/vertex.h"
#include "render/lumobj.h"
#include "render/viewports.h"  // R_ResetViewer
#include "render/rend_fakeradio.h"
#include "render/rend_main.h"
#include "render/rendersystem.h"
#include "render/rendpoly.h"
#include "resource/materialanimator.h"
#include "ui/progress.h"
#include "ui/inputsystem.h"

#include <doomsday/world/sector.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>
#include <doomsday/defs/mapinfo.h>
#include <doomsday/res/mapmanifests.h>
#include <doomsday/world/mapbuilder.h>
#include <doomsday/world/materialmanifest.h>
#include <doomsday/world/materials.h>
#include <doomsday/world/plane.h>
#include <doomsday/world/polyobjdata.h>
#include <doomsday/world/subsector.h>
#include <doomsday/world/surface.h>
#include <doomsday/world/thinkers.h>

#include <de/keymap.h>
#include <de/legacy/memoryzone.h>
#include <de/legacy/timer.h>
#include <de/dscript.h>
#include <de/error.h>
#include <de/log.h>
#include <de/time.h>

#include <map>
#include <utility>

using namespace de;
using namespace res;

ClientWorld::ClientWorld()
{
    using world::Factory;

    Factory::setConvexSubspaceConstructor([](mesh::Face &f, world::BspLeaf *bl) -> world::ConvexSubspace * {
        return new ConvexSubspace(f, bl);
    });
    Factory::setLineConstructor([](world::Vertex &s, world::Vertex &t, int flg, world::Sector *fs, world::Sector *bs) -> world::Line * {
        return new Line(s, t, flg, fs, bs);
    });
    Factory::setLineSideConstructor([](world::Line &ln, world::Sector *s) -> world::LineSide * {
        return new LineSide(ln, s);
    });
    Factory::setLineSideSegmentConstructor([](world::LineSide &ls, mesh::HEdge &he) -> world::LineSideSegment * {
        return new LineSideSegment(ls, he);
    });
    Factory::setMapConstructor([]() -> world::Map * {
        return new Map();
    });
    Factory::setMobjThinkerDataConstructor([](const Id &id) -> MobjThinkerData * {
        return new ClientMobjThinkerData(id);
    });
    Factory::setMaterialConstructor([](world::MaterialManifest &m) -> world::Material * {
        return new ClientMaterial(m);
    });
    Factory::setPlaneConstructor([](world::Sector &sec, const Vec3f &norm, double hgt) -> world::Plane * {
        return new Plane(sec, norm, hgt);
    });
    Factory::setPolyobjDataConstructor([]() -> world::PolyobjData * {
        return new PolyobjData();
    });
    Factory::setSkyConstructor([](const defn::Sky *def) -> world::Sky * {
        return new Sky(def);
    });
    Factory::setSubsectorConstructor([](const List<world::ConvexSubspace *> &sl) -> world::Subsector * {
        return new Subsector(sl);
    });
    Factory::setSurfaceConstructor([](world::MapElement &me, float opac, const Vec3f &clr) -> world::Surface * {
        return new Surface(me, opac, clr);
    });
    Factory::setVertexConstructor([](mesh::Mesh &m, const Vec2d &p) -> world::Vertex * {
        return new Vertex(m, p);
    });
    
    audienceForMapChange() += []() {
        // Now that the setup is done, let's reset the timer so that it will
        // appear that no time has passed during the setup.
        DD_ResetTimer();
    };
}

Map &ClientWorld::map() const
{
    return world::World::map().as<Map>();
}

void ClientWorld::aboutToChangeMap()
{
    App_AudioSystem().aboutToUnloadMap();
    App_Resources().purgeCacheQueue();

    if (hasMap())
    {
        // Remove the current map from our audiences.
        /// @todo Map should handle this.
        audienceForFrameState() -= map();
    }

    // As the memory zone does not provide the mechanisms to prepare another
    // map in parallel we must free the current map first.
    /// @todo The memory zone would still be useful if the purge and tagging
    /// mechanisms allowed more fine grained control. It is no longer useful
    /// for allocating memory used elsewhere so it should be repurposed for
    /// this usage specifically.
    R_DestroyContactLists();
}

void ClientWorld::mapFinalized()
{
    world::World::mapFinalized();
    
    if (gameTime > 20000000 / TICSPERSEC)
    {
        // In very long-running games, gameTime will become so large that
        // it cannot be accurately converted to 35 Hz integer tics. Thus it
        // needs to be reset back to zero.
        gameTime = 0;
    }

    DoomsdayApp::players().forAll([](Player &plr) {
        auto &client = plr.as<ClientPlayer>();

        // Determine the "invoid" status.
        client.inVoid = true;
        
        if (const mobj_t *mob = plr.publicData().mo)
        {
            if (Mobj_HasSubsector(*mob))
            {
                const auto &subsec = Mobj_Subsector(*mob).as<Subsector>();
                if (   mob->origin[2] >= subsec.visFloor  ().heightSmoothed()
                    && mob->origin[2] <  subsec.visCeiling().heightSmoothed() - 4)
                {
                    client.inVoid = false;
                }
            }
        }
        return LoopContinue;
    });

    // Prepare the client-side data.
    Cl_ResetFrame();
    Cl_InitPlayers();  // Player data, too (reset to zero).

    auto &rendSys = ClientApp::render();

    audienceForFrameState() += map();

    // Set up the SkyDrawable to get its config from the map's Sky.
    map().skyAnimator().setSky(&ClientApp::render().sky().configure(&map().sky().as<Sky>()));

    ClientApp::audio().worldMapChanged();

    GL_SetupFogFromMapInfo(map().mapInfo().accessedRecordPtr());
    
    map().initSkyFix();
    map().spawnPlaneParticleGens();

    // Precaching from 100 to 200.
    Con_SetProgress(100);
    Time begunPrecacheAt;
    // Sky models usually have big skins.
    rendSys.sky().cacheAssets();
    App_Resources().cacheForCurrentMap();
    App_Resources().processCacheQueue();
    LOG_RES_VERBOSE("Precaching completed in %.2f seconds") << begunPrecacheAt.since();

    rendSys.clearDrawLists();
    R_InitRendPolyPools();
    Rend_UpdateLightModMatrix();

    map().initGenerators();
    map().initRadio();
    map().initContactBlockmaps();
    R_InitContactLists(map());
    rendSys.worldSystemMapChanged(map());

    // Rewind/restart material animators.
    /// @todo Only rewind animators responsible for map-surface contexts.
    world::Materials::get().updateLookup();
    world::Materials::get().forAnimatedMaterials([](world::Material &material)
    {
        return material.as<ClientMaterial>().forAllAnimators([](MaterialAnimator &animator)
        {
            animator.rewind();
            return LoopContinue;
        });
    });

    // Make sure that the next frame doesn't use a filtered viewer.
    R_ResetViewer();

    // Clear any input events that might have accumulated during setup.
    ClientApp::input().clearEvents();

    // Inform the timing system to suspend the starting of the clock.
    firstFrameAfterLoad = true;
}

bool ClientWorld::allowAdvanceTime() const
{
    return !clientPaused;
}

void ClientWorld::reset()
{
    World::reset();
    if (netState.isClient)
    {
        Cl_ResetFrame();
        Cl_InitPlayers();
    }
    unloadMap();
}

void ClientWorld::tick(timespan_t elapsed)
{
    world::World::tick(elapsed);
    
    if (hasMap())
    {
        map().as<Map>().skyAnimator().advanceTime(elapsed);

        if (DD_IsSharpTick())
        {
            map().thinkers().forAll(reinterpret_cast<thinkfunc_t>(gx.MobjThinker), 0x1, [] (thinker_t *th)
            {
                Mobj_AnimateHaloOcclussion(*reinterpret_cast<mobj_t *>(th));
                return LoopContinue;
            });
        }
    }
}
