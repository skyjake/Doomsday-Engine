/** @file map.h  Base for world maps.
 * @ingroup world
 *
 * @authors Copyright © 2014-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_MAP_H
#define LIBDOOMSDAY_WORLD_MAP_H

#include "../libdoomsday.h"
#include "../res/mapmanifest.h"
#include "mapelement.h"
#include "bspnode.h"
#include "api_mapedit.h"

#include <de/id.h>
#include <de/observers.h>
#include <de/reader.h>
#include <de/writer.h>

class EntityDatabase;
struct polyobj_s;

namespace mesh { class Mesh; }
namespace network { class MapOutlinePacket; }

namespace world {

class Blockmap;
class BspLeaf;
class IThinkerMapping;
class Line;
class LineBlockmap;
class LineSide;
class Sky;
class Subsector;
class Surface;
class Thinkers;

/**
 * Base class for world maps.
 */
class LIBDOOMSDAY_PUBLIC Map
{
public:
    static void consoleRegister();

    /// No resource manifest is associated with the map. @ingroup errors
    DE_ERROR(MissingResourceManifestError);

    /// Required map object is missing. @ingroup errors
    DE_ERROR(MissingObjectError);

    /// Required map element is missing. @ingroup errors
    DE_ERROR(MissingElementError);

    /// Base error for runtime map editing errors. @ingroup errors
    DE_ERROR(EditError);

    /// Required blockmap is missing. @ingroup errors
    DE_ERROR(MissingBlockmapError);

    /// Required BSP data is missing. @ingroup errors
    DE_ERROR(MissingBspTreeError);

    /// Thrown when the referenced subsector is missing/unknown. @ingroup errors
    DE_ERROR(MissingSubsectorError);

    /// Required thinker lists are missing. @ingroup errors
    DE_ERROR(MissingThinkersError);

    /// Notified when a one-way window construct is first found.
    DE_AUDIENCE(OneWayWindowFound, void oneWayWindowFound(Line &line, Sector &backFacingSector))

    /// Notified when an unclosed sector is first found.
    DE_AUDIENCE(UnclosedSectorFound, void unclosedSectorFound(Sector &sector, const de::Vec2d &nearPoint))

    /// Notified when the map is about to be deleted.
    DE_AUDIENCE(Deletion, void mapBeingDeleted(const Map &map))

public:
    /**
     * @param manifest  Resource manifest for the map (Can be set later, @ref setDef).
     */
    explicit Map(res::MapManifest *manifest = nullptr);

    virtual ~Map();
    
    /**
     * Delete all elements of the map, returning it to an empty state like the map
     * would exist after construction.
     */
    void clearData();

    de::String id() const;

    /**
     * Returns @c true if a resource manifest is associated with the map.
     *
     * @see manifest(), setManifest()
     */
    bool hasManifest() const;

    /**
     * Returns the resource manifest for the map.
     *
     * @see hasManifest(), setManifest()
     */
    res::MapManifest &manifest() const;

    /**
     * Change the associated resource manifest to @a newManifest.
     *
     * @see hasManifest(), manifest()
     */
    void setManifest(res::MapManifest *newManifest);

    res::Uri uri() const;

    /**
     * Returns the effective map-info definition Record for the map.
     *
     * @see World::mapInfoForMapUri()
     */
    const de::Record &mapInfo() const;

    /**
     * Returns the points which describe the boundary of the map coordinate space, which,
     * are defined by the minimal and maximal vertex coordinates of the non-editable,
     * non-polyobj line geometries).
     */
    const AABoxd &bounds() const;

    inline de::Vec2d origin    () const {
        return de::Vec2d(bounds().min);
    }

    inline de::Vec2d dimensions() const {
        return de::Vec2d(bounds().max) - de::Vec2d(bounds().min);
    }

    /**
     * Returns the minimum ambient light level for the whole map.
     */
    int ambientLightLevel() const;

    /**
     * Returns the currently effective gravity multiplier for the map.
     */
    double gravity() const;

    /**
     * Change the effective gravity multiplier for the map.
     *
     * @param newGravity  New gravity multiplier.
     */
    void setGravity(double newGravity);

    /**
     * Provides access to the entity database.
     */
    EntityDatabase &entityDatabase() const;
    
    /**
     * To be called following an engine reset to update the map state.
     */
    virtual void update();

    virtual void serializeInternalState(de::Writer &to) const;
    virtual void deserializeInternalState(de::Reader &from, const IThinkerMapping &);

    DE_CAST_METHODS()

public:

//- Lines and LineSides --------------------------------------------------------------------------

    /**
     * Returns the total number of Lines in the map.
     */
    int lineCount() const;

    /**
     * Lookup a Line in the map by it's unique @a index.
     */
    Line &line   (int index) const;
    Line *linePtr(int index) const;

    /**
     * Iterate Lines in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each Line.
     */
    de::LoopResult forAllLines(const std::function<de::LoopResult (Line &)>& callback) const;

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
    de::LoopResult forAllLinesInBox(const AABoxd &box, int flags,
                                    const std::function<de::LoopResult (Line &)> &callback) const;

    /**
     * @overload
     */
    inline de::LoopResult forAllLinesInBox(const AABoxd &box,
                                           const std::function<de::LoopResult (Line &)> &callback) const {
        return forAllLinesInBox(box, LIF_ALL, callback);
    }

    /**
     * The callback function will be called once for each Line that crosses the object.
     * This means all the lines will be two-sided.
     *
     * @param callback  Function to call for each Line.
     */
    de::LoopResult forAllLinesTouchingMobj(struct mobj_s &mob,
                                           const std::function<de::LoopResult (Line &)> &callback) const;

    /**
     * Returns the total number of Line::Sides in the map.
     */
    inline int sideCount() const { return lineCount() * 2; }

    /**
     * Lookup a LineSide in the map by it's unique @a index.
     *
     * @see toSideIndex()
     */
    LineSide &side   (int index) const;
    LineSide *sidePtr(int index) const;

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
    static int toSideIndex(int lineIndex, int side);

//- Map-objects -----------------------------------------------------------------------------------

    de::LoopResult forAllMobjsTouchingLine(
        Line &                                                line,
        const std::function<de::LoopResult(struct mobj_s &)> &callback) const;

    /**
     * @important Increment validCount before calling this!
     *
     * Iterate mobj_ts in the map that are "inside" the specified sector (even partially)
     * on the X|Y plane (mobj_t Z origin and Plane heights not considered).
     *
     * @param sector    Sector requirement (only consider map-objects "touching" this).
     * @param callback  Function to call for each mobj_t.
     */
    de::LoopResult forAllMobjsTouchingSector(
        Sector &                                              sector,
        const std::function<de::LoopResult(struct mobj_s &)> &callback) const;

    /**
     * Links a mobj into both a block and a BSP leaf based on it's (x,y). Sets mobj->bspLeaf
     * properly. Calling with flags==0 only updates the BspLeaf pointer. Can be called without
     * unlinking first. Should be called AFTER mobj translation to (re-)insert the mobj.
     */
    virtual void link(struct mobj_s &mobj, int flags);

    /**
     * Unlinks a map-object from everything it has been linked to. Should be called BEFORE
     * mobj translation to extract the mobj.
     *
     * @param mob  Map-object to be unlinked.
     *
     * @return  DDLINK_* flags denoting what the mobj was unlinked from (in case we need
     * to re-link).
     */
    virtual int unlink(struct mobj_s &mob);

//- Vertices --------------------------------------------------------------------------------------

    /**
     * Returns the total number of Vertices in the map.
     */
    int vertexCount() const;

    /**
     * Lookup a Vertex in the map by it's unique @a index.
     */
    Vertex &vertex   (int index) const;
    Vertex *vertexPtr(int index) const;

    /**
     * Iterate Vertices in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each Vertex.
     */
    de::LoopResult forAllVertices(const std::function<de::LoopResult (Vertex &)>& callback) const;

//- Polyobjects ----------------------------------------------------------------------------------

    using Polyobj = struct polyobj_s;

    /**
     * Returns the total number of Polyobjs in the map.
     */
    int polyobjCount() const;

    /**
     * Lookup a Polyobj in the map by it's unique @a index.
     */
    Polyobj &polyobj   (int index) const;
    Polyobj *polyobjPtr(int index) const;

    /**
     * Iterate Polyobjs in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each Polyobj.
     */
    de::LoopResult forAllPolyobjs(const std::function<de::LoopResult (Polyobj &)>& callback) const;

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
    int sectorCount() const;

    /**
     * Lookup a Sector in the map by it's unique @a index.
     */
    Sector &sector   (int index) const;
    Sector *sectorPtr(int index) const;

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
    de::LoopResult forAllSectorsTouchingMobj(struct mobj_s &mob, const std::function<de::LoopResult (Sector &)>& callback) const;

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
    Subsector *subsectorAt(const de::Vec2d &point) const;

    /**
     * Returns the logical sky for the map.
     */
    Sky &sky() const;

//- Subspaces ------------------------------------------------------------------

    /**
     * Returns the total number of subspaces in the map.
     */
    int subspaceCount() const;

    /**
     * Lookup a Subspace in the map by it's unique @a index.
     */
    ConvexSubspace &subspace   (int index) const;
    ConvexSubspace *subspacePtr(int index) const;

    /**
     * Iterate ConvexSubspaces in the map, making a function @a callback for each.
     *
     * @param callback  Function to call for each ConvexSubspace.
     */
    de::LoopResult forAllSubspaces(const std::function<de::LoopResult (ConvexSubspace &)>& callback) const;

//- Data structures ------------------------------------------------------------

    /**
     * Provides access to the primary @ref Mesh geometry owned by the map. Note that further
     * meshes may be assigned to individual elements of the map should their geometries
     * not be representable as a manifold with the primary mesh (e.g., Polyobjs and BspLeaf
     * "extra" meshes).
     */
    const mesh::Mesh &mesh() const;

    /**
     * Provides access to the line blockmap.
     */
    const LineBlockmap &lineBlockmap() const;

    /**
     * Provides access to the mobj blockmap.
     */
    const Blockmap &mobjBlockmap() const;

    /**
     * Provides access to the polyobj blockmap.
     */
    const Blockmap &polyobjBlockmap() const;

    /**
     * Provides access to the convex subspace blockmap.
     */
    const Blockmap &subspaceBlockmap() const;

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
    const BspTree &bspTree() const;

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
    BspLeaf &bspLeafAt(const de::Vec2d &point) const;

    /**
     * @copydoc bspLeafAt()
     *
     * The test is carried out using fixed-point math for behavior compatible with vanilla
     * DOOM. Note that this means there is a maximum size for the point: it cannot exceed
     * the fixed-point 16.16 range (about 65k units).
     */
    BspLeaf &bspLeafAt_FixedPrecision(const de::Vec2d &point) const;

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
    bool identifySoundEmitter(const ddmobj_base_t &emitter, Sector **sector,
                              Polyobj **poly, Plane **plane, Surface **surface) const;

    /**
     * Returns a rich formatted, textual summary of the map's elements, suitable for logging.
     */
    de::String elementSummaryAsStyledText() const;

    /**
     * Returns a rich formatted, textual summary of the map's objects, suitable for logging.
     */
    virtual de::String objectSummaryAsStyledText() const;

    de::String objectsDescription() const;

    void restoreObjects(const de::Info &              objState,
                        const world::IThinkerMapping &thinkerMapping) const;

//- Editing --------------------------------------------------------------------

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
    virtual bool endEditing();

    /**
     * @see isEditable()
     */
    Vertex *createVertex(const de::Vec2d &origin,
                         int archiveIndex = MapElement::NoIndex);

    /**
     * @see isEditable()
     */
    Line *createLine(Vertex &v1, Vertex &v2, int flags = 0,
                     Sector *frontSector = nullptr, Sector *backSector = nullptr,
                     int archiveIndex = MapElement::NoIndex);

    /**
     * @see isEditable()
     */
    Polyobj *createPolyobj(const de::Vec2d &origin);

    /**
     * @see isEditable()
     */
    Sector *createSector(float lightLevel, const de::Vec3f &lightColor,
                         int archiveIndex = MapElement::NoIndex,
                         const struct de_api_sector_hacks_s *hacks = nullptr);

    virtual void applySectorHacks(Sector &sector, const struct de_api_sector_hacks_s *hacks);

    /**
     * Provides a list of all the editable lines in the map.
     */
    typedef de::List<Line *> Lines;
    const Lines &editableLines() const;

    /**
     * Provides a list of all the editable polyobjs in the map.
     */
    typedef de::List<Polyobj *> Polyobjs;
    const Polyobjs &editablePolyobjs() const;

    /**
     * Provides a list of all the editable sectors in the map.
     */
    typedef de::List<Sector *> Sectors;
    const Sectors &editableSectors() const;

    inline int editableLineCount   () const { return editableLines   ().count(); }
    inline int editablePolyobjCount() const { return editablePolyobjs().count(); }
    inline int editableSectorCount () const { return editableSectors ().count(); }

//- Multiplayer -------------------------------------------------------------------------

    void initMapOutlinePacket(network::MapOutlinePacket &packet);

//- Dummy Map Elements ----------------------------------------------------------------------------

    /**
     * Initialize the dummy map element arrays (which are used with the DMU API),
     * with a fixed number of @em shared dummies.
     */
    static void initDummyElements();

    static void *createDummyElement(int type, void *extraData);

    /**
     * Determines the type of a dummy object.
     */
    static int dummyElementType(const void *mapElement);

    static void destroyDummyElement(void *mapElement);

    static void *dummyElementExtraData(void *mapElement);

public:  /// @todo Most of the following should be private:

    /**
     * Initialize the node piles and link rings. To be called after map load.
     */
    void initNodePiles();

    /**
     * Initialize all polyobjs in the map. To be called after map load.
     */
    void initPolyobjs();
    
private:
    DE_PRIVATE(d)
};

typedef de::duint16 InternalSerialId;

// Identifiers for serialized internal state.
enum InternalSerialIds
{
    THINKER_DATA                = 0x0001,
    MOBJ_THINKER_DATA           = 0x0002,
    CLIENT_MOBJ_THINKER_DATA    = 0x0003,
    STATE_ANIMATOR              = 0x0004,
};

}  // namespace world

#endif  // LIBDOOMSDAY_WORLD_MAP_H
