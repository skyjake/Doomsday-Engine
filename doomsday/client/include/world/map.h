/** @file map.h  World map.
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

#ifndef DENG_WORLD_MAP_H
#define DENG_WORLD_MAP_H

#include "Mesh"

#include "Line"
#include "Polyobj"

#ifdef __CLIENT__
#  include "client/clmobjhash.h"
#  include "client/clplanemover.h"
#  include "client/clpolymover.h"

#  include "world/worldsystem.h"
#  include "Generator"

#  include "BiasSource"
#  include "Lumobj"
#endif

#include <doomsday/uri.h>

#include <de/Observers>
#include <de/Vector>
#include <QList>
#include <QMultiMap>
#include <QSet>

class BspLeaf;
class BspNode;
class Plane;
class Sector;
class SectorCluster;
class Surface;
class Vertex;

#ifdef __CLIENT__
class BiasTracker;
#endif

class LineBlockmap;

namespace de {

class Blockmap;
class EntityDatabase;
#ifdef __CLIENT__
class LightGrid;
#endif
class Thinkers;

/**
 * World map.
 *
 * @ingroup world
 */
class Map
#ifdef __CLIENT__
: DENG2_OBSERVES(WorldSystem, FrameBegin)
#endif
{
    DENG2_NO_COPY  (Map)
    DENG2_NO_ASSIGN(Map)

public:
    /// Base error for runtime map editing errors. @ingroup errors
    DENG2_ERROR(EditError);

    /// Required blockmap is missing. @ingroup errors
    DENG2_ERROR(MissingBlockmapError);

    /// Required BSP data is missing. @ingroup errors
    DENG2_ERROR(MissingBspError);

    /// Required thinker lists are missing. @ingroup errors
    DENG2_ERROR(MissingThinkersError);

#ifdef __CLIENT__
    /// Required light grid is missing. @ingroup errors
    DENG2_ERROR(MissingLightGridError);

    /// Attempted to add a new element when already full. @ingroup errors
    DENG2_ERROR(FullError);
#endif

    /// Notified when the map is about to be deleted.
    DENG2_DEFINE_AUDIENCE(Deletion, void mapBeingDeleted(Map const &map))

    /// Notified when a one-way window construct is first found.
    DENG2_DEFINE_AUDIENCE(OneWayWindowFound,
        void oneWayWindowFound(Line &line, Sector &backFacingSector))

    /// Notified when an unclosed sector is first found.
    DENG2_DEFINE_AUDIENCE(UnclosedSectorFound,
        void unclosedSectorFound(Sector &sector, Vector2d const &nearPoint))

    /*
     * Constants:
     */
#ifdef __CLIENT__
    static int const MAX_BIAS_SOURCES = 8 * 32; // Hard limit due to change tracking.

    /// Maximum number of generators per map.
    static int const MAX_GENERATORS = 512;
#endif

    /*
     * Linked-element lists:
     */
    typedef Mesh::Vertexes   Vertexes;
    typedef QList<Line *>    Lines;
    typedef QList<Polyobj *> Polyobjs;
    typedef QList<Sector *>  Sectors;

    typedef QList<BspNode *> BspNodes;
    typedef QList<BspLeaf *> BspLeafs;

    typedef QMultiMap<Sector *, SectorCluster *> SectorClusters;

#ifdef __CLIENT__
    typedef QSet<Plane *>    PlaneSet;
    typedef QSet<Surface *>  SurfaceSet;

    typedef QList<BiasSource *> BiasSources;
    typedef QList<Lumobj *>  Lumobjs;
#endif

public: /// @todo make private:
    coord_t _globalGravity; // The defined gravity for this map.
    coord_t _effectiveGravity; // The effective gravity for this map.

    int _ambientLightLevel; // Ambient lightlevel for the current map.

public:
    /**
     * Construct a new map initially configured in an editable state. Whilst
     * editable new map elements can be added, thereby allowing the map to be
     * constructed dynamically. When done editing @ref endEditing() should be
     * called to switch the map into a non-editable (i.e., playable) state.
     *
     * @param uri  Universal resource identifier to attribute to the map. For
     *             example, @code "E1M1" @endcode. Note that the scheme is
     *             presently ignored (unused).
     */
    Map(Uri const &uri = Uri());

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * To be called to initialize the dummy element arrays (which are used with
     * the DMU API), with a fixed number of @em shared dummies.
     */
    static void initDummies();

    /**
     * To be called following an engine reset to update the map state.
     */
    void update();

    /**
     * Returns the universal resource identifier (URI) attributed to the map.
     */
    Uri const &uri() const;

    /**
     * Returns the old 'unique' identifier attributed to the map.
     */
    char const *oldUniqueId() const;

    /**
     * Change the old 'unique' identifier attributed to the map.
     *
     * @param newUniqueId  New identifier to attribute.
     */
    void setOldUniqueId(char const *newUniqueId);

    /**
     * Determines if the map is from a container that has been flagged as a
     * Custom resource.
     *
     * @see P_MapIsCustom()
     */
    bool isCustom() const;

    /**
     * Returns the points which describe the boundary of the map coordinate
     * space, which, are defined by the minimal and maximal vertex coordinates
     * of the non-editable, non-polyobj line geometries).
     */
    AABoxd const &bounds() const;

    inline Vector2d origin() const {
        return Vector2d(bounds().min);
    }

    inline Vector2d dimensions() const {
        return Vector2d(bounds().max) - Vector2d(bounds().min);
    }

    /**
     * Returns the currently effective gravity multiplier for the map.
     */
    coord_t gravity() const;

    /**
     * Change the effective gravity multiplier for the map.
     *
     * @param newGravity  New gravity multiplier.
     */
    void setGravity(coord_t newGravity);

    /**
     * Returns the minimum ambient light level for the whole map.
     */
    int ambientLightLevel() const;

    /**
     * Provides access to the thinker lists for the map.
     */
    Thinkers /*const*/ &thinkers() const;

    /**
     * Provides access to the primary @ref Mesh geometry owned by the map.
     * Note that further meshes may be assigned to individual elements of
     * the map should their geometries not be representable as a manifold
     * with the primary mesh (e.g., polyobjs and BSP leaf "extra" meshes).
     */
    Mesh const &mesh() const;

    /**
     * Provides a list of all the non-editable vertexes in the map.
     */
    Vertexes const &vertexes() const;

    /**
     * Provides a list of all the non-editable lines in the map.
     */
    Lines const &lines() const;

    /**
     * Provides a list of all the non-editable polyobjs in the map.
     */
    Polyobjs const &polyobjs() const;

    /**
     * Provides a list of all the non-editable sectors in the map.
     */
    Sectors const &sectors() const;

    /**
     * Provides access to the list of BSP nodes for efficient traversal.
     */
    BspNodes const &bspNodes() const;

    /**
     * Provides access to the list of BSP leafs for efficient traversal.
     */
    BspLeafs const &bspLeafs() const;

    inline int vertexCount() const        { return vertexes().count(); }

    inline int lineCount() const          { return lines().count(); }

    inline int sideCount() const          { return lines().count() * 2; }

    inline int polyobjCount() const       { return polyobjs().count(); }

    inline int sectorCount() const        { return sectors().count(); }

    inline int bspNodeCount() const       { return bspNodes().count(); }

    inline int bspLeafCount() const       { return bspLeafs().count(); }

    /**
     * Provides access to the SectorCluster map for efficient traversal.
     */
    SectorClusters const &clusters() const;

    /**
     * Returns the total number of SectorClusters in the map.
     */
    inline int clusterCount() const { return clusters().count(); }

    /**
     * Helper function which returns the relevant side index given a @a lineIndex
     * and @a side identifier.
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
    static int toSideIndex(int lineIndex, int side);

    /**
     * Locate a LineSide in the map by it's unique @a index.
     *
     * @param index  Unique index attributed to the line side.
     *
     * @return  Pointer to the identified LineSide instance; otherwise @c 0.
     *
     * @see toSideIndex()
     */
    LineSide *sideByIndex(int index) const;

    /**
     * Locate a Polyobj in the map by it's unique in-map tag.
     *
     * @param tag  Tag associated with the polyobj to be located.
     *
     * @return  Pointer to the identified Polyobj instance; otherwise @c 0.
     */
    Polyobj *polyobjByTag(int tag) const;

    /**
     * Provides access to the entity database.
     */
    EntityDatabase &entityDatabase() const;

    /**
     * Provides access to the mobj blockmap.
     */
    Blockmap const &mobjBlockmap() const;

    /**
     * Provides access to the line blockmap.
     */
    LineBlockmap const &lineBlockmap() const;

    /**
     * Provides access to the polyobj blockmap.
     */
    Blockmap const &polyobjBlockmap() const;

    /**
     * Provides access to the BSP leaf blockmap.
     */
    Blockmap const &bspLeafBlockmap() const;

    /**
     * Returns @c true iff a BSP tree is available for the map.
     */
    bool hasBspRoot() const;

    /**
     * Returns the root element for the map's BSP tree.
     */
    MapElement &bspRoot() const;

    /**
     * Determine the BSP leaf on the back side of the BS partition that lies
     * in front of the specified point within the map's coordinate space.
     *
     * @note Always returns a valid BspLeaf although the point may not actually
     * lay within it (however it is on the same side of the space partition)!
     *
     * @param point  Map space coordinates to determine the BSP leaf for.
     *
     * @return  BspLeaf instance for that BSP node's leaf.
     */
    BspLeaf &bspLeafAt(Vector2d const &point) const;

    /**
     * @copydoc bspLeafAt()
     *
     * The test is carried out using fixed-point math for behavior compatible
     * with vanilla DOOM. Note that this means there is a maximum size for the
     * point: it cannot exceed the fixed-point 16.16 range (about 65k units).
     */
    BspLeaf &bspLeafAt_FixedPrecision(Vector2d const &point) const;

    /**
     * Determine the SectorCluster which contains @a point and which is on the
     * back side of the BS partition that lies in front of @a point.
     *
     * @param point  Map space coordinates to determine the BSP leaf for.
     *
     * @return  SectorCluster containing the specified point if any or @c 0 if
     * the clusters have not yet been built.
     */
    SectorCluster *clusterAt(Vector2d const &point) const;

    /**
     * Links a mobj into both a block and a BSP leaf based on it's (x,y).
     * Sets mobj->bspLeaf properly. Calling with flags==0 only updates
     * the BspLeaf pointer. Can be called without unlinking first.
     * Should be called AFTER mobj translation to (re-)insert the mobj.
     */
    void link(struct mobj_s &mobj, int flags);

    /**
     * Link the specified @a polyobj in any internal data structures for
     * bookkeeping purposes. Should be called AFTER Polyobj rotation and/or
     * translation to (re-)insert the polyobj.
     *
     * @param polyobj  Polyobj to be linked.
     */
    void link(Polyobj &polyobj);

    /**
     * Unlinks a mobj from everything it has been linked to. Should be called
     * BEFORE mobj translation to extract the mobj.
     *
     * @param mo  Mobj to be unlinked.
     *
     * @return  DDLINK_* flags denoting what the mobj was unlinked from
     * (in case we need to re-link).
     */
    int unlink(struct mobj_s &mobj);

    /**
     * Unlink the specified @a polyobj from any internal data structures for
     * bookkeeping purposes. Should be called BEFORE Polyobj rotation and/or
     * translation to extract the polyobj.
     *
     * @param polyobj  Polyobj to be unlinked.
     */
    void unlink(Polyobj &polyobj);

    /**
     * Given an @a emitter origin, attempt to identify the map element
     * to which it belongs.
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

    int mobjBoxIterator(AABoxd const &box,
        int (*callback) (struct mobj_s *mobj, void *context), void *context = 0) const;

    int mobjPathIterator(Vector2d const &from, Vector2d const &to,
        int (*callback) (struct mobj_s *mobj, void *context), void *context = 0) const;

    /**
     * Lines and Polyobj lines (note polyobj lines are iterated first).
     *
     * @note validCount should be incremented before calling this to begin
     * a new logical traversal. Otherwise Lines marked with a validCount
     * equal to this will be skipped over (can be used to avoid processing
     * a line multiple times during complex / non-linear traversals.
     *
     * @param flags  @ref lineIteratorFlags
     */
    int lineBoxIterator(AABoxd const &box, int flags,
        int (*callback) (Line *line, void *context), void *context = 0) const;

    /// @copydoc lineBoxIterator()
    inline int lineBoxIterator(AABoxd const &box,
        int (*callback) (Line *line, void *context), void *context = 0) const
    {
        return lineBoxIterator(box, LIF_ALL, callback, context);
    }

    /**
     * @param flags  @ref lineIteratorFlags
     */
    int linePathIterator(Vector2d const &from, Vector2d const &to, int flags,
        int (*callback) (Line *line, void *context), void *context = 0) const;

    /// @copydoc linePathIterator()
    inline int linePathIterator(Vector2d const &from, Vector2d const &to,
        int (*callback) (Line *line, void *context), void *context = 0) const
    {
        return linePathIterator(from, to, LIF_ALL, callback, context);
    }

    int bspLeafBoxIterator(AABoxd const &box,
        int (*callback) (BspLeaf *bspLeaf, void *context), void *context = 0) const;

    /**
     * @note validCount should be incremented before calling this to begin a
     * new logical traversal. Otherwise Lines marked with a validCount equal
     * to this will be skipped over (can be used to avoid processing a line
     * multiple times during complex / non-linear traversals.
     */
    int polyobjBoxIterator(AABoxd const &box,
        int (*callback) (struct polyobj_s *polyobj, void *context),
        void *context = 0) const;

    /**
     * The callback function will be called once for each line that crosses
     * trough the object. This means all the lines will be two-sided.
     */
    int mobjTouchedLineIterator(struct mobj_s *mo,
        int (*callback) (Line *, void *), void *context = 0) const;

    /**
     * Increment validCount before calling this routine. The callback function
     * will be called once for each sector the mobj is touching (totally or
     * partly inside). This is not a 3D check; the mobj may actually reside
     * above or under the sector.
     */
    int mobjTouchedSectorIterator(struct mobj_s *mo,
        int (*callback) (Sector *sector, void *context), void *context = 0) const;

    int lineTouchingMobjIterator(Line *line,
        int (*callback) (struct mobj_s *mobj, void *context), void *context = 0) const;

    /**
     * Increment validCount before using this. 'func' is called for each mobj
     * that is (even partly) inside the sector. This is not a 3D test, the
     * mobjs may actually be above or under the sector.
     *
     * (Lovely name; actually this is a combination of SectorMobjs and
     * a bunch of LineMobjs iterations.)
     */
    int sectorTouchingMobjIterator(Sector *sector,
        int (*callback) (struct mobj_s *mobj, void *context), void *context = 0) const;

#ifdef __CLIENT__
    coord_t skyFix(bool ceiling) const;

    inline coord_t skyFixFloor() const   { return skyFix(false /*the floor*/); }
    inline coord_t skyFixCeiling() const { return skyFix(true /*the ceiling*/); }

    void setSkyFix(bool ceiling, coord_t newHeight);

    inline void setSkyFixFloor(coord_t newHeight) {
        setSkyFix(false /*the floor*/, newHeight);
    }
    inline void setSkyFixCeiling(coord_t newHeight) {
        setSkyFix(true /*the ceiling*/, newHeight);
    }

    /**
     * Attempt to spawn a new (particle) generator for the map. If no free identifier
     * is available then @c 0 is returned.
     */
    Generator *newGenerator();

    void unlink(Generator &generator);

    /**
     * Iterate over all generators in the map making a callback for each. Iteration
     * ends when all generators have been processed or a callback returns non-zero.
     *
     * @param callback  Callback to make for each iteration.
     * @param context   User data to be passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int generatorIterator(int (*callback) (Generator *, void *), void *context = 0);

    /**
     * Iterate over all generators in the map which are present in the identified
     * list making a callback for each. Iteration ends when all targeted generators
     * have been processed or a callback returns non-zero.
     *
     * @param listIndex  Index of the list to traverse.
     * @param callback   Callback to make for each iteration.
     * @param context    User data to be passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int generatorListIterator(uint listIndex, int (*callback) (Generator *, void *), void *context = 0);

    /**
     * Returns the total number of @em active generators in the map.
     */
    int generatorCount() const;

    /**
     * Add a new lumobj to the map (a copy is made).
     *
     * @return  Reference to the newly added lumobj.
     *
     * @see lumobjCount()
     */
    Lumobj &addLumobj(Lumobj const &lumobj = Lumobj());

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
     * Provides a list of all the lumobjs in the map.
     */
    Lumobjs const &lumobjs() const;

    /**
     * Returns the total number of lumobjs in the map.
     */
    inline int lumobjCount() const { return lumobjs().count(); }

    /**
     * Lookup a lumobj in the map by it's unique @a index.
     */
    inline Lumobj *lumobj(int index) const { return lumobjs().at(index); }

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
    void removeBiasSource(int which);

    /**
     * Remove all bias sources from the map.
     *
     * @see removeBiasSource()
     */
    void removeAllBiasSources();

    /**
     * Provides a list of all the bias sources in the map.
     */
    BiasSources const &biasSources() const;

    /**
     * Returns the total number of bias sources in the map.
     */
    inline int biasSourceCount() const { return biasSources().count(); }

    /**
     * Returns the time in milliseconds when the current render frame began. Used
     * for interpolation purposes.
     */
    uint biasCurrentTime() const;

    /**
     * Returns the frameCount of the current render frame. Used for tracking changes
     * to bias sources/surfaces.
     */
    uint biasLastChangeOnFrame() const;

    /**
     * Lookup a bias source in the map by it's unique @a index.
     */
    BiasSource *biasSource(int index) const;

    /**
     * Finds the bias source nearest to the specified map space @a point.
     *
     * @note This result is not cached. May return @c 0 if no bias sources exist.
     */
    BiasSource *biasSourceNear(Vector3d const &point) const;

    /**
     * Lookup the unique index for the given bias @a source.
     */
    int toIndex(BiasSource const &source) const;

    /**
     * Deletes hidden, unpredictable or nulled mobjs for which we have not received
     * updates in a while.
     */
    void expireClMobjs();

    void clearClMovers();

    /**
     * Allocate a new client-side plane mover.
     *
     * @return  The new mover or @c NULL if arguments are invalid.
     */
    ClPlaneMover *newClPlaneMover(Plane &plane, coord_t dest, float speed);

    void deleteClPlaneMover(ClPlaneMover *mover);

    ClPlaneMover *clPlaneMoverFor(Plane &plane);

    /**
     * Find/create a ClPolyMover for @a polyobj.
     *
     * @param canCreate  @c true= create a new one if not found.
     */
    ClPolyMover *clPolyMoverFor(Polyobj &polyobj, bool canCreate = false);

    void deleteClPolyMover(ClPolyMover *mover);

    /**
     * Link the given @a surface in all material lists and surface sets which
     * the map maintains to improve performance. Only surfaces attributed to
     * the map will be linked (alien surfaces are ignored).
     *
     * @param surface  The surface to be linked.
     */
    void linkInMaterialLists(Surface *surface);

    /**
     * Unlink the given @a surface in all material lists and surface sets which
     * the map maintains to improve performance.
     *
     * @note The material currently attributed to the surface does not matter
     * for unlinking purposes and the surface will be unlinked from all lists
     * regardless.
     *
     * @param surface  The surface to be unlinked.
     */
    void unlinkInMaterialLists(Surface *surface);

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
     * Returns @c true iff a LightGrid has been initialized for the map.
     *
     * @see lightGrid()
     */
    bool hasLightGrid();

    /**
     * Provides access to the light grid for the map.
     *
     * @see hasLightGrid()
     */
    LightGrid &lightGrid();

    /**
     * (Re)-initialize the light grid used for smoothed sector lighting.
     *
     * If the grid has not yet been initialized block light sources are determined
     * at this time (SectorClusters must be built for this).
     *
     * If the grid has already been initialized calling this will perform a full update.
     *
     * @note Initialization may take some time depending on the complexity of the
     * map (physial dimensions, number of sectors) and should therefore be done
     * "off-line".
     */
    void initLightGrid();

    /**
     * Perform spreading of all contacts in the specified map space @a region.
     */
    void spreadAllContacts(AABoxd const &region);

protected:
    /// Observes WorldSystem FrameBegin
    void worldSystemFrameBegins(bool resetNextViewer);

#endif // __CLIENT__

public: /// @todo Make private:

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
     * Fixing the sky means that for adjacent sky sectors the lower sky
     * ceiling is lifted to match the upper sky. The raising only affects
     * rendering, it has no bearing on gameplay.
     */
    void initSkyFix();

    /**
     * Rebuild the surface material lists. To be called when a full update is
     * necessary.
     */
    void buildMaterialLists();

    /**
     * Initializes bias lighting for the map. New light sources are initialized
     * from the loaded Light definitions. Map surfaces are prepared for tracking
     * rays.
     *
     * Must be called before rendering a frame with bias lighting enabled.
     */
    void initBias();

    /**
     * Initialize the map object => BSP leaf "contact" blockmaps.
     */
    void initContactBlockmaps();

    /**
     * Spawn all generators for the map which should be initialized automatically
     * during map setup.
     */
    void initGenerators();

    /**
     * Attempt to spawn all flat-triggered particle generators for the map.
     * To be called after map setup is completed.
     *
     * @note Cannot presently be done in @ref initGenerators() as this is called
     *       during initial Map load and before any saved game has been loaded.
     */
    void spawnPlaneParticleGens();

    /**
     * Destroys all clientside clmobjs in the map. To be called when a network
     * game ends.
     */
    void clearClMobjs();

    /**
     * Find/create a client mobj with the unique identifier @a id.
     *
     * Memory layout of a client mobj:
     * - client mobj magic1 (4 bytes)
     * - engineside clmobj info
     * - client mobj magic2 (4 bytes)
     * - gameside mobj (mobjSize bytes) <- this is returned from the function
     *
     * To check whether a given mobj_t is a clmobj_t, just check the presence of
     * the client mobj magic number (by calling Cl_IsClientMobj()).
     * The clmoinfo_s can then be accessed with ClMobj_GetInfo().
     *
     * @param id  Identifier of the client mobj. Every client mobj has a unique
     *            identifier.
     *
     * @return  Pointer to the gameside mobj.
     */
    mobj_t *clMobjFor(thid_t id, bool canCreate = false) const;

    /**
     * Destroys the client mobj. Before this is called, the client mobj should be
     * unlinked from the thinker list by calling @ref thinkers().remove()
     */
    void deleteClMobj(mobj_t *mo);

    /**
     * Iterate all client mobjs, making a callback for each. Iteration ends if a
     * callback returns a non-zero value.
     *
     * @param callback  Function to callback for each client mobj.
     * @param context   Data pointer passed to the callback.
     *
     * @return  @c 0 if all callbacks return @c 0; otherwise the result of the last.
     */
    int clMobjIterator(int (*callback) (mobj_t *, void *), void *context = 0);

    /**
     * Provides readonly access to the client mobj hash.
     */
    ClMobjHash const &clMobjHash() const;
#endif // __CLIENT__

    /**
     * Returns a rich formatted, textual summary of the map's elements, suitable
     * for logging.
     */
    de::String elementSummaryAsStyledText() const;

    /**
     * Returns a rich formatted, textual summary of the map's objects, suitable
     * for logging.
     */
    de::String objectSummaryAsStyledText() const;

public:
    /*
     * Runtime map editing:
     */

    /**
     * Returns @c true iff the map is currently in an editable state.
     */
    bool isEditable() const;

    /**
     * Switch the map from editable to non-editable (i.e., playable) state,
     * incorporating any new map elements, (re)building the BSP, etc...
     *
     * @return  @c true= mode switch was completed successfully.
     */
    bool endEditing();

    /**
     * @see isEditable()
     */
    Vertex *createVertex(Vector2d const &origin,
                         int archiveIndex = MapElement::NoIndex);

    /**
     * @see isEditable()
     */
    Line *createLine(Vertex &v1, Vertex &v2, int flags = 0,
                     Sector *frontSector = 0, Sector *backSector = 0,
                     int archiveIndex = MapElement::NoIndex);

    /**
     * @see isEditable()
     */
    Polyobj *createPolyobj(Vector2d const &origin);

    /**
     * @see isEditable()
     */
    Sector *createSector(float lightLevel, Vector3f const &lightColor,
                         int archiveIndex = MapElement::NoIndex);

    /**
     * Provides a list of all the editable lines in the map.
     */
    Lines const &editableLines() const;

    /**
     * Provides a list of all the editable polyobjs in the map.
     */
    Polyobjs const &editablePolyobjs() const;

    /**
     * Provides a list of all the editable sectors in the map.
     */
    Sectors const &editableSectors() const;

    inline int editableLineCount() const    { return editableLines().count(); }

    inline int editablePolyobjCount() const { return editablePolyobjs().count(); }

    inline int editableSectorCount() const  { return editableSectors().count(); }

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_WORLD_MAP_H
