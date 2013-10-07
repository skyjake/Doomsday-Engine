/** @file contact.h World object => BSP leaf "contact" and contact lists.
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

#ifdef __CLIENT__
#ifndef DENG_CLIENT_WORLD_CONTACT_H
#define DENG_CLIENT_WORLD_CONTACT_H

#include <de/aabox.h>

#include <de/Vector>

#include "world/map.h"

class BspLeaf;
struct Contact;
class Lumobj;

enum ContactType
{
    ContactMobj = 0,
    ContactLumobj,

    ContactTypeCount
};

/// @todo Obviously, polymorphism is a better solution.
struct Contact
{
    Contact *nextUsed;    ///< Next in the used list (if any, not owned).
    Contact *next;        ///< Next in global list of contacts (if any, not owned).

    ContactType _type;    ///< Logical identifier.
    void *_object;        ///< The contacted object.

    ContactType type() const;

    void *objectPtr() const;

    template <class ObjectType>
    ObjectType &objectAs() const {
        DENG2_ASSERT(_object != 0);
        return *static_cast<ObjectType *>(_object);
    }

    /**
     * Returns a copy of the linked object's origin in map space.
     */
    de::Vector3d objectOrigin() const;

    /**
     * Returns the linked object's radius in map space.
     */
    double objectRadius() const;

    /**
     * Returns an axis-aligned bounding box for the linked object in map space.
     */
    AABoxd objectAABox() const;

    /**
     * Returns the BSP leaf at the linked object's origin in map space.
     */
    BspLeaf &objectBspLeafAtOrigin() const;
};

struct ContactList
{
    struct Node;

    // Start reusing list nodes.
    static void reset();

    void link(Contact *contact);

    Node *begin() const;

private:
    /**
     * Create a new list node. If there are none available in the list of
     * used objects a new one will be allocated and linked to the global list.
     */
    static Node *newNode(void *object);

    Node *_head;
};

/**
 * To be called during game change/on shutdown to destroy all contact lists.
 * This is necessary because the lists are allocated from the Zone using a
 * >= PU_MAP purge level and access to them is handled with global pointers.
 *
 * @todo Encapsulate allocation of and access to the lists in de::Map
 */
void R_DestroyContactLists();

/**
 * Initialize contact lists for the current map.
 */
void R_InitContactLists(de::Map &map);

/**
 * To be called at the beginning of a render frame to clear all contact lists
 * ready for the new frame.
 */
void R_ClearContactLists(de::Map &map);

/**
 * Add a new contact for the specified mobj, for spreading purposes.
 */
void R_AddContact(struct mobj_s &mobj);

/**
 * Add a new contact for the specified lumobj, for spreading purposes.
 */
void R_AddContact(Lumobj &lumobj);

/**
 * Traverse the list of @em all contacts for the current render frame.
 */
int R_ContactIterator(int (*callback) (Contact &, void *), void *context = 0);

/**
 * Returns the contact list for the specified BSP @a leaf and contact @a type.
 */
ContactList &R_ContactList(BspLeaf &leaf, ContactType type);

/**
 * Traverse the list of mobj contacts linked directly to the specified BSP @a leaf,
 * for the current render frame.
 */
int R_BspLeafMobjContactIterator(BspLeaf &leaf,
    int (*callback) (struct mobj_s &, void *), void *context = 0);

/**
 * Traverse the list of lumobj contacts linked directly to the specified BSP @a leaf,
 * for the current render frame.
 */
int R_BspLeafLumobjContactIterator(BspLeaf &leaf,
    int (*callback) (Lumobj &, void *), void *context = 0);

#endif // DENG_CLIENT_WORLD_CONTACT_H
#endif // __CLIENT__
