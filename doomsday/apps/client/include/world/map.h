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

#ifndef DENG_WORLD_MAP_H
#define DENG_WORLD_MAP_H

#include <functional>
#include <QHash>
#include <QList>
#include <QSet>
#include <doomsday/BspNode>
#include <doomsday/world/map.h>
#include <doomsday/world/ithinkermapping.h>
#include <doomsday/uri.h>
#include <de/shell/Protocol>
#include <de/BinaryTree>
#include <de/Id>
#include <de/Observers>
#include <de/Vector>

#ifdef __CLIENT__
#  include "client/clplanemover.h"
#  include "client/clpolymover.h"
#endif

#include "Mesh"
#include "Line"
#include "Polyobj"
#include "world/bspleaf.h"
#include "world/p_object.h"

#ifdef __CLIENT__
#  include "world/clientserverworld.h"
#  include "Generator"
//#  include "BiasSource"
#  include "Lumobj"
#  include "render/skydrawable.h"
#endif

class Plane;
class Sector;
class Surface;
class Vertex;
struct de_api_sector_hacks_s;

#if 0
#ifdef __CLIENT__
class BiasTracker;
namespace de { class LightGrid; }
#endif
#endif // 0

namespace de { class Info; }

namespace world {

class Blockmap;
class ConvexSubspace;
class LineBlockmap;
class Subsector;
class Sky;
class Thinkers;
#ifdef __CLIENT__
class ClSkyPlane;
#endif

/**
 * World map.
 */
class Map : public world::BaseMap
#ifdef __CLIENT__
, DENG2_OBSERVES(ClientServerWorld, FrameBegin)
#endif
{
    DENG2_NO_COPY  (Map)
    DENG2_NO_ASSIGN(Map)

public:
    /// Base error for runtime map editing errors. @ingroup errors
    DENG2_ERROR(EditError);

    /// Required map element is missing. @ingroup errors
    DENG2_ERROR(MissingElementError);

    /// Required blockmap is missing. @ingroup errors
    DENG2_ERROR(MissingBlockmapError);

    /// Required BSP data is missing. @ingroup errors
    DENG2_ERROR(MissingBspTreeError);

    /// Required thinker lists are missing. @ingroup errors
    DENG2_ERROR(MissingThinkersError);

#ifdef __CLIENT__
    /// Required light grid is missing. @ingroup errors
    DENG2_ERROR(MissingLightGridError);

    /// Attempted to add a new element/object when full. @ingroup errors
    DENG2_ERROR(FullError);
#endif

    /// Notified when a one-way window construct is first found.
    DENG2_DEFINE_AUDIENCE(OneWayWindowFound, void oneWayWindowFound(Line &line, Sector &backFacingSector))

    /// Notified when an unclosed sector is first found.
    DENG2_DEFINE_AUDIENCE(UnclosedSectorFound, void unclosedSectorFound(Sector &sector, de::Vector2d const &nearPoint))

    /*
     * Constants:
     */
#ifdef __CLIENT__
    static de::dint const MAX_BIAS_SOURCES = 8 * 32;  // Hard limit due to change tracking.

    /// Maximum number of generators per map.
    static de::dint const MAX_GENERATORS = 512;

    typedef QSet<Plane *> PlaneSet;
    typedef QSet<Surface *> SurfaceSet;
    typedef QHash<thid_t, struct mobj_s *> ClMobjHash;
#endif

public:  /// @todo make private:
    de::ddouble _globalGravity    = 0;  ///< The defined gravity for this map.
    de::ddouble _effectiveGravity = 0;  ///< The effective gravity for this map.
    de::dint _ambientLightLevel   = 0;  ///< Ambient lightlevel for the current map.

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

    /**
     * Returns the effective map-info definition Record for the map.
     *
     * @see WorldSystem::mapInfoForMapUri()
     */
    de::Record const &mapInfo() const;

    /**
     * Returns the points which describe the boundary of the map coordinate space, which,
     * are defined by the minimal and maximal vertex coordinates of the non-editable,
     * non-polyobj line geometries).
     */
    AABoxd const &bounds() const;

    inline de::Vector2d origin    () const {
        return de::Vector2d(bounds().min);
    }

    inline de::Vector2d dimensions() const {
        return de::Vector2d(bounds().max) - de::Vector2d(bounds().min);
    }

    /**
     * Returns the minimum ambient light level for the whole map.
     */
    de::dint ambientLightLevel() const;

    /**
     * Returns the currently effective gravity multiplier for the map.
     */
    de::ddouble gravity() const;

    /**
     * Change the effective gravity multiplier for the map.
     *
     * @param newGravity  New gravity multiplier.
     */
    void setGravity(de::ddouble newGravity);

    /**
     * To be called following an engine reset to update the map state.
     */
    void update();

#ifdef __CLIENT__

    void serializeInternalState(de::Writer &to) const override;

    void deserializeInternalState(de::Reader &from, IThinkerMapping const &thinkerMapping) override;

    de::String objectsDescription() const;

    void restoreObjects(de::Info const &objState, IThinkerMapping const &thinkerMapping) const;

    /**
     * Force an update on all decorated surfaces.
     */
    void redecorate();

public:  //- Light sources --------------------------------------------------------------

#if 0
    /**
     * Returns the total number of BiasSources in the map.
     */
    de::dint biasSourceCount() const;

    /**
     * Attempt to add a new bias light source to the map (a copy is made).
     *
     * @note At most @ref MAX_BIAS_SOURCES are supported for technical reasons.
     *
     * @return  Reference to the newly added bias source.
     *
     * @see biasSourceCount()
     * @throws FullError  Once capacity is reached.
     */
    BiasSource &addBiasSource(BiasSource const &biasSource = BiasSource());

    /**
     * Removes the specified bias light source from the map.
     *
     * @see removeAllBiasSources()
     */
    void removeBiasSource(de::dint which);

    /**
     * Remove all bias sources from the map.
     *
     * @see removeBiasSource()
     */
    void removeAllBiasSources();

    /**
     * Lookup a BiasSource by it's unique @a index.
     */
    BiasSource &biasSource   (de::dint index) const;
    BiasSource *biasSourcePtr(de::dint index) const;

    /**
     * Finds the bias source nearest to the specified map space @a point.
     *
     * @note This result is not cached. May return @c 0 if no bias sources exist.
     */
    BiasSource *biasSourceNear(de::Vector3d const &point) const;

    /**
     * Iterate the BiasSources in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each BiasSource.
     */
    de::LoopResult forAllBiasSources(std::function<de::LoopResult (BiasSource &)> callback) const;

    /**
     * Lookup the unique index for the given bias @a source.
     */
    de::dint indexOf(BiasSource const &source) const;

    /**
     * Returns the time in milliseconds when the current render frame began. Used for
     * interpolation purposes.
     */
    de::duint biasCurrentTime() const;

    /**
     * Returns the frameCount of the current render frame. Used for tracking changes to
     * bias sources/surfaces.
     */
    de::duint biasLastChangeOnFrame() const;
#endif

    //- Luminous-objects ----------------------------------------------------------------

    /**
     * Returns the total number of lumobjs in the map.
     */
    de::dint lumobjCount() const;

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
    void removeLumobj(de::dint which);

    /**
     * Remove all lumobjs from the map.
     *
     * @see removeLumobj()
     */
    void removeAllLumobjs();

    /**
     * Lookup a Lumobj in the map by it's unique @a index.
     */
    Lumobj &lumobj   (de::dint index) const;
    Lumobj *lumobjPtr(de::dint index) const;

    /**
     * Iterate Lumobjs in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each Lumobj.
     */
    de::LoopResult forAllLumobjs(std::function<de::LoopResult (Lumobj &)> callback) const;

#endif  // __CLIENT__

public:  //- Lines (and Sides) ----------------------------------------------------------

    /**
     * Returns the total number of Lines in the map.
     */
    de::dint lineCount() const;

    /**
     * Lookup a Line in the map by it's unique @a index.
     */
    Line &line   (de::dint index) const;
    Line *linePtr(de::dint index) const;

    /**
     * Iterate Lines in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each Line.
     */
    de::LoopResult forAllLines(std::function<de::LoopResult (Line &)> callback) const;

    /**
     * Lines and Polyobj lines (note polyobj lines are iterated first).
     *
     * @note validCount should be incremented before calling this to begin a new logical
     * traversal. Otherwise Lines marked with a validCount equal to this will be skipped
     * over (can be used to avoid processing a line multiple times during complex and/or
     * non-linear traversals.
     *
     * @param box       Axis-aligned bounding box in which Lines must be Blockmap-linked.
     * @param flags     @ref lineIteratorFlags
     * @param callback  Function to call for each Line.
     */
    de::LoopResult forAllLinesInBox(AABoxd const &box, de::dint flags,
        std::function<de::LoopResult (Line &)> callback) const;

    /**
     * @overload
     */
    inline de::LoopResult forAllLinesInBox(AABoxd const &box,
        std::function<de::LoopResult (Line &)> callback) const {
        return forAllLinesInBox(box, LIF_ALL, callback);
    }

    /**
     * The callback function will be called once for each Line that crosses the object.
     * This means all the lines will be two-sided.
     *
     * @param callback  Function to call for each Line.
     */
    de::LoopResult forAllLinesTouchingMobj(struct mobj_s &mob,
        std::function<de::LoopResult (Line &)> callback) const;

    // ---

    /**
     * Returns the total number of Line::Sides in the map.
     */
    inline de::dint sideCount() const { return lineCount() * 2; }

    /**
     * Lookup a LineSide in the map by it's unique @a index.
     *
     * @see toSideIndex()
     */
    LineSide &side   (de::dint index) const;
    LineSide *sidePtr(de::dint index) const;

    /**
     * Helper function which returns the relevant side index given a @a lineIndex and a
     * @a side identifier.
     *
     * Indices are produced as follows:
     * @code
     *  lineIndex / 2 + (backSide? 1 : 0);
     * @endcode
     *
     * @param lineIndex  Index of the line in the map.
     * @param side       Side of the line. @c =0 the Line::Front else Line::Back
     *
     * @return  Unique index for the identified side.
     */
    static de::dint toSideIndex(de::dint lineIndex, de::dint side);

public:  //- Map-objects ----------------------------------------------------------------

    de::LoopResult forAllMobjsTouchingLine(Line &line, std::function<de::LoopResult (struct mobj_s &)> callback) const;

    /**
     * @important Increment validCount before calling this!
     *
     * Iterate mobj_ts in the map that are "inside" the specified sector (even partially)
     * on the X|Y plane (mobj_t Z origin and Plane heights not considered).
     *
     * @param sector    Sector requirement (only consider map-objects "touching" this).
     * @param callback  Function to call for each mobj_t.
     */
    de::LoopResult forAllMobjsTouchingSector(Sector &sector, std::function<de::LoopResult (struct mobj_s &)> callback) const;

    /**
     * Links a mobj into both a block and a BSP leaf based on it's (x,y). Sets mobj->bspLeaf
     * properly. Calling with flags==0 only updates the BspLeaf pointer. Can be called without
     * unlinking first. Should be called AFTER mobj translation to (re-)insert the mobj.
     */
    void link(struct mobj_s &mobj, de::dint flags);

    /**
     * Unlinks a map-object from everything it has been linked to. Should be called BEFORE
     * mobj translation to extract the mobj.
     *
     * @param mob  Map-object to be unlinked.
     *
     * @return  DDLINK_* flags denoting what the mobj was unlinked from (in case we need
     * to re-link).
     */
    de::dint unlink(struct mobj_s &mob);

#ifdef __CLIENT__

public:  //- Particle generators --------------------------------------------------------

    /**
     * Returns the total number of @em active generators in the map.
     */
    de::dint generatorCount() const;

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
    de::LoopResult forAllGenerators(std::function<de::LoopResult (Generator &)> callback) const;

    /**
     * Iterate Generators linked in the specified @a sector, making a function @a callback
     * for each.
     *
     * @param sector    Sector requirement (only linked @em Generators will be processed).
     * @param callback  Function to call for each Generator.
     */
    de::LoopResult forAllGeneratorsInSector(Sector const &sector, std::function<de::LoopResult (Generator &)> callback) const;

    void unlink(Generator &generator);

#endif  // __CLIENT__

public:  //- Polyobjects ----------------------------------------------------------------

    /**
     * Returns the total number of Polyobjs in the map.
     */
    de::dint polyobjCount() const;

    /**
     * Lookup a Polyobj in the map by it's unique @a index.
     */
    Polyobj &polyobj   (de::dint index) const;
    Polyobj *polyobjPtr(de::dint index) const;

    /**
     * Iterate Polyobjs in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each Polyobj.
     */
    de::LoopResult forAllPolyobjs(std::function<de::LoopResult (Polyobj &)> callback) const;

    /**
     * Link the specified @a polyobj in any internal data structures for bookkeeping purposes.
     * Should be called AFTER Polyobj rotation and/or translation to (re-)insert the polyobj.
     *
     * @param polyobj  Poly-object to be linked.
     */
    void link(Polyobj &polyobj);

    /**
     * Unlink the specified @a polyobj from any internal data structures for bookkeeping
     * purposes. Should be called BEFORE Polyobj rotation and/or translation to extract
     * the polyobj.
     *
     * @param polyobj  Poly-object to be unlinked.
     */
    void unlink(Polyobj &polyobj);

//- Sectors -----------------------------------------------------------------------------

    /**
     * Returns the total number of Sectors in the map.
     */
    de::dint sectorCount() const;

    /**
     * Lookup a Sector in the map by it's unique @a index.
     */
    Sector &sector   (de::dint index) const;
    Sector *sectorPtr(de::dint index) const;

    /**
     * Iterate Sectors in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each Sector.
     */
    de::LoopResult forAllSectors(const std::function<de::LoopResult (Sector &)> &callback) const;

    /**
     * Increment validCount before calling this routine. The callback function will be
     * called once for each sector the mobj is touching (totally or partly inside). This
     * is not a 3D check; the mobj may actually reside above or under the sector.
     *
     * @param mob       Map-object to iterate the "touched" Sectors of.
     * @param callback  Function to call for each Sector.
     */
    de::LoopResult forAllSectorsTouchingMobj(struct mobj_s &mob, std::function<de::LoopResult (Sector &)> callback) const;

    /// Thrown when the referenced subsector is missing/unknown.
    DENG2_ERROR(MissingSubsectorError);

    /**
     * Lookup a Subsector in the map by it's unique identifier @a id.
     */
    Subsector &subsector   (de::Id id) const;
    Subsector *subsectorPtr(de::Id id) const;

    /**
     * Determine the Subsector which contains @a point and which is on the back side of
     * the binary map space partition which, lies in front of @a point.
     *
     * @param point  Map space coordinates to determine the Subsector for.
     *
     * @return  Subsector containing the specified point if any or @c nullptr if the
     * subsectors have not yet been built.
     */
    Subsector *subsectorAt(de::Vector2d const &point) const;

//- Skies -------------------------------------------------------------------------------

    /**
     * Returns the logical sky for the map.
     */
    Sky &sky() const;

#ifdef __CLIENT__

    SkyDrawable::Animator &skyAnimator() const;

    ClSkyPlane       &skyFloor();
    ClSkyPlane const &skyFloor()  const;

    ClSkyPlane       &skyCeiling();
    ClSkyPlane const &skyCeiling() const;

    inline ClSkyPlane       &skyPlane(bool ceiling) {
        return ceiling ? skyCeiling() : skyFloor();
    }
    inline ClSkyPlane const &skyPlane(bool ceiling) const {
        return ceiling ? skyCeiling() : skyFloor();
    }

#endif

public:  //- Subspaces ------------------------------------------------------------------

    /**
     * Returns the total number of subspaces in the map.
     */
    de::dint subspaceCount() const;

    /**
     * Lookup a Subspace in the map by it's unique @a index.
     */
    ConvexSubspace &subspace   (de::dint index) const;
    ConvexSubspace *subspacePtr(de::dint index) const;

    /**
     * Iterate ConvexSubspaces in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each ConvexSubspace.
     */
    de::LoopResult forAllSubspaces(std::function<de::LoopResult (ConvexSubspace &)> callback) const;

#ifdef __CLIENT__
    /**
    * Returns @c true if the given @a point is in the void (outside all map subspaces).
    */
    bool isPointInVoid(de::Vector3d const &pos) const;
#endif

public:  //- Vertexs --------------------------------------------------------------------

    /**
     * Returns the total number of Vertexs in the map.
     */
    de::dint vertexCount() const;

    /**
     * Lookup a Vertex in the map by it's unique @a index.
     */
    Vertex &vertex   (de::dint index) const;
    Vertex *vertexPtr(de::dint index) const;

    /**
     * Iterate Vertexs in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each Vertex.
     */
    de::LoopResult forAllVertexs(std::function<de::LoopResult (Vertex &)> callback) const;

public:  //- Data structures ------------------------------------------------------------

    /**
     * Provides access to the primary @ref Mesh geometry owned by the map. Note that further
     * meshes may be assigned to individual elements of the map should their geometries
     * not be representable as a manifold with the primary mesh (e.g., Polyobjs and BspLeaf
     * "extra" meshes).
     */
    de::Mesh const &mesh() const;

    /**
     * Provides access to the line blockmap.
     */
    LineBlockmap const &lineBlockmap() const;

    /**
     * Provides access to the mobj blockmap.
     */
    Blockmap const &mobjBlockmap() const;

    /**
     * Provides access to the polyobj blockmap.
     */
    Blockmap const &polyobjBlockmap() const;

    /**
     * Provides access to the convex subspace blockmap.
     */
    Blockmap const &subspaceBlockmap() const;

    /**
     * Provides access to the thinker lists for the map.
     */
    Thinkers /*const*/ &thinkers() const;

    /**
     * Returns @c true iff a BSP tree is available for the map.
     */
    bool hasBspTree() const;

    /**
     * Provides access to map's BSP tree, for efficient traversal.
     */
    BspTree const &bspTree() const;

    /**
     * Determine the BSP leaf on the back side of the BS partition that lies in front of
     * the specified point within the map's coordinate space.
     *
     * @note Always returns a valid BspLeaf although the point may not actually lay within
     * it (however it is on the same side of the space partition)!
     *
     * @param point  Map space coordinates to determine the BSP leaf for.
     *
     * @return  BspLeaf instance for that BSP node's leaf.
     */
    BspLeaf &bspLeafAt(de::Vector2d const &point) const;

    /**
     * @copydoc bspLeafAt()
     *
     * The test is carried out using fixed-point math for behavior compatible with vanilla
     * DOOM. Note that this means there is a maximum size for the point: it cannot exceed
     * the fixed-point 16.16 range (about 65k units).
     */
    BspLeaf &bspLeafAt_FixedPrecision(de::Vector2d const &point) const;

    /**
     * Given an @a emitter origin, attempt to identify the map element to which it belongs.
     *
     * @param emitter  The sound emitter to be identified.
     * @param sector   The identified sector if found is written here.
     * @param poly     The identified polyobj if found is written here.
     * @param plane    The identified plane if found is written here.
     * @param surface  The identified line side surface if found is written here.
     *
     * @return  @c true iff @a emitter is an identifiable map element.
     */
    bool identifySoundEmitter(ddmobj_base_t const &emitter, Sector **sector,
        Polyobj **poly, Plane **plane, Surface **surface) const;

#ifdef __CLIENT__

#if 0
    /**
     * Returns @c true if a LightGrid has been initialized for the map.
     *
     * @see lightGrid()
     */
    bool hasLightGrid() const;

    /**
     * Provides access to the light grid for the map.
     *
     * @see hasLightGrid()
     */
    de::LightGrid       &lightGrid();
    de::LightGrid const &lightGrid() const;

    /**
     * (Re)-initialize the light grid used for smoothed sector lighting.
     *
     * If the grid has not yet been initialized block light sources are determined at this
     * time (Subsectors must be built for this).
     *
     * If the grid has already been initialized calling this will perform a full update.
     *
     * @note Initialization may take some time depending on the complexity of the map
     * (physical dimensions, number of sectors) and should therefore be done "off-line".
     */
    void initLightGrid();
#endif

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
    void spreadAllContacts(AABoxd const &region);

#endif  // __CLIENT__

public:

    /**
     * Returns a rich formatted, textual summary of the map's elements, suitable for logging.
     */
    de::String elementSummaryAsStyledText() const;

    /**
     * Returns a rich formatted, textual summary of the map's objects, suitable for logging.
     */
    de::String objectSummaryAsStyledText() const;

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * To be called to initialize the dummy element arrays (which are used with the DMU API),
     * with a fixed number of @em shared dummies.
     */
    static void initDummies();

public:  /// @todo Most of the following should be private:

    /**
     * Initialize the node piles and link rings. To be called after map load.
     */
    void initNodePiles();

    /**
     * Initialize all polyobjs in the map. To be called after map load.
     */
    void initPolyobjs();

#ifdef __CLIENT__

    /**
     * Fixing the sky means that for adjacent sky sectors the lower sky ceiling is lifted
     * to match the upper sky. The raising only affects rendering, it has no bearing on gameplay.
     */
    void initSkyFix();

    /**
     * Rebuild the surface material lists. To be called when a full update is necessary.
     */
    void buildMaterialLists();

#if 0
    /**
     * Initializes bias lighting for the map. New light sources are initialized from the
     * loaded Light definitions. Map surfaces are prepared for tracking rays.
     *
     * Must be called before rendering a frame with bias lighting enabled.
     */
    void initBias();
#endif

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
    de::dint clMobjIterator(de::dint (*callback) (struct mobj_s *, void *), void *context = nullptr);

    /**
     * Provides readonly access to the client mobj hash.
     */
    ClMobjHash const &clMobjHash() const;

protected:

    /// Observes WorldSystem FrameBegin
    void worldSystemFrameBegins(bool resetNextViewer);

#endif  // __CLIENT__

public:  //- Editing --------------------------------------------------------------------

    /**
     * Returns @c true iff the map is currently in an editable state.
     */
    bool isEditable() const;

    /**
     * Switch the map from editable to non-editable (i.e., playable) state, incorporating
     * any new map elements, (re)building the BSP, etc...
     *
     * @return  @c true= mode switch was completed successfully.
     */
    bool endEditing();

    /**
     * @see isEditable()
     */
    Vertex *createVertex(de::Vector2d const &origin,
                         de::dint archiveIndex = MapElement::NoIndex);

    /**
     * @see isEditable()
     */
    Line *createLine(Vertex &v1, Vertex &v2, de::dint flags = 0,
                     Sector *frontSector = nullptr, Sector *backSector = nullptr,
                     de::dint archiveIndex = MapElement::NoIndex);

    /**
     * @see isEditable()
     */
    Polyobj *createPolyobj(de::Vector2d const &origin);

    /**
     * @see isEditable()
     */
    Sector *createSector(float lightLevel, const de::Vector3f &lightColor,
                         int archiveIndex = MapElement::NoIndex,
                         const struct de_api_sector_hacks_s *hacks = nullptr);

    /**
     * Provides a list of all the editable lines in the map.
     */
    typedef QList<Line *> Lines;
    Lines const &editableLines() const;

    /**
     * Provides a list of all the editable polyobjs in the map.
     */
    typedef QList<Polyobj *> Polyobjs;
    Polyobjs const &editablePolyobjs() const;

    /**
     * Provides a list of all the editable sectors in the map.
     */
    typedef QList<Sector *> Sectors;
    Sectors const &editableSectors() const;

    inline de::dint editableLineCount   () const { return editableLines   ().count(); }
    inline de::dint editablePolyobjCount() const { return editablePolyobjs().count(); }
    inline de::dint editableSectorCount () const { return editableSectors ().count(); }

//- Multiplayer -------------------------------------------------------------------------

    void initMapOutlinePacket(de::shell::MapOutlinePacket &packet);

private:
    DENG2_PRIVATE(d)
};

}  // namespace world

#endif  // DENG_WORLD_MAP_H
