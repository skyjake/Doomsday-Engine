/** @file world.cpp  World base class.
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/world.h"
#include "doomsday/world/convexsubspace.h"
#include "doomsday/world/line.h"
#include "doomsday/world/map.h"
#include "doomsday/world/mapbuilder.h"
#include "doomsday/world/mapconversionreporter.h"
#include "doomsday/world/materials.h"
#include "doomsday/world/mobjthinkerdata.h"
#include "doomsday/world/polyobjdata.h"
#include "doomsday/world/sector.h"
#include "doomsday/world/sky.h"
#include "doomsday/world/thinkers.h"
#include "doomsday/world/surface.h"
#include "doomsday/console/exec.h"
#include "doomsday/defs/ded.h"
#include "doomsday/defs/mapinfo.h"
#include "doomsday/resource/mapmanifests.h"
#include "doomsday/resource/resources.h"
#include "doomsday/filesys/lumpindex.h"
#include "doomsday/DoomsdayApp"
#include "doomsday/busymode.h"
#include "doomsday/players.h"
#include "api_player.h"
#include "bindings.h"

#include <de/Context>
#include <de/Scheduler>
#include <de/ScriptSystem>
#include <de/legacy/memoryzone.h>

namespace world {

using namespace de;

static World *theWorld = nullptr;

int World::ddMapSetup = false;
int World::validCount = 1;

DE_PIMPL(World)
{
    Binder binder; // Doomsday Script bindings for the World.
    Record worldModule;

    timespan_t time = 0; // World-wide time.
    Scheduler  scheduler;
    
    mobj_t *unusedMobjList = nullptr;

    Record           fallbackMapInfo; // Used when no effective MapInfo definition.
    std::unique_ptr<world::Map> map;
    world::Materials materials;

    Impl(Public *i) : Base(i)
    {
        theWorld = thisPublic;

        initBindings(binder, worldModule);
        ScriptSystem::get().addNativeModule("World", worldModule);

        // One time init of the fallback MapInfo definition.
        defn::MapInfo(fallbackMapInfo).resetToDefaults();
    }

    ~Impl()
    {
        map.reset();
        theWorld = nullptr;
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
    Map *convertMap(const res::MapManifest &mapManifest,
                    MapConversionReporter *reporter = nullptr)
    {
        // We require a map converter for this.
        if (!Plug_CheckForHook(HOOK_MAP_CONVERT))
            return nullptr;

        LOG_DEBUG("Attempting \"%s\"...") << mapManifest.composeUri().path();

        if (!mapManifest.sourceFile()) return nullptr;

        // Initiate the conversion process.
        editMap.clear();
        editMap.begin();

        // Associate the map with its corresponding manifest.
        editMap->setManifest(&const_cast<res::MapManifest &>(mapManifest));

        if (reporter)
        {
            // Instruct the reporter to begin observing the conversion.
            reporter->setMap(editMap);
        }

        // Ask each converter in turn whether the map format is recognizable
        // and if so to interpret and transfer it to us via the runtime map
        // editing interface.
        if (!DoomsdayApp::plugins().callAllHooks(HOOK_MAP_CONVERT, 0,
                                                 const_cast<res::Id1MapRecognizer *>(&mapManifest.recognizer())))
            return nullptr;

        // A converter signalled success.

        // End the conversion process (if not already).
        world::editMap.end();

        // Take ownership of the map.
        return world::editMap.take();
    }

    /**
     * Attempt to load the associated map data.
     *
     * @return  The loaded map if successful. Ownership given to the caller.
     */
    world::Map *loadMap(const res::MapManifest &mapManifest, MapConversionReporter *reporter = nullptr)
    {
        LOG_AS("ClientServerWorld::loadMap");

        // Try a JIT conversion with the help of a plugin.
        auto *map = convertMap(mapManifest, reporter);
        if (!map)
        {
            LOG_WARNING("Failed conversion of \"%s\".") << mapManifest.composeUri().path();
        }
        return map;
    }
    /**
     * Replace the current map with @a map.
     */
    void makeCurrent(Map *map)
    {
        // This is now the current map (if any).
        this->map.reset(map);
        if (!map) return;

        // We cannot make an editable map current.
        DE_ASSERT(!map->isEditable());

        // Print summary information about this map.
        LOG_MAP_NOTE(_E(b) "Current map elements:");
        LOG_MAP_NOTE("%s") << map->elementSummaryAsStyledText();

        // Init the thinker lists (public and private).
        map->thinkers().initLists(0x1 | 0x2);

        // Must be called before any mobjs are spawned.
        map->initNodePiles();

        map->initPolyobjs();

        // Update based on Map Info.
        map->update();

        const res::Uri mapUri = map->uri();

        // The game may need to perform it's own finalization now that the
        // "current" map has changed.
        auto &gx = DoomsdayApp::plugins().gameExports();
        if (gx.FinalizeMapChange)
        {
            gx.FinalizeMapChange(reinterpret_cast<const uri_s *>(&mapUri));
        }
        
        self().mapFinalized();

        /*
         * Post-change map setup has now been fully completed.
         */

        // Run any commands specified in MapInfo.
        if (const String execute = map->mapInfo().gets("execute"))
        {
            Con_Execute(CMDS_SCRIPT, execute, true, false);
        }

        // Run the special map setup command, which the user may alias to do
        // something useful.
        if (!mapUri.isEmpty())
        {
            String cmd = "init-" + mapUri.path();
            if (Con_IsValidCommand(cmd))
            {
                Con_Executef(CMDS_SCRIPT, false, "%s", cmd.c_str());
            }
        }

        // Reset world time.
        time = 0;

        Z_PrintStatus();

        // Inform interested parties that the "current" map has changed.
        DE_NOTIFY_PUBLIC(MapChange, i) i->worldMapChanged();
    }
    
    bool changeMap(res::MapManifest *mapManifest = nullptr)
    {
        scheduler.clear();
        map.reset();
        Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);
        unusedMobjList = nullptr;

        // Are we just unloading the current map?
        if (!mapManifest) return true;

        LOG_MSG("Loading map \"%s\"...") << mapManifest->composeUri().path();

        // A new map is about to be set up.
        World::ddMapSetup = true;

        // Attempt to load in the new map.
        MapConversionReporter reporter;
        std::unique_ptr<Map> newMap(loadMap(*mapManifest, &reporter));
        if (newMap)
        {
            // The map may still be in an editable state -- switch to playable.
            const bool mapIsPlayable = newMap->endEditing();
            
            // Cancel further reports about the map.
            reporter.setMap(nullptr);

            if (!mapIsPlayable)
            {
                // Darn. Discard the useless data.
                newMap.reset();
            }
        }

        // This becomes the new current map.
        makeCurrent(newMap.release());

        // We've finished setting up the map.
        World::ddMapSetup = false;

        // Output a human-readable report of any issues encountered during conversion.
        reporter.writeLog();

        return bool(map);
    }

    DE_PIMPL_AUDIENCES(MapChange, FrameState, PlaneMovement)
};

DE_AUDIENCE_METHODS(World, MapChange, FrameState, PlaneMovement)

World::World() : d(new Impl(this))
{
    DmuArgs::setPointerToIndexFunc(P_ToIndex);

    // Let players know that a world exists.
    DoomsdayApp::players().forAll([this] (Player &plr)
    {
        plr.setWorld(this);
        return LoopContinue;
    });
}

void World::useDefaultConstructors()
{
    Factory::setConvexSubspaceConstructor([](mesh::Face &f, world::BspLeaf *bl) {
        return new world::ConvexSubspace(f, bl);
    });
    Factory::setLineConstructor([](world::Vertex &s, world::Vertex &t, int flg,
                                   world::Sector *fs, world::Sector *bs) {
        return new world::Line(s, t, flg, fs, bs);
    });
    Factory::setLineSideConstructor([](world::Line &ln, world::Sector *s) {
        return new world::LineSide(ln, s);
    });
    Factory::setLineSideSegmentConstructor([](world::LineSide &ls, mesh::HEdge &he) {
        return new world::LineSideSegment(ls, he);
    });
    Factory::setMapConstructor([]() { return new world::Map(); });
    Factory::setMobjThinkerDataConstructor([](const Id &id) { return new MobjThinkerData(id); });
    Factory::setMaterialConstructor([] (world::MaterialManifest &m) {
        return new world::Material(m);
    });
    Factory::setPlaneConstructor([](world::Sector &sec, const Vec3f &norm, double hgt) {
        return new world::Plane(sec, norm, hgt);
    });
    Factory::setPolyobjDataConstructor([]() { return new world::PolyobjData(); });
    Factory::setSkyConstructor([](const defn::Sky *def) { return new world::Sky(def); });
    Factory::setSubsectorConstructor([] (const List<world::ConvexSubspace *> &sl) {
        return new world::Subsector(sl);
    });
    Factory::setSurfaceConstructor([](world::MapElement &me, float opac, const Vec3f &clr) {
        return new world::Surface(me, opac, clr);
    });
    Factory::setVertexConstructor([](mesh::Mesh &m, const Vec2d &p) -> world::Vertex * {
        return new world::Vertex(m, p);
    });
}

const Record &World::mapInfoForMapUri(const res::Uri &mapUri) const
{
    // Is there a MapInfo definition for the given URI?
    if (const Record *def = DED_Definitions()->mapInfos.tryFind("id", mapUri.compose()))
    {
        return *def;
    }
    // Is there is a default definition (for all maps)?
    if (const Record *def = DED_Definitions()->mapInfos.tryFind("id", res::Uri("Maps", Path("*")).compose()))
    {
        return *def;
    }
    // Use the fallback.
    return d->fallbackMapInfo;
}

void World::reset()
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
        for (ddpsprite_t &pspr : ddpl.pSprites)
        {
            pspr.statePtr = nullptr;
        }

        return LoopContinue;
    });
}

mobj_t *World::takeUnusedMobj()
{
    mobj_t *mo = nullptr;
    if (d->unusedMobjList)
    {
        mo = d->unusedMobjList;
        d->unusedMobjList = d->unusedMobjList->sNext;
    }
    return mo;
}

void World::putUnusedMobj(mobj_t *mo)
{
    if (mo)
    {
        mo->sNext = d->unusedMobjList;
        d->unusedMobjList = mo;
    }
}

//void World::timeChanged(const Clock &)
//{
//    // Nothing to do.
//}

bool World::allowAdvanceTime() const
{
    return true;
}

void World::tick(timespan_t)
{
    // Nothing to do.
}

void World::setMap(world::Map *map)
{
    d->map.reset(map);
}

bool World::hasMap() const
{
    return bool(d->map);
}

Map &World::map() const
{
    if (!hasMap())
    {
        /// @throw MapError Attempted with no map loaded.
        throw MapError("World::map", "No map is currently loaded");
    }
    return *d->map;
}

void World::aboutToChangeMap() {}

void World::mapFinalized()
{
    // Init player values.
    DoomsdayApp::players().forAll([](Player &plr) {
        plr.extraLight        = 0;
        plr.targetExtraLight  = 0;
        plr.extraLightCounter = 0;
        return LoopContinue;
    });
}

bool World::changeMap(const res::Uri &mapUri)
{
    res::MapManifest *mapDef = nullptr;

    if (!mapUri.path().isEmpty())
    {
        mapDef = Resources::get().mapManifests().tryFindMapManifest(mapUri);
    }
    
    aboutToChangeMap();

    // Switch to busy mode (if we haven't already) except when simply unloading.
    if (!mapUri.path().isEmpty() && !DoomsdayApp::app().busyMode().isActive())
    {
        /// @todo Use progress bar mode and update progress during the setup.
        return DoomsdayApp::app().busyMode().runNewTaskWithName(
                    BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR | BUSYF_TRANSITION |
                    (DoomsdayApp::verbose ? BUSYF_CONSOLE_OUTPUT : 0),
                    "Loading map...", [this, &mapDef](void *) -> int {
            return d->changeMap(mapDef);
        });
    }
    else
    {
        return d->changeMap(mapDef);
    }
}

Materials &World::materials()
{
    return d->materials;
}

const Materials &World::materials() const
{
    return d->materials;
}

Scheduler &World::scheduler()
{
    return d->scheduler;
}

void World::advanceTime(timespan_t delta)
{
    if (allowAdvanceTime())
    {
        d->time += delta;
        d->scheduler.advanceTime(TimeSpan(delta));
    }
}

timespan_t World::time() const
{
    return d->time;
}

void World::update()
{
    DoomsdayApp::players().forAll([] (Player &plr)
    {
        // States have changed, the state pointers are unknown.
        for (ddpsprite_t &pspr : plr.publicData().pSprites)
        {
            pspr.statePtr = nullptr;
        }
        return LoopContinue;
    });

    // Update the current map, also.
    if (hasMap())
    {
        map().update();
    }
}

void World::notifyFrameState(FrameState frameState)
{
    DE_NOTIFY(FrameState, i)
    {
        i->worldFrameState(frameState);
    }
}

void World::notifyBeginPlaneMovement(const Plane &plane)
{
    DE_NOTIFY(PlaneMovement, i)
    {
        i->planeMovementBegan(plane);
    }
}

World &World::get() // static
{
    DE_ASSERT(theWorld);
    return *theWorld;
}

mobj_t &World::contextMobj(const Context &ctx) // static
{
    const int id = ctx.selfInstance().geti(DE_STR("__id__"), 0);
    mobj_t *mo = get().map().thinkers().mobjById(id);
    if (!mo)
    {
        throw Map::MissingObjectError("World::contextMobj",
                                      Stringf("Mobj %d does not exist", id));
    }
    return *mo;
}

} // namespace world

