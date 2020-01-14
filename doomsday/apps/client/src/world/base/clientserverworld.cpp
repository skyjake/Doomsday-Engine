/** @file worldsystem.cpp  World subsystem.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "world/clientserverworld.h"

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
#include "world/bindings_world.h"
#include "edit_map.h"

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "client/cl_def.h"
#  include "client/cl_frame.h"
#  include "client/cl_player.h"
#  include "client/cledgeloop.h"
#  include "gl/gl_main.h"
#  include "world/contact.h"
#  include "world/polyobjdata.h"
#  include "world/subsector.h"
#  include "world/vertex.h"
#  include "Lumobj"
#  include "render/viewports.h"  // R_ResetViewer
#  include "render/rend_fakeradio.h"
#  include "render/rend_main.h"
#  include "render/rendersystem.h"
#  include "render/rendpoly.h"
#  include "MaterialAnimator"
#  include "ui/progress.h"
#  include "ui/inputsystem.h"
#endif

#ifdef __SERVER__
#  include "server/sv_pool.h"
#  include <doomsday/world/convexsubspace.h>
#  include <doomsday/world/mobjthinkerdata.h>
#endif

#include <doomsday/world/sector.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>
#include <doomsday/defs/mapinfo.h>
#include <doomsday/resource/mapmanifests.h>
#include <doomsday/world/MaterialManifest>
#include <doomsday/world/Materials>
#include <doomsday/world/plane.h>
#include <doomsday/world/polyobjdata.h>
#include <doomsday/world/subsector.h>
#include <doomsday/world/surface.h>
#include <doomsday/world/thinkers.h>

#include <de/KeyMap>
#include <de/legacy/memoryzone.h>
#include <de/legacy/timer.h>
#include <de/Binder>
#include <de/Context>
#include <de/Error>
#include <de/Log>
#include <de/Scheduler>
#include <de/ScriptSystem>
#include <de/Time>

#include <map>
#include <utility>

using namespace de;
using namespace res;

/**
 * Observes the progress of a map conversion and records any issues/problems that
 * are encountered in the process. When asked, compiles a human-readable report
 * intended to assist mod authors in debugging their maps.
 *
 * @todo Consolidate with the missing material reporting done elsewhere -ds
 */
class MapConversionReporter
: DE_OBSERVES(world::Map, UnclosedSectorFound)
, DE_OBSERVES(world::Map, OneWayWindowFound)
, DE_OBSERVES(world::Map, Deletion)
{
    /// Record "unclosed sectors".
    /// Sector index => world point relatively near to the problem area.
    typedef std::map<dint, Vec2i> UnclosedSectorMap;

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
    MapConversionReporter(world::Map *map = nullptr)
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
    void setMap(world::Map *newMap)
    {
        if (_map != newMap)
        {
            observeMap(false);
            _map = newMap;
            observeMap(true);
        }
    }

    /// @see setMap(), clearReport()
    inline void setMapAndClearReport(world::Map *newMap)
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
        if (dint numToLog = maxWarnings(unclosedSectorCount()))
        {
            String str;

            UnclosedSectorMap::const_iterator it = _unclosedSectors.begin();
            for (dint i = 0; i < numToLog; ++i, ++it)
            {
                if (i != 0) str += "\n";
                str += Stringf(
                    "Sector #%d is unclosed near %s", it->first, it->second.asText().c_str());
            }

            if (numToLog < unclosedSectorCount())
                str += Stringf("\n(%i more like this)", unclosedSectorCount() - numToLog);

            LOGDEV_MAP_WARNING("%s") << str;
        }

        if (dint numToLog = maxWarnings(oneWayWindowCount()))
        {
            String str;

            OneWayWindowMap::const_iterator it = _oneWayWindows.begin();
            for (dint i = 0; i < numToLog; ++i, ++it)
            {
                if (i != 0) str += "\n";
                str += Stringf("Line #%i seems to be a One-Way Window (back faces sector #%i).",
                           it->first, it->second);
            }

            if (numToLog < oneWayWindowCount())
                str += Stringf("\n(%i more like this)", oneWayWindowCount() - numToLog);

            LOGDEV_MAP_MSG("%s") << str;
        }
    }

protected:
    /// Observes Map UnclosedSectorFound.
    void unclosedSectorFound(world::Sector &sector, const Vec2d &nearPoint) override
    {
        _unclosedSectors.insert(std::make_pair(sector.indexInArchive(), nearPoint.toVec2i()));
    }

    /// Observes Map OneWayWindowFound.
    void oneWayWindowFound(world::Line &line, world::Sector &backFacingSector) override
    {
        _oneWayWindows.insert(std::make_pair(line.indexInArchive(), backFacingSector.indexInArchive()));
    }

    /// Observes Map Deletion.
    void mapBeingDeleted(const world::Map &map) override
    {
        DE_ASSERT(&map == _map);  // sanity check.
        DE_UNUSED(map);
        _map = nullptr;
    }

private:
    inline dint unclosedSectorCount() const { return dint( _unclosedSectors.size() ); }
    inline dint oneWayWindowCount() const   { return dint( _oneWayWindows.size() ); }

    static inline dint maxWarnings(dint issueCount)
    {
#ifdef DE_DEBUG
        return issueCount; // No limit.
#else
        return de::min(issueCount, maxWarningsPerType);
#endif
    }

    void observeMap(bool yes)
    {
        if (!_map) return;

        if (yes)
        {
            _map->audienceForDeletion()            += this;
            _map->audienceForOneWayWindowFound()   += this;
            _map->audienceForUnclosedSectorFound() += this;
        }
        else
        {
            _map->audienceForDeletion()            -= this;
            _map->audienceForOneWayWindowFound()   -= this;
            _map->audienceForUnclosedSectorFound() -= this;
        }
    }

    world::Map *      _map = nullptr; ///< Map currently being reported on, if any (not owned).
    UnclosedSectorMap _unclosedSectors;
    OneWayWindowMap   _oneWayWindows;
};

const dint MapConversionReporter::maxWarningsPerType = 10;

DE_PIMPL(ClientServerWorld)
#if defined(__SERVER__)
, DE_OBSERVES(world::Thinkers, Removal)
#endif
{
    Binder binder;               ///< Doomsday Script bindings for the World.
    Record worldModule;

    timespan_t time = 0;         ///< World-wide time.
    Scheduler scheduler;

    Impl(Public *i) : Base(i)
    {
        world::initBindings(binder, worldModule);
        ScriptSystem::get().addNativeModule("World", worldModule);

        // Callbacks.
        world::DmuArgs::setPointerToIndexFunc(P_ToIndex);
        
        using world::Factory;
        
#ifdef __CLIENT__
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
#else
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
#endif
    }

#if defined(__CLIENT__)
    static inline RenderSystem &rendSys()
    {
        return ClientApp::renderSystem();
    }
#endif

    /**
     * Attempt JIT conversion of the map data with the help of a plugin. Note that
     * the map is left in an editable state in case the caller wishes to perform
     * any further changes.
     *
     * @param reporter  Reporter which will observe the conversion process.
     *
     * @return  The newly converted map (if any).
     */
    world::Map *convertMap(const res::MapManifest &mapManifest,
                           MapConversionReporter * reporter = nullptr)
    {
        // We require a map converter for this.
        if (!Plug_CheckForHook(HOOK_MAP_CONVERT))
            return nullptr;

        LOG_DEBUG("Attempting \"%s\"...") << mapManifest.composeUri().path();

        if (!mapManifest.sourceFile()) return nullptr;

        // Initiate the conversion process.
        MPE_Begin(nullptr/*dummy*/);

        auto *newMap = MPE_Map();

        // Associate the map with its corresponding manifest.
        newMap->setManifest(&const_cast<res::MapManifest &>(mapManifest));

        if (reporter)
        {
            // Instruct the reporter to begin observing the conversion.
            reporter->setMap(newMap);
        }

        // Ask each converter in turn whether the map format is recognizable
        // and if so to interpret and transfer it to us via the runtime map
        // editing interface.
        if (!DoomsdayApp::plugins().callAllHooks(HOOK_MAP_CONVERT, 0,
                                                const_cast<Id1MapRecognizer *>(&mapManifest.recognizer())))
            return nullptr;

        // A converter signalled success.

        // End the conversion process (if not already).
        MPE_End();

        // Take ownership of the map.
        return MPE_TakeMap();
    }

    /**
     * Attempt to load the associated map data.
     *
     * @return  The loaded map if successful. Ownership given to the caller.
     */
    world::Map *loadMap(res::MapManifest & mapManifest, MapConversionReporter *reporter = nullptr)
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
    void makeCurrent(world::Map *map)
    {
        // This is now the current map (if any).
        self().setMap(map);
        if (!map) return;

        // We cannot make an editable map current.
        DE_ASSERT(!map->isEditable());

        // Print summary information about this map.
        LOG_MAP_NOTE(_E(b) "Current map elements:");
        LOG_MAP_NOTE("%s") << map->elementSummaryAsStyledText();

        // Init the thinker lists (public and private).
        map->thinkers().initLists(0x1 | 0x2);

        // Must be called before we go any further.
        P_InitUnusedMobjList();

        // Must be called before any mobjs are spawned.
        map->initNodePiles();

        map->initPolyobjs();

        // Update based on Map Info.
        map->update();

#ifdef __CLIENT__
        {
            Map &clMap = map->as<Map>();
            
            // Connect the map to world audiences.
            /// @todo The map should instead be notified when it is made current
            /// so that it may perform the connection itself. Such notification
            /// would also afford the map the opportunity to prepare various data
            /// which is only needed when made current (e.g., caches for render).
            self().audienceForFrameBegin() += clMap;

            // Set up the SkyDrawable to get its config from the map's Sky.
            clMap.skyAnimator().setSky(&rendSys().sky().configure(&map->sky().as<Sky>()));

            // Prepare the client-side data.
            Cl_ResetFrame();
            Cl_InitPlayers();  // Player data, too.

            /// @todo Defer initial generator spawn until after finalization.
            clMap.initGenerators();
        }
#endif

        // The game may need to perform it's own finalization now that the
        // "current" map has changed.
        const res::Uri mapUri = (map->hasManifest() ? map->manifest().composeUri() : res::makeUri("Maps:"));
        if (gx.FinalizeMapChange)
        {
            gx.FinalizeMapChange(reinterpret_cast<const uri_s *>(&mapUri));
        }

        if (gameTime > 20000000 / TICSPERSEC)
        {
            // In very long-running games, gameTime will become so large that
            // it cannot be accurately converted to 35 Hz integer tics. Thus it
            // needs to be reset back to zero.
            gameTime = 0;
        }

        // Init player values.
        DoomsdayApp::players().forAll([] (Player &plr)
        {
            plr.extraLight        = 0;
            plr.targetExtraLight  = 0;
            plr.extraLightCounter = 0;

#ifdef __CLIENT__
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
#endif
            return LoopContinue;
        });

#ifdef __SERVER__
        if (::isServer)
        {
            // Init server data.
            Sv_InitPools();
        }
#endif

#ifdef __CLIENT__
        {
            Map &clMap = map->as<Map>();
            App_AudioSystem().worldMapChanged();

            GL_SetupFogFromMapInfo(map->mapInfo().accessedRecordPtr());
            
            clMap.initSkyFix();
            clMap.spawnPlaneParticleGens();

            // Precaching from 100 to 200.
            Con_SetProgress(100);
            Time begunPrecacheAt;
            // Sky models usually have big skins.
            rendSys().sky().cacheAssets();
            App_Resources().cacheForCurrentMap();
            App_Resources().processCacheQueue();
            LOG_RES_VERBOSE("Precaching completed in %.2f seconds") << begunPrecacheAt.since();

            rendSys().clearDrawLists();
            R_InitRendPolyPools();
            Rend_UpdateLightModMatrix();

            clMap.initRadio();
            clMap.initContactBlockmaps();
            R_InitContactLists(clMap);
            rendSys().worldSystemMapChanged(clMap);

            // Rewind/restart material animators.
            /// @todo Only rewind animators responsible for map-surface contexts.
            world::Materials::get().updateLookup();
            world::Materials::get().forAnimatedMaterials([] (world::Material &material)
            {
                return material.as<ClientMaterial>().forAllAnimators([] (MaterialAnimator &animator)
                {
                    animator.rewind();
                    return LoopContinue;
                });
            });

            // Make sure that the next frame doesn't use a filtered viewer.
            R_ResetViewer();

            // Clear any input events that might have accumulated during setup.
            ClientApp::inputSystem().clearEvents();

            // Inform the timing system to suspend the starting of the clock.
            firstFrameAfterLoad = true;
        }
#endif

        /*
         * Post-change map setup has now been fully completed.
         */

        // Run any commands specified in MapInfo.
        String execute = map->mapInfo().gets("execute");
        if (!execute.isEmpty())
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

        // Now that the setup is done, let's reset the timer so that it will
        // appear that no time has passed during the setup.
        DD_ResetTimer();

        Z_PrintStatus();

        // Inform interested parties that the "current" map has changed.
        self().notifyMapChange();
    }

    /// @todo Split this into subtasks (load, make current, cache assets).
    bool changeMap(res::MapManifest *mapManifest = nullptr)
    {
        auto *map = self().mapPtr();

#ifdef __SERVER__
        if (map)
        {
            map->thinkers().audienceForRemoval() -= this;
        }
#endif
#ifdef __CLIENT__
        if (map)
        {
            // Remove the current map from our audiences.
            /// @todo Map should handle this.
            self().audienceForFrameBegin() -= map->as<Map>();
        }

        // As the memory zone does not provide the mechanisms to prepare another
        // map in parallel we must free the current map first.
        /// @todo The memory zone would still be useful if the purge and tagging
        /// mechanisms allowed more fine grained control. It is no longer useful
        /// for allocating memory used elsewhere so it should be repurposed for
        /// this usage specifically.
        R_DestroyContactLists();
#endif 

        scheduler.clear();

        delete map;
        self().setMap(nullptr);

        Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);

        // Are we just unloading the current map?
        if (!mapManifest) return true;

        LOG_MSG("Loading map \"%s\"...") << mapManifest->composeUri().path();

        // A new map is about to be set up.
        World::ddMapSetup = true;

        // Attempt to load in the new map.
        MapConversionReporter reporter;
        auto *newMap = loadMap(*mapManifest, &reporter);
        if (newMap)
        {
            // The map may still be in an editable state -- switch to playable.
            const bool mapIsPlayable = newMap->endEditing();
            
            // Cancel further reports about the map.
            reporter.setMap(nullptr);

            if (!mapIsPlayable)
            {
                // Darn. Discard the useless data.
                delete newMap; newMap = nullptr;
            }
        }

#ifdef __SERVER__
        newMap->thinkers().audienceForRemoval() += this;
#endif

        // This becomes the new current map.
        makeCurrent(newMap);

        // We've finished setting up the map.
        World::ddMapSetup = false;

        // Output a human-readable report of any issues encountered during conversion.
        reporter.writeLog();

        return self().hasMap();
    }

#ifdef __SERVER__
    void thinkerRemoved(thinker_t &th) override
    {
        auto *mob = reinterpret_cast<mobj_t *>(&th);

        // If the state of the mobj is the NULL state, this is a
        // predictable mobj removal (result of animation reaching its
        // end) and shouldn't be included in netGame deltas.
        if (!mob->state || !runtimeDefs.states.indexOf(mob->state))
        {
            Sv_MobjRemoved(th.id);
        }
    }
#endif

#ifdef __CLIENT__
    DE_PIMPL_AUDIENCE(FrameBegin)
    DE_PIMPL_AUDIENCE(FrameEnd)
#endif
};

#ifdef __CLIENT__
DE_AUDIENCE_METHOD(ClientServerWorld, FrameBegin)
DE_AUDIENCE_METHOD(ClientServerWorld, FrameEnd)
#endif

ClientServerWorld::ClientServerWorld()
    : World()
    , d(new Impl(this))
{}

world::Map &ClientServerWorld::map() const
{
    if (!hasMap())
    {
        /// @throw MapError Attempted with no map loaded.
        throw MapError("ClientServerWorld::map", "No map is currently loaded");
    }
    return World::map().as<world::Map>();
}

bool ClientServerWorld::changeMap(const res::Uri &mapUri)
{
    res::MapManifest *mapDef = nullptr;

    if (!mapUri.path().isEmpty())
    {
        mapDef = App_Resources().mapManifests().tryFindMapManifest(mapUri);
    }

    // Switch to busy mode (if we haven't already) except when simply unloading.
    if (!mapUri.path().isEmpty() && !DoomsdayApp::app().busyMode().isActive())
    {
        /// @todo Use progress bar mode and update progress during the setup.
        return DoomsdayApp::app().busyMode().runNewTaskWithName(
                    BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR | BUSYF_TRANSITION | (::verbose ? BUSYF_CONSOLE_OUTPUT : 0),
                    "Loading map...", [this, &mapDef] (void *)
        {
            return d->changeMap(mapDef);
        });
    }
    else
    {
        return d->changeMap(mapDef);
    }
}

void ClientServerWorld::reset()
{
    World::reset();

#ifdef __CLIENT__
    if (isClient)
    {
        Cl_ResetFrame();
        Cl_InitPlayers();
    }
#endif

    // If a map is currently loaded -- unload it.
    unloadMap();
}

void ClientServerWorld::update()
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

Scheduler &ClientServerWorld::scheduler()
{
    return d->scheduler;
}

void ClientServerWorld::advanceTime(timespan_t delta)
{
#if defined (__CLIENT__)
    if (!::clientPaused)
#endif
    {
        d->time += delta;
        d->scheduler.advanceTime(TimeSpan(delta));
    }
}

timespan_t ClientServerWorld::time() const
{
    return d->time;
}

void ClientServerWorld::tick(timespan_t elapsed)
{
#ifdef __CLIENT__
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
#else
    DE_UNUSED(elapsed);
#endif
}

mobj_t &ClientServerWorld::contextMobj(const Context &ctx) // static
{
    /// @todo Not necessarily always the current map. -jk
    const int id = ctx.selfInstance().geti(DE_STR("__id__"), 0);
    mobj_t *mo = App_World().map().thinkers().mobjById(id);
    if (!mo)
    {
        throw world::Map::MissingObjectError("ClientServerWorld::contextMobj",
                                      String::format("Mobj %d does not exist", id));
    }
    return *mo;
}

#ifdef __CLIENT__

void ClientServerWorld::beginFrame(bool resetNextViewer)
{
    // Notify interested parties that a new frame has begun.
    DE_NOTIFY(FrameBegin, i) i->worldSystemFrameBegins(resetNextViewer);
}

void ClientServerWorld::endFrame()
{
    // Notify interested parties that the current frame has ended.
    DE_NOTIFY(FrameEnd, i) i->worldSystemFrameEnds();
}

#endif  // __CLIENT__
