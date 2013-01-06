/** @file binarytree_wrapper.cpp Binary tree (C wrapper).
 * @ingroup data
 *
 * @authors Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#define PUBLIC(inst)    reinterpret_cast<BinaryTree *>(inst)
#define SELF(inst)      DENG2_SELF(VoidBinaryTree, inst)

BinaryTree *BinaryTree_NewWithSubtrees(void *userData, BinaryTree *rightSubtree, BinaryTree *leftSubtree)
{
    BinaryTree *tree = PUBLIC(new de::BinaryTree<void*>(userData, 0/*no parent*/, INTERNAL(rightSubtree), INTERNAL(leftSubtree)));
    return tree;
}

BinaryTree *BinaryTree_NewWithParent(void *userData, BinaryTree *parent)
{
    BinaryTree *tree = PUBLIC(new de::BinaryTree<void*>(userData, INTERNAL(parent)));
    return tree;
}

BinaryTree *BinaryTree_NewWithUserData(void *userData)
{
    BinaryTree *tree = PUBLIC(new de::BinaryTree<void*>(userData));
    return tree;
}

BinaryTree *BinaryTree_New(void)
{
    BinaryTree *tree = PUBLIC(new de::BinaryTree<void*>(NULL/*no user data*/));
    return tree;
}

void BinaryTree_Delete(BinaryTree *tree)
{
    SELF(tree);
    delete self;
}

int BinaryTree_Height(BinaryTree *tree)
{
    SELF(tree);
    return self->height();
}

int BinaryTree_IsLeaf(BinaryTree *tree)
{
    SELF(tree);
    return self->isLeaf();
}

BinaryTree *BinaryTree_Parent(BinaryTree *tree)
{
    SELF(tree);
    return PUBLIC(self->parent());
}

int BinaryTree_HasParent(BinaryTree *tree)
{
    SELF(tree);
    return self->hasParent();
}

BinaryTree *BinaryTree_SetParent(BinaryTree *tree, BinaryTree *parent)
{
    SELF(tree);
    return PUBLIC(&self->setParent(INTERNAL(parent)));
}

BinaryTree *BinaryTree_Child(BinaryTree *tree, int left)
{
    SELF(tree);
    return PUBLIC(self->child(CHILD_ID(left)));
}

BinaryTree *BinaryTree_SetChild(BinaryTree *tree, int left, BinaryTree *child)
{
    SELF(tree);
    return PUBLIC(&self->setChild(CHILD_ID(left), INTERNAL(child)));
}

int BinaryTree_HasChild(BinaryTree *tree, int left)
{
    SELF(tree);
    return self->hasChild(CHILD_ID(left));
}

void *BinaryTree_UserData(BinaryTree *tree)
{
    SELF(tree);
    return self->userData();
}

BinaryTree *BinaryTree_SetUserData(BinaryTree *tree, void *userData)
{
    SELF(tree);
    return PUBLIC(&self->setUserData(userData));
}

namespace de { namespace internal {
    struct CallbackWrapper {
        int (*callback) (::BinaryTree *, void *);
        void *parameters;
    };
}}

static int callbackWrapper(de::BinaryTree<void *> &tree, void *parameters)
{
    de::internal::CallbackWrapper *p = static_cast<de::internal::CallbackWrapper *>(parameters);
    return p->callback(PUBLIC(&tree), p->parameters);
}

int BinaryTree_PreOrder(BinaryTree *tree, int (*callback)(BinaryTree *, void *), void *parameters)
{
    if(!tree || !callback) return false; // Continue iteration.

    de::internal::CallbackWrapper parm;
    parm.callback = callback;
    parm.parameters = parameters;

    SELF(tree);
    return self->traversePreOrder(callbackWrapper, &parm);
}

int BinaryTree_InOrder(BinaryTree *tree, int (*callback)(BinaryTree *, void *), void *parameters)
{
    if(!tree || !callback) return false; // Continue iteration.

    de::internal::CallbackWrapper parm;
    parm.callback = callback;
    parm.parameters = parameters;

    SELF(tree);
    return self->traverseInOrder(callbackWrapper, &parm);
}

int BinaryTree_PostOrder(BinaryTree *tree, int (*callback)(BinaryTree *, void *), void *parameters)
{
    if(!tree || !callback) return false; // Continue iteration.

    de::internal::CallbackWrapper parm;
    parm.callback = callback;
    parm.parameters = parameters;

    SELF(tree);
    return self->traversePostOrder(callbackWrapper, &parm);
}
