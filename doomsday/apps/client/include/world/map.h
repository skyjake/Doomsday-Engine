/** @file map.h  World Map.
 * @ingroup world
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#pragma once

#ifdef __SERVER__
#  error "Client's map.h included from Server app"
#endif

#include "client/clplanemover.h"
#include "client/clpolymover.h"

#include "world/p_object.h"
#include "clientworld.h"
#include "generator.h"
#include "render/lumobj.h"
#include "render/skydrawable.h"

#include <doomsday/mesh/mesh.h>
#include <doomsday/world/bspleaf.h>
#include <doomsday/world/bspnode.h>
#include <doomsday/world/line.h>
#include <doomsday/world/polyobj.h>
#include <doomsday/network/protocol.h>
#include <doomsday/uri.h>
#include <doomsday/world/map.h>
#include <doomsday/world/ithinkermapping.h>

#include <de/hash.h>
#include <de/list.h>
#include <de/set.h>
#include <de/binarytree.h>
#include <de/id.h>
#include <de/info.h>
#include <de/observers.h>
#include <de/vector.h>

class ClSkyPlane;
class Plane;
class Surface;
class Vertex;
struct de_api_sector_hacks_s;

namespace world
{
    class Blockmap;
    class ConvexSubspace;
    class LineBlockmap;
    class Sector;
    class Subsector;
    class Sky;
    class Thinkers;
}

/**
 * World map.
 */
class Map
    : public world::Map
    , DE_OBSERVES(world::World, FrameState)
{
    DE_NO_COPY  (Map)
    DE_NO_ASSIGN(Map)

public:

    /// Required light grid is missing. @ingroup errors
    DE_ERROR(MissingLightGridError);

    /// Attempted to add a new element/object when full. @ingroup errors
    DE_ERROR(FullError);

    /*
     * Constants:
     */
    static const int MAX_BIAS_SOURCES = 8 * 32;  // Hard limit due to change tracking.

    /// Maximum number of generators per map.
    static const int MAX_GENERATORS = 512;

    typedef de::Set<Plane *> PlaneSet;
    typedef de::Set<Surface *> SurfaceSet;
    typedef de::Hash<thid_t, struct mobj_s *> ClMobjHash;

public:
    /**
     * Construct a new map initially configured in an editable state. Whilst editable new
     * map elements can be added, thereby allowing the map to be constructed dynamically.
     * When done editing @ref endEditing() should be called to switch the map into a
     * non-editable (i.e., playable) state.
     *
     * @param manifest  Resource manifest for the map, if any (Can be set later).
     */
    explicit Map(res::MapManifest *manifest = nullptr);

    void applySectorHacks(world::Sector &sector, const struct de_api_sector_hacks_s *hacks) override;
    bool endEditing() override;
    void update() override;
    void link(mobj_t &mob, int flags) override;
    de::String objectSummaryAsStyledText() const override;

    static void consoleRegister();

    void serializeInternalState(de::Writer &to) const override;

    void deserializeInternalState(de::Reader &                  from,
                                  const world::IThinkerMapping &thinkerMapping) override;

    /**
     * Force an update on all decorated surfaces.
     */
    void redecorate();

public:

    //- Luminous-objects ----------------------------------------------------------------

    /**
     * Returns the total number of lumobjs in the map.
     */
    int lumobjCount() const;

    /**
     * Add a new lumobj to the map.
     *
     * @return  Lumobj instance. Ownership taken.
     */
    Lumobj &addLumobj(Lumobj *lumobj);

    /**
     * Removes the specified lumobj from the map.
     *
     * @see removeAllLumobjs()
     */
    void removeLumobj(int which);

    /**
     * Remove all lumobjs from the map.
     *
     * @see removeLumobj()
     */
    void removeAllLumobjs();

    /**
     * Lookup a Lumobj in the map by it's unique @a index.
     */
    Lumobj &lumobj   (int index) const;
    Lumobj *lumobjPtr(int index) const;

    /**
     * Iterate Lumobjs in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each Lumobj.
     */
    de::LoopResult forAllLumobjs(const std::function<de::LoopResult (Lumobj &)>& callback) const;

public:  //- Particle generators --------------------------------------------------------

    /**
     * Returns the total number of @em active generators in the map.
     */
    int generatorCount() const;

    /**
     * Attempt to spawn a new (particle) generator for the map. If no free identifier is
     * available then @c nullptr is returned.
     */
    Generator *newGenerator();

    /**
     * Iterate Generators in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each Generator.
     */
    de::LoopResult forAllGenerators(const std::function<de::LoopResult (Generator &)>& callback) const;

    /**
     * Iterate Generators linked in the specified @a sector, making a function @a callback
     * for each.
     *
     * @param sector    Sector requirement (only linked @em Generators will be processed).
     * @param callback  Function to call for each Generator.
     */
    de::LoopResult forAllGeneratorsInSector(
        const world::Sector &                             sector,
        const std::function<de::LoopResult(Generator &)> &callback) const;

    void unlinkGenerator(Generator &generator);

//- Skies -------------------------------------------------------------------------------

    SkyDrawable::Animator &skyAnimator() const;

    ClSkyPlane       &skyFloor();
    const ClSkyPlane &skyFloor()  const;

    ClSkyPlane       &skyCeiling();
    const ClSkyPlane &skyCeiling() const;

    inline ClSkyPlane       &skyPlane(bool ceiling) {
        return ceiling ? skyCeiling() : skyFloor();
    }
    inline const ClSkyPlane &skyPlane(bool ceiling) const {
        return ceiling ? skyCeiling() : skyFloor();
    }

    /**
    * Returns @c true if the given @a point is in the void (outside all map subspaces).
    */
    bool isPointInVoid(const de::Vec3d &pos) const;

    /**
     * Returns the set of scrolling surfaces for the map.
     */
    SurfaceSet /*const*/ &scrollingSurfaces();

    /**
     * $smoothmatoffset: Roll the surface material offset tracker buffers.
     */
    void updateScrollingSurfaces();

    /**
     * Returns the set of tracked planes for the map.
     */
    PlaneSet /*const*/ &trackedPlanes();

    /**
     * $smoothplane: Roll the height tracker buffers.
     */
    void updateTrackedPlanes();

    /**
     * Perform spreading of all contacts in the specified map space @a region.
     */
    void spreadAllContacts(const AABoxd &region);

    /**
     * Fixing the sky means that for adjacent sky sectors the lower sky ceiling is lifted
     * to match the upper sky. The raising only affects rendering, it has no bearing on gameplay.
     */
    void initSkyFix();

    /**
     * Rebuild the surface material lists. To be called when a full update is necessary.
     */
    void buildMaterialLists();

    /**
     * Initialize the map object => BSP leaf "contact" blockmaps.
     */
    void initContactBlockmaps();

    /**
     * Initialize data and structures needed for FakeRadio.
     */
    void initRadio();

    /**
     * Spawn all generators for the map which should be initialized automatically during
     * map setup.
     */
    void initGenerators();

    /**
     * Attempt to spawn all flat-triggered particle generators for the map. To be called
     * after map setup is completed.
     *
     * @note  Cannot presently be done in @ref initGenerators() as this is called during
     * initial Map load and before any saved game has been loaded.
     */
    void spawnPlaneParticleGens();

    /**
     * Destroys all clientside clmobjs in the map. To be called when a network game ends.
     */
    void clearClMobjs();

    /**
     * Deletes hidden, unpredictable or nulled mobjs for which we have not received updates
     * in a while.
     */
    void expireClMobjs();

    /**
     * Find/create a client mobj with the unique identifier @a id. Client mobjs are just
     * like normal mobjs, except they have additional network state.
     *
     * To check whether a given mobj is a client mobj, use Cl_IsClientMobj(). The network
     * state can then be accessed with ClMobj_GetInfo().
     *
     * @param id         Identifier of the client mobj. Every client mobj has a unique
     *                   identifier.
     * @param canCreate  @c true= create a new client mobj if none existing.
     *
     * @return  Pointer to the gameside mobj.
     */
    struct mobj_s *clMobjFor(thid_t id, bool canCreate = false) const;

    /**
     * Iterate client-mobjs, making a function @c callback for each. Iteration ends if a
     * callback returns a non-zero value.
     *
     * @param callback  Function to callback for each client mobj.
     * @param context   Data pointer passed to the callback.
     *
     * @return  @c 0 if all callbacks return @c 0; otherwise the result of the last.
     */
    int clMobjIterator(int (*callback) (struct mobj_s *, void *), void *context = nullptr);

    /**
     * Provides readonly access to the client mobj hash.
     */
    const ClMobjHash &clMobjHash() const;

protected:
    void worldFrameState(world::World::FrameState) override;

private:
    DE_PRIVATE(d)
};
