/** @file p_objlink.cpp World map object => BSP leaf contact blockmap.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_graphics.h"
//#include "de_misc.h"
#include "de_play.h"
#include "de_defs.h"

#include "Face"

#include "world/map.h"
#include "BspLeaf"

#include "MaterialSnapshot"
#include "WallEdge"

#include "gridmap.h"

#include "world/p_objlink.h"

using namespace de;

#define BLOCK_SIZE              128

enum objtype_t
{
    OT_MOBJ       = 0,
    OT_LUMOBJ     = 1,
    NUM_OBJ_TYPES
};

#define VALID_OBJTYPE(val) ((val) >= OT_MOBJ && (val) < NUM_OBJ_TYPES)

/// @todo Obviously, polymorphism is a better solution.
struct objlink_t
{
    objlink_t *nextInBlock; /// Next in the same obj block, or NULL.
    objlink_t *nextUsed;
    objlink_t *next; /// Next in list of ALL objlinks.
    objtype_t _type;
    void *_object;

    objtype_t type() const
    {
        return _type;
    }

    void *objectPtr() const
    {
        return _object;
    }

    template <class ObjectType>
    ObjectType &objectAs() const {
        DENG2_ASSERT(_object != 0);
        return *static_cast<ObjectType *>(_object);
    }

    /**
     * Returns a copy of the linked object's origin in map space.
     */
    Vector3d objectOrigin() const
    {
        switch(_type)
        {
        case OT_LUMOBJ: return objectAs<Lumobj>().origin();
        case OT_MOBJ:   return Mobj_Origin(objectAs<mobj_t>());

        default:
            DENG2_ASSERT(false);
            return Vector3d();
        }
    }

    /**
     * Returns the linked object's radius in map space.
     */
    ddouble objectRadius() const
    {
        switch(_type)
        {
        case OT_LUMOBJ: return objectAs<Lumobj>().radius();
        case OT_MOBJ:   return Mobj_VisualRadius(&objectAs<mobj_t>());

        default:
            DENG2_ASSERT(false);
            return 0;
        }
    }

    /**
     * Returns an axis-aligned bounding box for the linked object in map space.
     */
    AABoxd objectAABox() const
    {
        Vector2d const origin = objectOrigin();
        ddouble const radius  = objectRadius();
        return AABoxd(origin.x - radius, origin.y - radius,
                      origin.x + radius, origin.y + radius);
    }

    /**
     * Returns the BSP leaf at the linked object's origin in map space.
     */
    BspLeaf &objectBspLeafAtOrigin() const
    {
        switch(_type)
        {
        case OT_LUMOBJ: return objectAs<Lumobj>().bspLeafAtOrigin();
        case OT_MOBJ:   return Mobj_BspLeafAtOrigin(objectAs<mobj_t>());

        default:
            throw Error("oblink_t::objectBspLeafAtOrigin", "Invalid type");
        }
    }
};

class objlinkblockmap_t
{
public:
    struct CellData
    {
        objlink_t *head;
        bool doneSpread; ///< Used to prevent repeated per-frame processing of a block.

        void unlinkAll()
        {
            head = 0;
            doneSpread = false;
        }
    };

public:
    objlinkblockmap_t(AABoxd const &bounds, uint blockSize = 128)
        : _origin(bounds.min),
          _gridmap(Vector2ui(de::ceil((bounds.maxX - bounds.minX) / ddouble( blockSize )),
                             de::ceil((bounds.maxY - bounds.minY) / ddouble( blockSize ))),
                   sizeof(CellData), PU_MAPSTATIC)
    {}

    /**
     * Returns the origin of the blockmap in map space.
     */
    Vector2d const &origin() const
    {
        return _origin;
    }

    /**
     * Determines in which blockmap cell the specified map point lies. If the
     * these coordinates are outside the blockmap they will be clamped within
     * the valid range.
     *
     * @param retAdjusted  If specified, whether or not the @a point coordinates
     *                     specified had to be adjusted is written back here.
     *
     * @return  The determined blockmap cell.
     */
    GridmapCell toCell(Vector2d const &point, bool *retAdjusted = 0) const
    {
        Vector2d const max = _origin + _gridmap.dimensions() * BLOCK_SIZE;

        GridmapCell cell;
        bool adjusted = false;
        if(point.x < _origin.x)
        {
            cell.x = 0;
            adjusted = true;
        }
        else if(point.x >= max.x)
        {
            cell.x = _gridmap.width() - 1;
            adjusted = true;
        }
        else
        {
            cell.x = toX(point.x);
        }

        if(point.y < _origin.y)
        {
            cell.y = 0;
            adjusted = true;
        }
        else if(point.y >= max.y)
        {
            cell.y = _gridmap.height() - 1;
            adjusted = true;
        }
        else
        {
            cell.y = toY(point.y);
        }

        if(retAdjusted) *retAdjusted = adjusted;

        return cell;
    }

    GridmapCellBlock toCellBlock(AABoxd const &box)
    {
        GridmapCellBlock cellBlock;
        cellBlock.min = toCell(box.min);
        cellBlock.max = toCell(box.max);
        return cellBlock;
    }

    /// @pre  Coordinates held by @a blockXY are within valid range.
    void link(GridmapCell cell, objlink_t *link)
    {
        if(!link) return;
        CellData *block = data(cell, true/*can allocate a block*/);
        link->nextInBlock = block->head;
        block->head = link;
    }

    // Clear all the contact list heads and spread flags.
    void unlinkAll()
    {
        iterate(unlinkAllWorker);
    }

    CellData *data(GridmapCell const &cell, bool canAlloc = false)
    {
        return static_cast<CellData *>(_gridmap.cellData(cell, canAlloc));
    }

private:
    inline uint toX(ddouble x) const
    {
        DENG2_ASSERT(x >= _origin.x);
        return (x - _origin.x) / ddouble( BLOCK_SIZE );
    }

    inline uint toY(ddouble y) const
    {
        DENG2_ASSERT(y >= _origin.y);
        return (y - _origin.y) / ddouble( BLOCK_SIZE );
    }

    static int unlinkAllWorker(void *obj, void *context)
    {
        DENG2_UNUSED(context);
        static_cast<CellData *>(obj)->unlinkAll();
        return false; // Continue iteration.
    }

    int iterate(int (*callback) (void *obj, void *context), void *context = 0)
    {
        return _gridmap.iterate(callback, context);
    }

    Vector2d _origin; ///< Origin in map space.
    Gridmap _gridmap;
};

// Each linked object type uses a separate blockmap.
static objlinkblockmap_t *blockmaps[NUM_OBJ_TYPES];

struct contactfinderparams_t
{
    objlink_t *objlink;
    AABoxd objAABox;
};

struct objcontact_t
{
    objcontact_t *next; /// Next in the BSP leaf.
    objcontact_t *nextUsed; /// Next used contact.
    void *obj;
};

struct objcontactlist_t
{
    objcontact_t *head[NUM_OBJ_TYPES];
};

static objlink_t *objlinks;
static objlink_t *objlinkFirst, *objlinkCursor;

// List of unused and used contacts.
static objcontact_t *contFirst, *contCursor;

// List of contacts for each BSP leaf.
static objcontactlist_t *bspLeafContacts;

static void spreadInBspLeaf(BspLeaf *bspLeaf, contactfinderparams_t *parms);

static inline void linkContact(objcontact_t *con, objcontact_t **list, uint index)
{
    con->next = list[index];
    list[index] = con;
}

static void linkContactToBspLeaf(objcontact_t *node, objtype_t type, uint index)
{
    linkContact(node, &bspLeafContacts[index].head[type], 0);
}

/**
 * Create a new objcontact. If there are none available in the list of
 * used objects a new one will be allocated and linked to the global list.
 */
static objcontact_t *allocObjContact()
{
    objcontact_t *con;
    if(!contCursor)
    {
        con = (objcontact_t *) Z_Malloc(sizeof *con, PU_APPSTATIC, 0);

        // Link to the list of objcontact nodes.
        con->nextUsed = contFirst;
        contFirst = con;
    }
    else
    {
        con = contCursor;
        contCursor = contCursor->nextUsed;
    }
    con->obj = 0;
    return con;
}

static objlink_t *allocObjlink()
{
    objlink_t *link;
    if(!objlinkCursor)
    {
        link = (objlink_t *) Z_Malloc(sizeof *link, PU_APPSTATIC, 0);

        // Link the link to the global list.
        link->nextUsed = objlinkFirst;
        objlinkFirst = link;
    }
    else
    {
        link = objlinkCursor;
        objlinkCursor = objlinkCursor->nextUsed;
    }
    link->nextInBlock = 0;
    link->_object = 0;

    // Link it to the list of in-use objlinks.
    link->next = objlinks;
    objlinks = link;

    return link;
}

void R_InitObjlinkBlockmapForMap(Map &map)
{
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        DENG2_ASSERT(blockmaps[i] == 0);
        blockmaps[i] = new objlinkblockmap_t(map.bounds(), BLOCK_SIZE);
    }

    // Initialize obj => BspLeaf contact lists.
    bspLeafContacts = (objcontactlist_t *)
        Z_Calloc(sizeof *bspLeafContacts * map.bspLeafCount(),
                 PU_MAPSTATIC, 0);
}

void R_DestroyObjlinkBlockmap()
{
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        delete blockmaps[i]; blockmaps[i] = 0;
    }

    if(bspLeafContacts)
    {
        Z_Free(bspLeafContacts);
        bspLeafContacts = 0;
    }
}

static inline objlinkblockmap_t &blockmap(objtype_t type)
{
    DENG2_ASSERT(VALID_OBJTYPE(type));
    return *blockmaps[int( type )];
}

void R_ClearObjlinksForFrame()
{
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        blockmap(objtype_t(i)).unlinkAll();
    }

    // Start reusing objlinks.
    objlinkCursor = objlinkFirst;
    objlinks = 0;
}

static void linkObjToBspLeaf(BspLeaf &bspLeaf, void *object, objtype_t type)
{
    if(!object) return;

    // Never link to a BspLeaf with no geometry.
    if(!bspLeaf.hasPoly()) return;

    objcontact_t *con = allocObjContact();
    con->obj = object;
    // Link the contact list for this bspLeaf.
    linkContactToBspLeaf(con, type, bspLeaf.indexInMap());
}

inline static void linkObjToBspLeaf(BspLeaf &bspLeaf, objlink_t *link)
{
    linkObjToBspLeaf(bspLeaf, link->objectPtr(), link->type());
}

static void createObjlink(BspLeaf &bspLeaf, void *object, objtype_t type)
{
    if(!object) return;

    // Never link to a BspLeaf with no geometry.
    if(!bspLeaf.hasPoly()) return;

    objlink_t *link = allocObjlink();
    link->_object = object;
    link->_type   = type;
}

/**
 * On which side of the half-edge does the specified @a point lie?
 *
 * @param hedge  Half-edge to test.
 * @param point  Point to test in the map coordinate space.
 *
 * @return @c <0 Point is to the left/back of the segment.
 *         @c =0 Point lies directly on the segment.
 *         @c >0 Point is to the right/front of the segment.
 */
static coord_t pointOnHEdgeSide(HEdge const &hedge, Vector2d const &point)
{
    /// @todo Why are we calculating this every time?
    Vector2d direction = hedge.twin().origin() - hedge.origin();

    ddouble pointV1[2]      = { point.x, point.y };
    ddouble fromOriginV1[2] = { hedge.origin().x, hedge.origin().y };
    ddouble directionV1[2]  = { direction.x, direction.y };
    return V2d_PointOnLineSide(pointV1, fromOriginV1, directionV1);
}

static void maybeSpreadOverEdge(HEdge *hedge, contactfinderparams_t *parms)
{
    DENG_ASSERT(hedge != 0 && parms != 0);

    BspLeaf &leaf = hedge->face().mapElementAs<BspLeaf>();
    SectorCluster &cluster = leaf.cluster();

    // There must be a back BSP leaf to spread to.
    if(!hedge->hasTwin() || !hedge->twin().hasFace())
        return;

    BspLeaf &backLeaf = hedge->twin().face().mapElementAs<BspLeaf>();
    if(!backLeaf.hasCluster())
        return;
    SectorCluster &backCluster = backLeaf.cluster();

    // Which way does the spread go?
    if(!(leaf.validCount() == validCount && backLeaf.validCount() != validCount))
    {
        return; // Not eligible for spreading.
    }

    AABoxd const &backLeafAABox = backLeaf.poly().aaBox();

    // Is the leaf on the back side outside the origin's AABB?
    if(backLeafAABox.maxX <= parms->objAABox.minX ||
       backLeafAABox.minX >= parms->objAABox.maxX ||
       backLeafAABox.maxY <= parms->objAABox.minY ||
       backLeafAABox.minY >= parms->objAABox.maxY)
        return;

    // Too far from the edge?
    coord_t const length   = (hedge->twin().origin() - hedge->origin()).length();
    coord_t const distance = pointOnHEdgeSide(*hedge, parms->objlink->objectOrigin()) / length;
    if(de::abs(distance) >= parms->objlink->objectRadius())
        return;

    // Do not spread if the sector on the back side is closed with no height.
    if(!backCluster.hasWorldVolume())
        return;

    if(backCluster.visCeiling().heightSmoothed() <= cluster.visFloor().heightSmoothed() ||
       backCluster.visFloor().heightSmoothed() >= cluster.visCeiling().heightSmoothed())
        return;

    // Are there line side surfaces which should prevent spreading?
    if(hedge->hasMapElement())
    {
        LineSideSegment const &seg = hedge->mapElementAs<LineSideSegment>();

        // On which side of the line are we? (distance is from segment to origin).
        LineSide const &facingLineSide = seg.line().side(seg.lineSide().sideId() ^ (distance < 0));

        // One-way window?
        if(!facingLineSide.back().hasSections())
            return;

        SectorCluster const &fromCluster = facingLineSide.isFront()? cluster : backCluster;
        SectorCluster const &toCluster   = facingLineSide.isFront()? backCluster : cluster;

        // Might a material cover the opening?
        if(facingLineSide.hasSections() && facingLineSide.middle().hasMaterial())
        {
            // Stretched middles always cover the opening.
            if(facingLineSide.isFlagged(SDF_MIDDLE_STRETCH))
                return;

            // Determine the opening between the visual sector planes at this edge.
            coord_t openBottom;
            if(toCluster.visFloor().heightSmoothed() > fromCluster.visFloor().heightSmoothed())
            {
                openBottom = toCluster.visFloor().heightSmoothed();
            }
            else
            {
                openBottom = fromCluster.visFloor().heightSmoothed();
            }

            coord_t openTop;
            if(toCluster.visCeiling().heightSmoothed() < fromCluster.visCeiling().heightSmoothed())
            {
                openTop = toCluster.visCeiling().heightSmoothed();
            }
            else
            {
                openTop = fromCluster.visCeiling().heightSmoothed();
            }

            // Ensure we have up to date info about the material.
            MaterialSnapshot const &ms = facingLineSide.middle().material().prepare(Rend_MapSurfaceMaterialSpec());
            if(ms.height() >= openTop - openBottom)
            {
                // Possibly; check the placement.
                WallEdge edge(WallSpec::fromMapSide(facingLineSide, LineSide::Middle),
                              *facingLineSide.leftHEdge(), Line::From);

                if(edge.isValid() && edge.top().z() > edge.bottom().z()
                   && edge.top().z() >= openTop && edge.bottom().z() <= openBottom)
                    return;
            }
        }
    }

    // During next step, obj will continue spreading from there.
    backLeaf.setValidCount(validCount);

    // Link up a new contact with the back BSP leaf.
    linkObjToBspLeaf(backLeaf, parms->objlink);

    spreadInBspLeaf(&backLeaf, parms);
}

/**
 * Attempt to spread the obj from the given contact from the source
 * BspLeaf and into the (relative) back BspLeaf.
 *
 * @param bspLeaf  BspLeaf to attempt to spread over to.
 * @param parms    Contact finder parameters.
 *
 * @return  Always @c true. (This function is also used as an iterator.)
 */
static void spreadInBspLeaf(BspLeaf *bspLeaf, contactfinderparams_t *parms)
{
    if(!bspLeaf || !bspLeaf->hasCluster())
        return;

    HEdge *base = bspLeaf->poly().hedge();
    HEdge *hedge = base;
    do
    {
        maybeSpreadOverEdge(hedge, parms);
    } while((hedge = &hedge->next()) != base);
}

/**
 * Create a contact for the objlink in all the BspLeafs the linked obj is
 * contacting (tests done on bounding boxes and the BSP leaf spread test).
 *
 * @param oLink Ptr to objlink to find BspLeaf contacts for.
 */
static void findContacts(objlink_t *link)
{
    DENG_ASSERT(link != 0);

    BspLeaf &bspLeaf = link->objectBspLeafAtOrigin();

    // Do the BSP leaf spread. Begin from the obj's own BspLeaf.
    bspLeaf.setValidCount(++validCount);

    contactfinderparams_t parms; zap(parms);
    parms.objlink  = link;
    parms.objAABox = link->objectAABox();

    // Always contact the obj's own BspLeaf.
    linkObjToBspLeaf(bspLeaf, link);

    spreadInBspLeaf(&bspLeaf, &parms);
}

/**
 * Spread contacts in the blockmap to all other BspLeafs within the block.
 *
 * @param obm      Objlink blockmap.
 * @param bspLeaf  BspLeaf to spread the contacts of.
 */
static void spreadContacts(objlinkblockmap_t &obm, AABoxd const &box)
{
    GridmapCellBlock const cellBlock = obm.toCellBlock(box);

    GridmapCell cell;
    for(cell.y = cellBlock.min.y; cell.y <= cellBlock.max.y; ++cell.y)
    for(cell.x = cellBlock.min.x; cell.x <= cellBlock.max.x; ++cell.x)
    {
        objlinkblockmap_t::CellData *data = obm.data(cell, true/*can allocate a block*/);
        if(!data->doneSpread)
        {
            for(objlink_t *iter = data->head; iter; iter = iter->nextInBlock)
            {
                findContacts(iter);
            }
            data->doneSpread = true;
        }
    }
}

static inline float radiusMax(objtype_t type)
{
    DENG_ASSERT(VALID_OBJTYPE(type));
    if(type == OT_LUMOBJ) return Lumobj::radiusMax();
    // Must be OT_MOBJ
    return DDMOBJ_RADIUS_MAX;
}

void R_InitForBspLeaf(BspLeaf &bspLeaf)
{
    if(!bspLeaf.hasCluster())
        return;

    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        float const maxRadius   = radiusMax(objtype_t(i));
        objlinkblockmap_t &bmap = blockmap(objtype_t(i));

        AABoxd bounds = bspLeaf.poly().aaBox();
        bounds.minX -= maxRadius;
        bounds.minY -= maxRadius;
        bounds.maxX += maxRadius;
        bounds.maxY += maxRadius;

        spreadContacts(bmap, bounds);
    }
}

void R_LinkObjs()
{
    // Link object-links into the relevant blockmap.
    for(objlink_t *link = objlinks; link; link = link->next)
    {
        objlinkblockmap_t &bmap = blockmap(link->type());
        bool clamped;
        GridmapCell cell = bmap.toCell(link->objectOrigin(), &clamped);
        if(!clamped)
        {
            bmap.link(cell, link);
        }
    }
}

void R_InitForNewFrame()
{
    // Start reusing nodes from the first one in the list.
    contCursor = contFirst;
    if(bspLeafContacts)
    {
        std::memset(bspLeafContacts, 0, App_World().map().bspLeafCount() * sizeof *bspLeafContacts);
    }
}

int R_IterateBspLeafMobjContacts(BspLeaf &bspLeaf,
    int (*callback)(mobj_s &, void *), void *context)
{
    objcontactlist_t const &conList = bspLeafContacts[bspLeaf.indexInMap()];
    for(objcontact_t *con = conList.head[OT_MOBJ]; con; con = con->next)
    {
        if(int result = callback(*static_cast<struct mobj_s *>(con->obj), context))
            return result;
    }
    return false; // Continue iteration.
}

int R_IterateBspLeafLumobjContacts(BspLeaf &bspLeaf,
    int (*callback)(Lumobj &, void *), void *context)
{
    objcontactlist_t const &conList = bspLeafContacts[bspLeaf.indexInMap()];
    for(objcontact_t *con = conList.head[OT_LUMOBJ]; con; con = con->next)
    {
        if(int result = callback(*static_cast<Lumobj *>(con->obj), context))
            return result;
    }
    return false; // Continue iteration.
}

void R_ObjlinkCreate(mobj_t &mobj)
{
    if(!Mobj_IsLinked(mobj)) return;
    createObjlink(Mobj_BspLeafAtOrigin(mobj), &mobj, OT_MOBJ);
}

void R_LinkObjToBspLeaf(BspLeaf &bspLeaf, mobj_t &mobj)
{
    linkObjToBspLeaf(bspLeaf, &mobj, OT_MOBJ);
}

void R_ObjlinkCreate(Lumobj &lum)
{
    createObjlink(lum.bspLeafAtOrigin(), &lum, OT_LUMOBJ);
}

void R_LinkObjToBspLeaf(BspLeaf &bspLeaf, Lumobj &lum)
{
    linkObjToBspLeaf(bspLeaf, &lum, OT_LUMOBJ);
}
