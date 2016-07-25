/** @file binarytree_wrapper.cpp  Binary tree (C wrapper).
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/BinaryTree"
#include "de/c_wrapper.h"

#include <cstdlib>
#include <cassert>

namespace de {
    typedef de::BinaryTree<void *> VoidBinaryTree;
}

/// Helper macro for converting a boolean to child id.
#define CHILD_ID(v)     ((v)? de::VoidBinaryTree::Left : de::VoidBinaryTree::Right)

#define INTERNAL(inst)  reinterpret_cast<de::VoidBinaryTree *>(inst)
#define PUBLIC(inst)    reinterpret_cast<de_BinaryTree *>(inst)
#define SELF(inst)      DENG2_SELF(VoidBinaryTree, inst)

de_BinaryTree *BinaryTree_NewWithSubtrees(void *userData, de_BinaryTree *rightSubtree, de_BinaryTree *leftSubtree)
{
    de_BinaryTree *tree = PUBLIC(new de::BinaryTree<void*>(userData, 0/*no parent*/, INTERNAL(rightSubtree), INTERNAL(leftSubtree)));
    return tree;
}

de_BinaryTree *BinaryTree_NewWithParent(void *userData, de_BinaryTree *parent)
{
    de_BinaryTree *tree = PUBLIC(new de::BinaryTree<void*>(userData, INTERNAL(parent)));
    return tree;
}

de_BinaryTree *BinaryTree_NewWithUserData(void *userData)
{
    de_BinaryTree *tree = PUBLIC(new de::BinaryTree<void*>(userData));
    return tree;
}

de_BinaryTree *BinaryTree_New(void)
{
    de_BinaryTree *tree = PUBLIC(new de::BinaryTree<void*>(NULL/*no user data*/));
    return tree;
}

void BinaryTree_Delete(de_BinaryTree *tree)
{
    SELF(tree);
    delete self;
}

int BinaryTree_Height(de_BinaryTree *tree)
{
    SELF(tree);
    return int(self->height());
}

int BinaryTree_IsLeaf(de_BinaryTree *tree)
{
    SELF(tree);
    return self->isLeaf();
}

de_BinaryTree *BinaryTree_Parent(de_BinaryTree *tree)
{
    SELF(tree);
    return PUBLIC(self->parentPtr());
}

int BinaryTree_HasParent(de_BinaryTree *tree)
{
    SELF(tree);
    return self->hasParent();
}

de_BinaryTree *BinaryTree_SetParent(de_BinaryTree *tree, de_BinaryTree *parent)
{
    SELF(tree);
    return PUBLIC(&self->setParent(INTERNAL(parent)));
}

de_BinaryTree *BinaryTree_Child(de_BinaryTree *tree, int left)
{
    SELF(tree);
    return PUBLIC(self->childPtr(CHILD_ID(left)));
}

de_BinaryTree *BinaryTree_SetChild(de_BinaryTree *tree, int left, de_BinaryTree *child)
{
    SELF(tree);
    return PUBLIC(&self->setChild(CHILD_ID(left), INTERNAL(child)));
}

int BinaryTree_HasChild(de_BinaryTree *tree, int left)
{
    SELF(tree);
    return self->hasChild(CHILD_ID(left));
}

void *BinaryTree_UserData(de_BinaryTree *tree)
{
    SELF(tree);
    return self->userData();
}

de_BinaryTree *BinaryTree_SetUserData(de_BinaryTree *tree, void *userData)
{
    SELF(tree);
    return PUBLIC(&self->setUserData(userData));
}

namespace de { namespace internal {
    struct CallbackWrapper {
        int (*callback) (de_BinaryTree *, void *);
        void *parameters;
    };
}}

static int callbackWrapper(de::BinaryTree<void *> &tree, void *parameters)
{
    de::internal::CallbackWrapper *p = static_cast<de::internal::CallbackWrapper *>(parameters);
    return p->callback(PUBLIC(&tree), p->parameters);
}

int BinaryTree_PreOrder(de_BinaryTree *tree, int (*callback)(de_BinaryTree *, void *), void *parameters)
{
    if (!tree || !callback) return false; // Continue iteration.

    de::internal::CallbackWrapper parm;
    parm.callback = callback;
    parm.parameters = parameters;

    SELF(tree);
    return self->traversePreOrder(callbackWrapper, &parm);
}

int BinaryTree_InOrder(de_BinaryTree *tree, int (*callback)(de_BinaryTree *, void *), void *parameters)
{
    if (!tree || !callback) return false; // Continue iteration.

    de::internal::CallbackWrapper parm;
    parm.callback = callback;
    parm.parameters = parameters;

    SELF(tree);
    return self->traverseInOrder(callbackWrapper, &parm);
}

int BinaryTree_PostOrder(de_BinaryTree *tree, int (*callback)(de_BinaryTree *, void *), void *parameters)
{
    if (!tree || !callback) return false; // Continue iteration.

    de::internal::CallbackWrapper parm;
    parm.callback = callback;
    parm.parameters = parameters;

    SELF(tree);
    return self->traversePostOrder(callbackWrapper, &parm);
}
