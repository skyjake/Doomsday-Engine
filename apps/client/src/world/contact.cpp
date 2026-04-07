/** @file contact.cpp  World object => BSP leaf "contact" and contact lists.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_platform.h"
#include "world/contact.h"
#include "world/map.h"
#include "world/p_object.h"
#include "world/convexsubspace.h"

#include <doomsday/world/bspleaf.h>
#include <de/legacy/memoryzone.h>
#include <de/error.h>

using namespace de;

ContactType Contact::type() const
{
    return _type;
}

void *Contact::objectPtr() const
{
    return _object;
}

Vec3d Contact::objectOrigin() const
{
    switch(_type)
    {
    case ContactLumobj: return objectAs<Lumobj>().origin();
    case ContactMobj:   return Mobj_Origin(objectAs<mobj_t>());

    default: break;
    }
    DE_ASSERT(false);
    return Vec3d();
}

ddouble Contact::objectRadius() const
{
    switch(_type)
    {
    case ContactLumobj: return objectAs<Lumobj>().radius();
    case ContactMobj:   return Mobj_VisualRadius(objectAs<mobj_t>());

    default: break;
    }
    DE_ASSERT(false);
    return 0;
}

AABoxd Contact::objectBounds() const
{
    switch(_type)
    {
    case ContactLumobj: return objectAs<Lumobj>().bounds();
    case ContactMobj:   return Mobj_Bounds(objectAs<mobj_t>());

    default: break;
    }
    DE_ASSERT(false);
    return AABoxd();
}

world::BspLeaf &Contact::objectBspLeafAtOrigin() const
{
    switch(_type)
    {
    case ContactLumobj: return objectAs<Lumobj>().bspLeafAtOrigin();
    case ContactMobj:   return Mobj_BspLeafAtOrigin(objectAs<mobj_t>());

    default: throw Error("Contact::objectBspLeafAtOrigin", "Invalid type");
    }
}

//- ContactList -------------------------------------------------------------------------

struct ContactList::Node
{
    Node *next;      ///< Next in the BSP leaf.
    Node *nextUsed;  ///< Next used contact.
    void *obj;
};
static ContactList::Node *firstNode;  ///< First unused list node.
static ContactList::Node *cursor;     ///< Current list node.

void ContactList::reset()  // static
{
    cursor = firstNode;
}

void ContactList::link(Contact *contact)
{
    if(!contact) return;

    Node *node = newNode(contact->objectPtr());

    node->next = _head;
    _head = node;
}

ContactList::Node *ContactList::begin() const
{
    return _head;
}

ContactList::Node *ContactList::newNode(void *object) // static
{
    DE_ASSERT(object);

    Node *node;
    if(!cursor)
    {
        node = (Node *) Z_Malloc(sizeof(*node), PU_APPSTATIC, nullptr);

        // Link in the global list of used nodes.
        node->nextUsed = firstNode;
        firstNode = node;
    }
    else
    {
        node = cursor;
        cursor = cursor->nextUsed;
    }

    node->obj  = object;
    node->next = nullptr;

    return node;
}

// Separate contact lists for each BSP leaf and contact type.
static ContactList *subspaceContactLists;

ContactList &R_ContactList(const ConvexSubspace &subspace, ContactType type)
{
    return subspaceContactLists[subspace.indexInMap() * ContactTypeCount + int(type)];
}

static Contact *contacts;
static Contact *contactFirst, *contactCursor;

static Contact *newContact(void *object, ContactType type)
{
    DE_ASSERT(object);

    Contact *contact;
    if(!contactCursor)
    {
        contact = (Contact *) Z_Malloc(sizeof *contact, PU_APPSTATIC, nullptr);

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
    subspaceContactLists = (ContactList *)
        Z_Calloc(map.subspaceCount() * ContactTypeCount * sizeof(*subspaceContactLists),
                 PU_MAPSTATIC, nullptr);
}

void R_DestroyContactLists()
{
    Z_Free(subspaceContactLists); subspaceContactLists = nullptr;
}

void R_ClearContactLists(Map &map)
{
    // Start reusing contacts.
    contactCursor = contactFirst;
    contacts = nullptr;

    // Start reusing nodes from the first one in the list.
    ContactList::reset();

    if(subspaceContactLists)
    {
        std::memset(subspaceContactLists, 0,
                    map.subspaceCount() * ContactTypeCount * sizeof(*subspaceContactLists));
    }
}

void R_AddContact(mobj_t &mobj)
{
    // BspLeafs with no geometry cannot be contacted (zero world volume).
    if(Mobj_BspLeafAtOrigin(mobj).hasSubspace())
    {
        newContact(&mobj, ContactMobj);
    }
}

void R_AddContact(Lumobj &lum)
{
    // BspLeafs with no geometry cannot be contacted (zero world volume).
    if(lum.bspLeafAtOrigin().hasSubspace())
    {
        newContact(&lum, ContactLumobj);
    }
}

LoopResult R_ForAllContacts(const std::function<LoopResult (const Contact &)> &func)
{
    for(Contact *contact = contacts; contact; contact = contact->next)
    {
        if(auto result = func(*contact))
            return result;
    }
    return LoopContinue;
}

LoopResult R_ForAllSubspaceMobContacts(const ConvexSubspace &subspace, const std::function<LoopResult (mobj_s &)> &func)
{
    ContactList &list = R_ContactList(subspace, ContactMobj);
    for(ContactList::Node *node = list.begin(); node; node = node->next)
    {
        if(auto result = func(*static_cast<mobj_t *>(node->obj)))
            return result;
    }
    return LoopContinue;
}

LoopResult R_ForAllSubspaceLumContacts(const ConvexSubspace &subspace, const std::function<LoopResult (Lumobj &)> &func)
{
    ContactList &list = R_ContactList(subspace, ContactLumobj);
    for(ContactList::Node *node = list.begin(); node; node = node->next)
    {
        if(auto result = func(*static_cast<Lumobj *>(node->obj)))
            return result;
    }
    return LoopContinue;
}
