/** @file p_objlink.cpp World object => BSP leaf "contact" blockmap.
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

#include <de/memoryzone.h>
#include <de/vector1.h>

#include "Face"
#include "gridmap.h"

#include "world/map.h"
#include "world/p_object.h"
#include "BspLeaf"
#include "Surface"

#include "render/r_main.h" // validCount
#include "render/rend_main.h" // Rend_MapSurfaceMaterialSpec
#include "WallEdge"

#include "world/contactblockmap.h"

using namespace de;

enum ContactType
{
    ContactMobj = 0,
    ContactLumobj,

    ContactTypeCount
};

/// @todo Obviously, polymorphism is a better solution.
struct Contact
{
    Contact *nextInBlock; ///< Next in the same blockmap cell (if any, not owned).
    Contact *nextUsed;    ///< Next in the used list (if any, not owned).
    Contact *next;        ///< Next in global list of contacts (if any, not owned).

    ContactType _type;    ///< Logical identifier.
    void *_object;        ///< The contacted object.

    ContactType type() const
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
        case ContactLumobj: return objectAs<Lumobj>().origin();
        case ContactMobj:   return Mobj_Origin(objectAs<mobj_t>());

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
        case ContactLumobj: return objectAs<Lumobj>().radius();
        case ContactMobj:   return Mobj_VisualRadius(&objectAs<mobj_t>());

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
        case ContactLumobj: return objectAs<Lumobj>().bspLeafAtOrigin();
        case ContactMobj:   return Mobj_BspLeafAtOrigin(objectAs<mobj_t>());

        default:
            throw Error("Contact::objectBspLeafAtOrigin", "Invalid type");
        }
    }
};

struct ListNode
{
    ListNode *next;     ///< Next in the BSP leaf.
    ListNode *nextUsed; ///< Next used contact.
    void *obj;
};
static ListNode *firstNode; ///< First unused list node.
static ListNode *cursor;    ///< Current list node.

struct ContactList
{
    // Start reusing list nodes.
    static void reset()
    {
        cursor = firstNode;
    }

    void link(Contact *contact)
    {
        if(!contact) return;

        ListNode *node = newNode(contact->objectPtr());

        node->next = _head;
        _head = node;
    }

    ListNode *begin() const
    {
        return _head;
    }

private:
    /**
     * Create a new list node. If there are none available in the list of
     * used objects a new one will be allocated and linked to the global list.
     */
    static ListNode *newNode(void *object)
    {
        DENG2_ASSERT(object != 0);

        ListNode *node;
        if(!cursor)
        {
            node = (ListNode *) Z_Malloc(sizeof(*node), PU_APPSTATIC, 0);

            // Link in the global list of used nodes.
            node->nextUsed = firstNode;
            firstNode = node;
        }
        else
        {
            node = cursor;
            cursor = cursor->nextUsed;
        }

        node->obj = object;
        node->next = 0;

        return node;
    }

    ListNode *_head;
};

// Separate contact lists for each BSP leaf and contact type.
static ContactList *bspLeafContactLists;

static inline ContactList &contactList(BspLeaf &bspLeaf, ContactType type)
{
    return bspLeafContactLists[bspLeaf.indexInMap() * ContactTypeCount + int( type )];
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
    Vector2d const direction = hedge.twin().origin() - hedge.origin();

    ddouble pointV1[2]      = { point.x, point.y };
    ddouble fromOriginV1[2] = { hedge.origin().x, hedge.origin().y };
    ddouble directionV1[2]  = { direction.x, direction.y };
    return V2d_PointOnLineSide(pointV1, fromOriginV1, directionV1);
}

DENG2_PIMPL(ContactBlockmap), public Gridmap
{
    struct CellData
    {
        Contact *head;
        bool doneSpread; ///< Used to prevent repeat processing.

        void unlinkAll()
        {
            head = 0;
            doneSpread = false;
        }
    };

    AABoxd bounds;
    uint blockSize; ///< In map space units.

    // For perf, spread state data is "global".
    struct SpreadState
    {
        Contact *contact;
        AABoxd contactAABox;
    };
    SpreadState spread;

    Instance(Public *i, AABoxd const &bounds, uint blockSize)
        : Base(i),
          Gridmap(Cell(de::ceil((bounds.maxX - bounds.minX) / ddouble( blockSize )),
                       de::ceil((bounds.maxY - bounds.minY) / ddouble( blockSize ))),
                  sizeof(CellData), PU_MAPSTATIC),
          bounds(bounds),
          blockSize(blockSize)
    {}

    /**
     * Given map space X coordinate @a x, return the corresponding cell coordinate.
     * If @a x is outside the blockmap it will be clamped to the nearest edge on
     * the X axis.
     *
     * @param x        Map space X coordinate to be translated.
     * @param didClip  Set to @c true iff clamping was necessary.
     *
     * @return  Translated blockmap cell X coordinate.
     */
    uint toCellX(ddouble x, bool &didClip) const
    {
        didClip = false;
        if(x < bounds.minX)
        {
            x = bounds.minX;
            didClip = true;
        }
        else if(x >= bounds.maxX)
        {
            x = bounds.maxX - 1;
            didClip = true;
        }
        return uint((x - bounds.minX) / blockSize);
    }

    /**
     * Given map space Y coordinate @a y, return the corresponding cell coordinate.
     * If @a y is outside the blockmap it will be clamped to the nearest edge on
     * the Y axis.
     *
     * @param y        Map space Y coordinate to be translated.
     * @param didClip  Set to @c true iff clamping was necessary.
     *
     * @return  Translated blockmap cell Y coordinate.
     */
    uint toCellY(ddouble y, bool &didClip) const
    {
        didClip = false;
        if(y < bounds.minY)
        {
            y = bounds.minY;
            didClip = true;
        }
        else if(y >= bounds.maxY)
        {
            y = bounds.maxY - 1;
            didClip = true;
        }
        return uint((y - bounds.minY) / blockSize);
    }

    /**
     * Determines in which blockmap cell the specified map point lies. If the
     * these coordinates are outside the blockmap they will be clamped within
     * the valid range.
     *
     * @param retAdjusted  If specified, whether or not the @a point coordinates
     *                     had to be adjusted is written back here.
     *
     * @return  The determined blockmap cell.
     */
    Cell toCell(Vector2d const &point, bool *retAdjusted = 0) const
    {
        bool didClipX, didClipY;
        Cell cell(toCellX(point.x, didClipX), toCellY(point.y, didClipY));
        if(retAdjusted) *retAdjusted = didClipX | didClipY;
        return cell;
    }

    inline CellBlock toCellBlock(AABoxd const &box)
    {
        return CellBlock(toCell(box.min), toCell(box.max));
    }

    /**
     * Returns the data for the specified cell.
     */
    inline CellData *cellData(Cell const &cell, bool canAlloc = false)
    {
        return static_cast<CellData *>(Gridmap::cellData(cell, canAlloc));
    }

    void maybeSpreadOverEdge(HEdge *hedge)
    {
        DENG2_ASSERT(spread.contact != 0);

        if(!hedge) return;

        BspLeaf &leaf = hedge->face().mapElementAs<BspLeaf>();
        SectorCluster &cluster = leaf.cluster();

        // There must be a back BSP leaf to spread to.
        if(!hedge->hasTwin()) return;
        if(!hedge->twin().hasFace()) return;

        BspLeaf &backLeaf = hedge->twin().face().mapElementAs<BspLeaf>();
        if(!backLeaf.hasCluster()) return;

        SectorCluster &backCluster = backLeaf.cluster();

        // Which way does the spread go?
        if(!(leaf.validCount() == validCount && backLeaf.validCount() != validCount))
        {
            return; // Not eligible for spreading.
        }

        // Is the leaf on the back side outside the origin's AABB?
        AABoxd const &aaBox = backLeaf.poly().aaBox();
        if(aaBox.maxX <= spread.contactAABox.minX || aaBox.minX >= spread.contactAABox.maxX ||
           aaBox.maxY <= spread.contactAABox.minY || aaBox.minY >= spread.contactAABox.maxY)
            return;

        // Too far from the edge?
        coord_t const length   = (hedge->twin().origin() - hedge->origin()).length();
        coord_t const distance = pointOnHEdgeSide(*hedge, spread.contact->objectOrigin()) / length;
        if(de::abs(distance) >= spread.contact->objectRadius())
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

                    if(edge.isValid() && edge.top().z() > edge.bottom().z() &&
                       edge.top().z() >= openTop && edge.bottom().z() <= openBottom)
                        return;
                }
            }
        }

        // During the next step this contact will spread from the back leaf.
        backLeaf.setValidCount(validCount);

        contactList(backLeaf, spread.contact->type()).link(spread.contact);

        spreadInBspLeaf(backLeaf);
    }

    /**
     * Attempt to spread the obj from the given contact from the source
     * BspLeaf and into the (relative) back BspLeaf.
     *
     * @param bspLeaf  BspLeaf to attempt to spread over to.
     *
     * @return  Always @c true. (This function is also used as an iterator.)
     */
    void spreadInBspLeaf(BspLeaf &bspLeaf)
    {
        if(bspLeaf.hasCluster())
        {
            HEdge *base = bspLeaf.poly().hedge();
            HEdge *hedge = base;
            do
            {
                maybeSpreadOverEdge(hedge);

            } while((hedge = &hedge->next()) != base);
        }
    }

    /**
     * Link the contact in all BspLeafs which touch the linked object (tests are
     * done with bounding boxes and the BSP leaf spread test).
     *
     * @param contact  Contact to be spread.
     */
    void spreadContact(Contact &contact)
    {
        BspLeaf &bspLeaf = contact.objectBspLeafAtOrigin();
        DENG2_ASSERT(bspLeaf.hasCluster()); // Sanity check.

        contactList(bspLeaf, contact.type()).link(&contact);

        // Spread to neighboring BSP leafs.
        bspLeaf.setValidCount(++validCount);

        spread.contact      = &contact;
        spread.contactAABox = contact.objectAABox();

        spreadInBspLeaf(bspLeaf);
    }

    static int unlinkAllWorker(void *obj, void *context)
    {
        DENG2_UNUSED(context);
        static_cast<CellData *>(obj)->unlinkAll();
        return false; // Continue iteration.
    }
};

ContactBlockmap::ContactBlockmap(AABoxd const &bounds, uint blockSize)
    : d(new Instance(this, bounds, blockSize))
{}

Vector2d ContactBlockmap::origin() const
{
    return Vector2d(d->bounds.min);
}

AABoxd const &ContactBlockmap::bounds() const
{
    return d->bounds;
}

void ContactBlockmap::link(Contact *contact)
{
    if(!contact) return;

    bool outside;
    Instance::Cell cell = d->toCell(contact->objectOrigin(), &outside);
    if(outside) return;

    Instance::CellData *data = d->cellData(cell, true/*can allocate a block*/);
    contact->nextInBlock = data->head;
    data->head = contact;
}

void ContactBlockmap::unlinkAll()
{
    d->iterate(Instance::unlinkAllWorker);
}

void ContactBlockmap::spreadAllContacts(AABoxd const &box)
{
    Instance::CellBlock const cellBlock = d->toCellBlock(box);

    Instance::Cell cell;
    for(cell.y = cellBlock.min.y; cell.y <= cellBlock.max.y; ++cell.y)
    for(cell.x = cellBlock.min.x; cell.x <= cellBlock.max.x; ++cell.x)
    {
        if(Instance::CellData *data = d->cellData(cell))
        {
            if(!data->doneSpread)
            {
                data->doneSpread = true;

                for(Contact *iter = data->head; iter; iter = iter->nextInBlock)
                {
                    d->spreadContact(*iter);
                }
            }
        }
    }
}

// Each contactable object type uses a separate blockmap.
static ContactBlockmap *blockmaps[ContactTypeCount];

static Contact *contacts;
static Contact *contactFirst, *contactCursor;

static Contact *newContact(void *object, ContactType type)
{
    DENG2_ASSERT(object != 0);

    Contact *contact;
    if(!contactCursor)
    {
        contact = (Contact *) Z_Malloc(sizeof *contact, PU_APPSTATIC, 0);

        // Link in the global list of used contacts.
        contact->nextUsed = contactFirst;
        contactFirst = contact;
    }
    else
    {
        contact = contactCursor;
        contactCursor = contactCursor->nextUsed;
    }

    contact->nextInBlock = 0;

    // Link in the list of in-use contacts.
    contact->next = contacts;
    contacts = contact;

    contact->_object = object;
    contact->_type   = type;

    return contact;
}

void R_InitContactBlockmaps(Map &map)
{
    for(int i = 0; i < ContactTypeCount; ++i)
    {
        DENG2_ASSERT(blockmaps[i] == 0);
        blockmaps[i] = new ContactBlockmap(map.bounds());
    }

    // Initialize object => BspLeaf contact lists.
    bspLeafContactLists = (ContactList *)
        Z_Calloc(map.bspLeafCount() * ContactTypeCount * sizeof(*bspLeafContactLists),
                 PU_MAPSTATIC, 0);
}

void R_DestroyContactBlockmaps()
{
    for(int i = 0; i < ContactTypeCount; ++i)
    {
        delete blockmaps[i]; blockmaps[i] = 0;
    }

    if(bspLeafContactLists)
    {
        Z_Free(bspLeafContactLists);
        bspLeafContactLists = 0;
    }
}

static inline ContactBlockmap &blockmap(ContactType type)
{
    return *blockmaps[int( type )];
}

void R_ClearContacts(Map &map)
{
    for(int i = 0; i < ContactTypeCount; ++i)
    {
        blockmap(ContactType(i)).unlinkAll();
    }

    // Start reusing contacts.
    contactCursor = contactFirst;
    contacts = 0;

    // Start reusing nodes from the first one in the list.
    ContactList::reset();

    if(bspLeafContactLists)
    {
        std::memset(bspLeafContactLists, 0,
                    map.bspLeafCount() * ContactTypeCount * sizeof(*bspLeafContactLists));
    }
}

static inline float radiusMax(ContactType type)
{
    switch(type)
    {
    case ContactLumobj: return Lumobj::radiusMax();
    case ContactMobj:   return DDMOBJ_RADIUS_MAX;

    default:
        DENG2_ASSERT(false);
        return 8;
    }
}

void R_SpreadContacts(BspLeaf &bspLeaf)
{
    if(!bspLeaf.hasCluster())
        return;

    for(int i = 0; i < ContactTypeCount; ++i)
    {
        float const maxRadius = radiusMax(ContactType(i));
        ContactBlockmap &bmap = blockmap(ContactType(i));

        AABoxd bounds = bspLeaf.poly().aaBox();
        bounds.minX -= maxRadius;
        bounds.minY -= maxRadius;
        bounds.maxX += maxRadius;
        bounds.maxY += maxRadius;

        bmap.spreadAllContacts(bounds);
    }
}

void R_AddContact(mobj_t &mobj)
{
    // BspLeafs with no geometry cannot be contacted (zero world volume).
    if(Mobj_BspLeafAtOrigin(mobj).hasCluster())
    {
        newContact(&mobj, ContactMobj);
    }
}

void R_AddContact(Lumobj &lum)
{
    // BspLeafs with no geometry cannot be contacted (zero world volume).
    if(lum.bspLeafAtOrigin().hasCluster())
    {
        newContact(&lum, ContactLumobj);
    }
}

void R_LinkContacts()
{
    // Link contacts into the relevant blockmap.
    for(Contact *contact = contacts; contact; contact = contact->next)
    {
        blockmap(contact->type()).link(contact);
    }
}

int R_IterateBspLeafMobjContacts(BspLeaf &bspLeaf,
    int (*callback)(mobj_s &, void *), void *context)
{
    ContactList &list = contactList(bspLeaf, ContactMobj);
    for(ListNode *node = list.begin(); node; node = node->next)
    {
        if(int result = callback(*static_cast<mobj_t *>(node->obj), context))
            return result;
    }
    return false; // Continue iteration.
}

int R_IterateBspLeafLumobjContacts(BspLeaf &bspLeaf,
    int (*callback)(Lumobj &, void *), void *context)
{
    ContactList &list = contactList(bspLeaf, ContactLumobj);
    for(ListNode *node = list.begin(); node; node = node->next)
    {
        if(int result = callback(*static_cast<Lumobj *>(node->obj), context))
            return result;
    }
    return false; // Continue iteration.
}
