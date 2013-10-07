/** @file contactblockmap.cpp World object => BSP leaf "contact" blockmap.
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

//#include <QBitArray>

#include <de/memoryzone.h>
#include <de/vector1.h>

#include <de/Error>

#include "Face"

#include "world/map.h"
//#include "world/blockmap.h"
#include "world/p_object.h"
#include "BspLeaf"
//#include "Surface"

//#include "render/r_main.h" // validCount
//#include "render/rend_main.h" // Rend_MapSurfaceMaterialSpec
//#include "WallEdge"

#include "world/contactblockmap.h"

using namespace de;

ContactType Contact::type() const
{
    return _type;
}

void *Contact::objectPtr() const
{
    return _object;
}

Vector3d Contact::objectOrigin() const
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

double Contact::objectRadius() const
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

AABoxd Contact::objectAABox() const
{
    Vector2d const origin = objectOrigin();
    ddouble const radius  = objectRadius();
    return AABoxd(origin.x - radius, origin.y - radius,
                  origin.x + radius, origin.y + radius);
}

BspLeaf &Contact::objectBspLeafAtOrigin() const
{
    switch(_type)
    {
    case ContactLumobj: return objectAs<Lumobj>().bspLeafAtOrigin();
    case ContactMobj:   return Mobj_BspLeafAtOrigin(objectAs<mobj_t>());

    default:
        throw Error("Contact::objectBspLeafAtOrigin", "Invalid type");
    }
}

struct ListNode
{
    ListNode *next;     ///< Next in the BSP leaf.
    ListNode *nextUsed; ///< Next used contact.
    void *obj;
};
static ListNode *firstNode; ///< First unused list node.
static ListNode *cursor;    ///< Current list node.

void ContactList::reset() // static
{
    cursor = firstNode;
}

void ContactList::link(Contact *contact)
{
    if(!contact) return;

    ListNode *node = newNode(contact->objectPtr());

    node->next = _head;
    _head = node;
}

ListNode *ContactList::begin() const
{
    return _head;
}

ListNode *ContactList::newNode(void *object) // static
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

// Separate contact lists for each BSP leaf and contact type.
static ContactList *bspLeafContactLists;

ContactList &R_ContactList(BspLeaf &bspLeaf, ContactType type)
{
    return bspLeafContactLists[bspLeaf.indexInMap() * ContactTypeCount + int( type )];
}

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

    // Link in the list of in-use contacts.
    contact->next = contacts;
    contacts = contact;

    contact->_object = object;
    contact->_type   = type;

    return contact;
}

void R_InitContactLists(Map &map)
{
    // Initialize object => BspLeaf contact lists.
    bspLeafContactLists = (ContactList *)
        Z_Calloc(map.bspLeafCount() * ContactTypeCount * sizeof(*bspLeafContactLists),
                 PU_MAPSTATIC, 0);
}

void R_DestroyContactLists()
{
    if(bspLeafContactLists)
    {
        Z_Free(bspLeafContactLists);
        bspLeafContactLists = 0;
    }
}

void R_ClearContactLists(Map &map)
{
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

int R_ContactIterator(int (*callback) (Contact &, void *), void *context)
{
    // Link contacts into the relevant blockmap.
    for(Contact *contact = contacts; contact; contact = contact->next)
    {
        if(int result = callback(*contact, context))
            return result;
    }
    return false; // Continue iteration.
}

int R_IterateBspLeafMobjContacts(BspLeaf &bspLeaf,
    int (*callback)(mobj_s &, void *), void *context)
{
    ContactList &list = R_ContactList(bspLeaf, ContactMobj);
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
    ContactList &list = R_ContactList(bspLeaf, ContactLumobj);
    for(ListNode *node = list.begin(); node; node = node->next)
    {
        if(int result = callback(*static_cast<Lumobj *>(node->obj), context))
            return result;
    }
    return false; // Continue iteration.
}
