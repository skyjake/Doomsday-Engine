/** @file gamemap.h World Map.
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

#ifndef DENG_WORLD_MAP_H
#define DENG_WORLD_MAP_H

#include <QList>
#include <QSet>

#include <de/mathutil.h>

#include "EntityDatabase"
#include "m_nodepile.h"
#include "p_particle.h"
#include "Polyobj"

class Vertex;
class Line;
class Sector;
class Plane;
class BspLeaf;
class BspNode;

struct thinkerlist_s;
struct clmoinfo_s;
struct generators_s;
struct blockmap_s;

/**
 * The client mobjs are stored into a hash for quickly locating a ClMobj by its identifier.
 */
#define CLIENT_MOBJ_HASH_SIZE       (256)

/**
 * @ingroup map
 */
typedef struct cmhash_s {
    struct clmoinfo_s *first, *last;
} cmhash_t;

#define CLIENT_MAX_MOVERS          1024 // Definitely enough!

/**
 * @ingroup map
 */
typedef enum {
    CPT_FLOOR,
    CPT_CEILING
} clplanetype_t;

struct clplane_s;
struct clpolyobj_s;

/**
 * World map.
 *
 * @ingroup map
 */
class GameMap
{
public:
    typedef QList<Vertex *> Vertexes;
    typedef QList<Sector *> Sectors;
    typedef QList<Line *> Lines;
    typedef QList<Polyobj *> Polyobjs;

    typedef QList<HEdge *> HEdges;
    typedef QList<BspNode *> BspNodes;
    typedef QList<BspLeaf *> BspLeafs;

    typedef QSet<Plane *> PlaneSet;
    typedef QSet<Surface *> SurfaceSet;

public:
    de::Uri _uri;
    char _oldUniqueId[256];

    struct thinkers_s {
        int idtable[2048]; // 65536 bits telling which IDs are in use.
        ushort iddealer;

        size_t numLists;
        struct thinkerlist_s **lists;
        boolean inited;
    } thinkers;

    // Client only data:
    cmhash_t clMobjHash[CLIENT_MOBJ_HASH_SIZE];

    struct clplane_s *clActivePlanes[CLIENT_MAX_MOVERS];
    struct clpolyobj_s *clActivePolyobjs[CLIENT_MAX_MOVERS];
    // End client only data.

    Vertexes _vertexes;
    Sectors _sectors;
    Lines _lines;
    Polyobjs _polyobjs;

    EntityDatabase *entityDatabase;

public:
    struct blockmap_s *mobjBlockmap;
    struct blockmap_s *polyobjBlockmap;
    struct blockmap_s *lineBlockmap;
    struct blockmap_s *bspLeafBlockmap;

    nodepile_t mobjNodes, lineNodes; // All kinds of wacky links.
    nodeindex_t *lineLinks; // Indices to roots.

    coord_t _globalGravity; // The defined gravity for this map.
    coord_t _effectiveGravity; // The effective gravity for this map.

    int _ambientLightLevel; // Ambient lightlevel for the current map.

public:
    GameMap();
    ~GameMap();

    /**
     * This ID is the name of the lump tag that marks the beginning of map
     * data, e.g. "MAP03" or "E2M8".
     */
    de::Uri uri() const;

    /// @return  The old 'unique' identifier of the map.
    char const *oldUniqueId() const;

    /**
     * Returns the minimal and maximal boundary points for the map.
     */
    AABoxd const &bounds() const;

    /**
     * @copydoc bounds()
     *
     * Return values:
     * @param min  Coordinates for the minimal point are written here.
     * @param max  Coordinates for the maximal point are written here.
     */
    inline void bounds(coord_t *min, coord_t *max) const {
        if(min) V2d_Copy(min, bounds().min);
        if(max) V2d_Copy(max, bounds().max);
    }

    /**
     * Returns the currently effective gravity multiplier for the map.
     */
    coord_t gravity() const;

    /**
     * Change the effective gravity multiplier for the map.
     *
     * @param gravity  New gravity multiplier.
     */
    void setGravity(coord_t gravity);

    /**
     * Returns the global ambient light level for the map.
     */
    int ambientLightLevel() const;

    Vertexes const &vertexes() const { return _vertexes; }

    inline uint vertexCount() const { return vertexes().count(); }

    Lines const &lines() const { return _lines; }

    inline uint lineCount() const { return lines().count(); }

    inline uint sideCount() const { return lines().count() * 2; }

    Sectors const &sectors() const { return _sectors; }

    inline uint sectorCount() const { return sectors().count(); }

    /**
     * Locate a sector in the map by sound emitter.
     *
     * @param soundEmitter  ddmobj_base_t to search for.
     *
     * @return  Pointer to the referenced Sector instance; otherwise @c 0.
     */
    Sector *sectorBySoundEmitter(ddmobj_base_t const &soundEmitter) const;

    /**
     * Locate a sector plane in the map by sound emitter.
     *
     * @param soundEmitter  ddmobj_base_t to search for.
     *
     * @return  Pointer to the referenced Plane instance; otherwise @c 0.
     */
    Plane *planeBySoundEmitter(ddmobj_base_t const &soundEmitter) const;

    /**
     * Locate a surface in the map by sound emitter.
     *
     * @param soundEmitter  ddmobj_base_t to search for.
     *
     * @return  Pointer to the referenced Surface instance; otherwise @c 0.
     */
    Surface *surfaceBySoundEmitter(ddmobj_base_t const &soundEmitter) const;

    /**
     * Provides access to the list of polyobjs for efficient traversal.
     */
    Polyobjs const &polyobjs() const { return _polyobjs; }

    /**
     * Returns the total number of Polyobjs in the map.
     */
    inline uint polyobjCount() const { return polyobjs().count(); }

    /**
     * Locate a polyobj in the map by unique in-map tag.
     *
     * @param tag  Tag associated with the polyobj to be located.
     * @return  Pointer to the referenced polyobj instance; otherwise @c 0.
     */
    Polyobj *polyobjByTag(int tag) const;

    /**
     * Locate a polyobj in the map by mobj base.
     *
     * @param ddMobjBase  Base mobj to search for.
     *
     * @return  Pointer to the referenced polyobj instance; otherwise @c 0.
     */
    Polyobj *polyobjByBase(ddmobj_base_t const &ddMobjBase) const;

    /**
     * Returns the root element for the map's BSP tree.
     */
    de::MapElement *bspRoot() const;

    /**
     * Provides access to the list of half-edges for efficient traversal.
     */
    HEdges const &hedges() const;

    /**
     * Returns the total number of HEdges in the map.
     */
    inline uint hedgeCount() const { return hedges().count(); }

    /**
     * Provides access to the list of BSP nodes for efficient traversal.
     */
    BspNodes const &bspNodes() const;

    /**
     * Returns the total number of BspNodes in the map.
     */
    inline uint bspNodeCount() const { return bspNodes().count(); }

    /**
     * Provides access to the list of BSP leafs for efficient traversal.
     */
    BspLeafs const &bspLeafs() const;

    /**
     * Returns the total number of BspLeafs in the map.
     */
    inline uint bspLeafCount() const { return bspLeafs().count(); }

    /**
     * Determine the BSP leaf on the back side of the BS partition that lies in front
     * of the specified point within the map's coordinate space.
     *
     * @note Always returns a valid BspLeaf although the point may not actually lay
     *       within it (however it is on the same side of the space partition)!
     *
     * @param point  XY coordinates of the point to test.
     *
     * @return     BspLeaf instance for that BSP node's leaf.
     */
    BspLeaf *bspLeafAtPoint(const_pvec2d_t point) const;

    /**
     * @copydoc bspLeafAtPoint()
     *
     * @param x  X coordinate of the point to test.
     * @param y  Y coordinate of the point to test.
     * @return   BspLeaf instance for that BSP node's leaf.
     */
    inline BspLeaf *bspLeafAtPoint(coord_t x, coord_t y) const {
        coord_t point[2] = { x, y };
        return bspLeafAtPoint(point);
    }

    int mobjsBoxIterator(AABoxd const &box,
        int (*callback) (struct mobj_s *, void *), void *parameters = 0) const;

    int linesBoxIterator(AABoxd const &box,
        int (*callback) (Line *, void *), void *parameters = 0) const;

    int polyobjLinesBoxIterator(AABoxd const &box,
        int (*callback) (Line *, void *), void *parameters = 0) const;

    /**
     * Lines and Polyobj lines (note polyobj lines are iterated first).
     *
     * @note validCount should be incremented before calling this to begin
     * a new logical traversal. Otherwise Lines marked with a validCount
     * equal to this will be skipped over (can be used to avoid processing
     * a line multiple times during complex / non-linear traversals.
     */
    int allLinesBoxIterator(AABoxd const &box,
        int (*callback) (Line *, void *), void *parameters = 0) const;

    int bspLeafsBoxIterator(AABoxd const &box, Sector *sector,
        int (*callback) (BspLeaf *, void *), void *parameters = 0) const;

    /**
     * @note validCount should be incremented before calling this to begin a
     * new logical traversal. Otherwise Lines marked with a validCount equal
     * to this will be skipped over (can be used to avoid processing a line
     * multiple times during complex / non-linear traversals.
     */
    int polyobjsBoxIterator(AABoxd const &box,
        int (*callback) (struct polyobj_s *, void *), void *parameters = 0) const;

    /**
     * Retrieve an immutable copy of the LOS trace line state.
     *
     * @todo GameMap should not own this data.
     */
    divline_t const &traceLine() const;

    /**
     * Retrieve an immutable copy of the LOS TraceOpening state.
     *
     * @todo GameMap should not own this data.
     */
    TraceOpening const &traceOpening() const;

    /**
     * Update the TraceOpening state for according to the opening defined by the
     * inner-minimal planes heights which intercept @a line
     *
     * If @a line is not owned by the map this is a no-op.
     *
     * @todo GameMap should not own this data.
     *
     * @param line  Map line to configure the opening for.
     */
    void setTraceOpening(Line &line);

    /**
     * Trace a line between @a from and @a to, making a callback for each
     * interceptable object linked within Blockmap cells which cover the path
     * this defines.
     */
    int pathTraverse(const_pvec2d_t from, const_pvec2d_t to, int flags,
                     traverser_t callback, void *parameters = 0);

    /**
     * @copydoc pathTraverse()
     *
     * @param fromX         X axis map space coordinate for the path origin.
     * @param fromY         Y axis map space coordinate for the path origin.
     * @param toX           X axis map space coordinate for the path destination.
     * @param toY           Y axis map space coordinate for the path destination.
     */
    inline int pathTraverse(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY,
                            int flags, traverser_t callback, void *parameters = 0)
    {
        coord_t from[2] = { fromX, fromY };
        coord_t to[2]   = { toX, toY };
        return pathTraverse(from, to, flags, callback, parameters);
    }

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
     * Link the specified @a bspLeaf in internal data structures for
     * bookkeeping purposes.
     *
     * @todo Does this really need to be public? -ds
     *
     * @param bspLeaf  BspLeaf to be linked.
     */
    void linkBspLeaf(BspLeaf &bspLeaf);

    /**
     * Link the specified @a line in any internal data structures for
     * bookkeeping purposes.
     *
     * @todo Does this really need to be public? -ds
     *
     * @param line  Line to be linked.
     */
    void linkLine(Line &line);

    /**
     * Link the specified @a mobj in any internal data structures for
     * bookkeeping purposes. Should be called AFTER mobj translation to
     * (re-)insert the mobj.
     *
     * @param mobj  Mobj to be linked.
     */
    void linkMobj(struct mobj_s &mobj);

    /**
     * Unlink the specified @a mobj from any internal data structures for
     * bookkeeping purposes. Should be called BEFORE mobj translation to
     * extract the mobj.
     *
     * @param mobj  Mobj to be unlinked.
     */
    bool unlinkMobj(struct mobj_s &mobj);

    /**
     * Link the specified @a polyobj in any internal data structures for
     * bookkeeping purposes. Should be called AFTER Polyobj rotation and/or
     * translation to (re-)insert the polyobj.
     *
     * @param polyobj  Polyobj to be linked.
     */
    void linkPolyobj(Polyobj &polyobj);

    /**
     * Unlink the specified @a polyobj from any internal data structures for
     * bookkeeping purposes. Should be called BEFORE Polyobj rotation and/or
     * translation to extract the polyobj.
     *
     * @param polyobj  Polyobj to be unlinked.
     */
    void unlinkPolyobj(Polyobj &polyobj);

    /**
     * Retrieve a pointer to the Generators collection for the map. If no collection
     * has yet been constructed a new empty collection will be initialized.
     *
     * @return  Generators collection for the map.
     */
    struct generators_s *generators();

#ifdef __CLIENT__
    /// @todo Should be private?
    void initClMobjs();

    /**
     * To be called when the client is shut down.
     * @todo Should be private?
     */
    void destroyClMobjs();

    /**
     * Deletes hidden, unpredictable or nulled mobjs for which we have not received
     * updates in a while.
     */
    void expireClMobjs();

    /**
     * Reset the client status. To be called when the map changes.
     */
    void clMobjReset();

    /**
     * Iterate the client mobj hash, exec the callback on each. Abort if callback
     * returns non-zero.
     *
     * @param callback  Function to callback for each client mobj.
     * @param context   Data pointer passed to the callback.
     *
     * @return  @c 0 if all callbacks return @c 0; otherwise the result of the last.
     */
    int clMobjIterator(int (*callback) (struct mobj_s *, void *), void *context);

    /**
     * Allocate a new client-side plane mover.
     *
     * @return  The new mover or @c NULL if arguments are invalid.
     */
    struct clplane_s *newClPlane(uint sectornum, clplanetype_t type, coord_t dest, float speed);

    /**
     * Returns the set of decorated surfaces for the map.
     */
    SurfaceSet /*const*/ &decoratedSurfaces();

    /**
     * Returns the set of glowing surfaces for the map.
     */
    SurfaceSet /*const*/ &glowingSurfaces();

#endif // __CLIENT__

    /**
     * $smoothmatoffset: interpolate the visual offset.
     */
    void lerpScrollingSurfaces(bool resetNextViewer = false);

    /**
     * $smoothmatoffset: Roll the surface material offset tracker buffers.
     */
    void updateScrollingSurfaces();

    /**
     * Returns the set of scrolling surfaces for the map.
     */
    SurfaceSet /*const*/ &scrollingSurfaces();

    /**
     * $smoothplane: interpolate the visual offset.
     */
    void lerpTrackedPlanes(bool resetNextViewer = false);

    /**
     * $smoothplane: Roll the height tracker buffers.
     */
    void updateTrackedPlanes();

    /**
     * Returns the set of tracked planes for the map.
     */
    PlaneSet /*const*/ &trackedPlanes();

public: /// @todo Replace with object level methods:
    /**
     * Lookup the in-map unique index for @a vertex.
     *
     * @param vtx  Vertex to lookup.
     * @return  Unique index for the Vertex else @c -1 if not present.
     */
    int vertexIndex(Vertex const *vtx) const;

    /**
     * Lookup the in-map unique index for @a line.
     *
     * @param line  Line to lookup.
     * @return  Unique index for the Line else @c -1 if not present.
     */
    int lineIndex(Line const *line) const;

    /**
     * Lookup the in-map unique index for @a side.
     *
     * @param side  Line::Side to lookup.
     * @return  Unique index for the Line::Side else @c -1 if not present.
     */
    int sideIndex(Line::Side const *side) const;

    /**
     * Returns a pointer to the Line::Side associated with the specified @a index;
     * otherwise @c 0.
     */
    Line::Side *sideByIndex(int index) const;

    /**
     * Lookup the in-map unique index for @a sector.
     *
     * @param sector  Sector to lookup.
     * @return  Unique index for the Sector else @c -1 if not present.
     */
    int sectorIndex(Sector const *sector) const;

    /**
     * Lookup the in-map unique index for @a bspLeaf.
     *
     * @param bspLeaf  BspLeaf to lookup.
     * @return  Unique index for the BspLeaf else @c -1 if not present.
     */
    int bspLeafIndex(BspLeaf const *bspLeaf) const;

    /**
     * Lookup the in-map unique index for @a hedge.
     *
     * @param hedge  HEdge to lookup.
     * @return  Unique index for the HEdge else @c -1 if not present.
     */
    int hedgeIndex(HEdge const *hedge) const;

    /**
     * Lookup the in-map unique index for @a node.
     *
     * @param bspNode  BspNode to lookup.
     * @return  Unique index for the BspNode else @c -1 if not present.
     */
    int bspNodeIndex(BspNode const *bspNode) const;

public: /// @todo Make private:

    void finishMapElements();

    /**
     * @pre Axis-aligned bounding boxes of all Sectors must be initialized.
     */
    void updateBounds();

    /**
     * Construct an initial (empty) Mobj Blockmap for this map.
     *
     * @param min  Minimal coordinates for the map.
     * @param max  Maximal coordinates for the map.
     */
    void initMobjBlockmap(const_pvec2d_t min, const_pvec2d_t max);

    /**
     * Construct an initial (empty) Line Blockmap for this map.
     *
     * @param min  Minimal coordinates for the map.
     * @param max  Maximal coordinates for the map.
     */
    void initLineBlockmap(const_pvec2d_t min, const_pvec2d_t max);

    /**
     * Construct an initial (empty) BspLeaf Blockmap for this map.
     *
     * @param min  Minimal coordinates for the map.
     * @param max  Maximal coordinates for the map.
     */
    void initBspLeafBlockmap(const_pvec2d_t min, const_pvec2d_t max);

    /**
     * Construct an initial (empty) Polyobj Blockmap for this map.
     *
     * @param min  Minimal coordinates for the map.
     * @param max  Maximal coordinates for the map.
     */
    void initPolyobjBlockmap(const_pvec2d_t min, const_pvec2d_t max);

    /**
     * Initialize the node piles and link rings. To be called after map load.
     */
    void initNodePiles();

    /**
     * Initialize all polyobjs in the map. To be called after map load.
     */
    void initPolyobjs();

    /**
     * Fixing the sky means that for adjacent sky sectors the lower sky
     * ceiling is lifted to match the upper sky. The raising only affects
     * rendering, it has no bearing on gameplay.
     */
    void initSkyFix();

#ifdef __CLIENT__
    void buildSurfaceLists();
#endif

    bool buildBsp();

    /**
     * To be called in response to a Material property changing which may
     * require updating any map surfaces which are presently using it.
     *
     * @todo Replace with a de::Observers-based mechanism.
     */
    void updateSurfacesOnMaterialChange(Material &material);

private:
    DENG2_PRIVATE(d)
};

/**
 * Have the thinker lists been initialized yet?
 * @param map       GameMap instance.
 */
boolean GameMap_ThinkerListInited(GameMap *map);

/**
 * Init the thinker lists.
 *
 * @param map       GameMap instance.
 * @param flags     @c 0x1 = Init public thinkers.
 *                  @c 0x2 = Init private (engine-internal) thinkers.
 */
void GameMap_InitThinkerLists(GameMap *map, byte flags);

/**
 * Iterate the list of thinkers making a callback for each.
 *
 * @param map  GameMap instance.
 * @param thinkFunc  If not @c NULL, only make a callback for thinkers
 *                   whose function matches this.
 * @param flags  Thinker filter flags.
 * @param callback  The callback to make. Iteration will continue
 *                  until a callback returns a non-zero value.
 * @param context  Passed to the callback function.
 */
int GameMap_IterateThinkers(GameMap *map, thinkfunc_t thinkFunc, byte flags,
    int (*callback) (thinker_t *th, void *), void *context);

/**
 * @param map  GameMap instance.
 * @param thinker  Thinker to be added.
 * @param makePublic  @c true = @a thinker will be visible publically
 *                    via the Doomsday public API thinker interface(s).
 */
void GameMap_ThinkerAdd(GameMap *map, thinker_t *thinker, boolean makePublic);

/**
 * Deallocation is lazy -- it will not actually be freed until its
 * thinking turn comes up.
 *
 * @param map   GameMap instance.
 */
void GameMap_ThinkerRemove(GameMap *map, thinker_t *thinker);

/**
 * Locates a mobj by it's unique identifier in the map.
 *
 * @param map   GameMap instance.
 * @param id    Unique id of the mobj to lookup.
 */
struct mobj_s *GameMap_MobjByID(GameMap *map, int id);

/**
 * @param map   GameMap instance.
 * @param id    Thinker id to test.
 */
boolean GameMap_IsUsedMobjID(GameMap* map, thid_t id);

/**
 * @param map   GameMap instance.
 * @param id    New thinker id.
 * @param inUse In-use state of @a id. @c true = the id is in use.
 */
void GameMap_SetMobjID(GameMap *map, thid_t id, boolean inUse);

// The current map.
DENG_EXTERN_C GameMap *theMap;

#endif // DENG_WORLD_MAP_H
