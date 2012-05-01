/**
 * @file binarytree.cpp
 * A fairly standard binary tree implementation. @ingroup data
 *
 * @authors Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

#include "binarytree.h"

#include <cstdlib>
#include <assert.h>

/**
 * C Wrapper implementation for a general purpose de::BinaryTree<void*>
 */

// Helper macro for converting a boolean to child id.
#define TOCHILDID(v) ((v)? de::BinaryTree<void*>::LEFT : de::BinaryTree<void*>::RIGHT)

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::BinaryTree<void*>*>(inst) : NULL

#define TOPUBLIC(inst) \
    (inst) != 0? reinterpret_cast<BinaryTree*>(inst) : NULL

#define SELF(inst) \
    assert(inst); \
    de::BinaryTree<void*>* self = TOINTERNAL(inst)

BinaryTree* BinaryTree_NewWithSubtrees(void* userData, BinaryTree* rightSubtree, BinaryTree* leftSubtree)
{
    BinaryTree* tree = TOPUBLIC(new de::BinaryTree<void*>(userData, 0/*no parent*/, TOINTERNAL(rightSubtree), TOINTERNAL(leftSubtree)));
    return tree;
}

BinaryTree* BinaryTree_NewWithParent(void* userData, BinaryTree* parent)
{
    BinaryTree* tree = TOPUBLIC(new de::BinaryTree<void*>(userData, TOINTERNAL(parent)));
    return tree;
}

BinaryTree* BinaryTree_NewWithUserData(void* userData)
{
    BinaryTree* tree = TOPUBLIC(new de::BinaryTree<void*>(userData));
    return tree;
}

BinaryTree* BinaryTree_New(void)
{
    BinaryTree* tree = TOPUBLIC(new de::BinaryTree<void*>(NULL/*no user data*/));
    return tree;
}

void BinaryTree_Delete(BinaryTree* tree)
{
    SELF(tree);
    delete self;
}

size_t BinaryTree_Height(BinaryTree* tree)
{
    SELF(tree);
    return self->height();
}

boolean BinaryTree_IsLeaf(BinaryTree* tree)
{
    SELF(tree);
    return CPP_BOOL(self->isLeaf());
}

BinaryTree* BinaryTree_Parent(BinaryTree* tree)
{
    SELF(tree);
    return TOPUBLIC(self->parent());
}

boolean BinaryTree_HasParent(BinaryTree* tree)
{
    SELF(tree);
    return CPP_BOOL(self->hasParent());
}

BinaryTree* BinaryTree_SetParent(BinaryTree* tree, BinaryTree* parent)
{
    SELF(tree);
    return TOPUBLIC(&self->setParent(TOINTERNAL(parent)));
}

BinaryTree* BinaryTree_Child(BinaryTree* tree, boolean left)
{
    SELF(tree);
    return TOPUBLIC(self->child(TOCHILDID(left)));
}

BinaryTree* BinaryTree_SetChild(BinaryTree* tree, boolean left, BinaryTree* child)
{
    SELF(tree);
    return TOPUBLIC(&self->setChild(TOCHILDID(left), TOINTERNAL(child)));
}

boolean BinaryTree_HasChild(BinaryTree* tree, boolean left)
{
    SELF(tree);
    return CPP_BOOL(self->hasChild(TOCHILDID(left)));
}

void* BinaryTree_UserData(BinaryTree* tree)
{
    SELF(tree);
    return self->userData();
}

BinaryTree* BinaryTree_SetUserData(BinaryTree* tree, void* userData)
{
    SELF(tree);
    return TOPUBLIC(&self->setUserData(userData));
}

typedef struct {
    int (*callback) (BinaryTree*, void*);
    void* parameters;
} callback_wrapper_t;

int C_DECL callbackWrapper(de::BinaryTree<void*>& tree, void* parameters)
{
    callback_wrapper_t* p = static_cast<callback_wrapper_t*>(parameters);
    return p->callback(TOPUBLIC(&tree), p->parameters);
}

int BinaryTree_PreOrder(BinaryTree* tree, int (*callback) (BinaryTree*, void*), void* parameters)
{
    if(!tree || !callback) return false; // Continue iteration.

    callback_wrapper_t parm;
    parm.callback = callback;
    parm.parameters = parameters;

    SELF(tree);
    return de::BinaryTree<void*>::PreOrder(*self, callbackWrapper, &parm);
}

int BinaryTree_InOrder(BinaryTree* tree, int (*callback) (BinaryTree*, void*), void* parameters)
{
    if(!tree || !callback) return false; // Continue iteration.

    callback_wrapper_t parm;
    parm.callback = callback;
    parm.parameters = parameters;

    SELF(tree);
    return de::BinaryTree<void*>::InOrder(*self, callbackWrapper, &parm);
}

int BinaryTree_PostOrder(BinaryTree* tree, int (*callback) (BinaryTree*, void*), void* parameters)
{
    if(!tree || !callback) return false; // Continue iteration.

    callback_wrapper_t parm;
    parm.callback = callback;
    parm.parameters = parameters;

    SELF(tree);
    return de::BinaryTree<void*>::PostOrder(*self, callbackWrapper, &parm);
}
