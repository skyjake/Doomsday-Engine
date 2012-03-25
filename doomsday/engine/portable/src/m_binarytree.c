/**
 * @file m_binarytree.c
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

#include <assert.h>

#include "de_platform.h"
#include "de_console.h"

#include "m_binarytree.h"

enum { RIGHT = 0, LEFT };

struct binarytree_s {
    /// {RIGHT, LEFT} subtrees.
    struct binarytree_s* children[2];

    /// User data pointer at this node.
    void* userData;
};

BinaryTree* BinaryTree_NewWithSubtrees(void* userData, BinaryTree* rightSubtree, BinaryTree* leftSubtree)
{
    BinaryTree* tree = malloc(sizeof *tree);
    if(!tree) Con_Error("BinaryTree_NewWithUserData: Failed on allocation of %lu bytes for new BinaryTree.", (unsigned long) sizeof *tree);

    tree->children[RIGHT] = rightSubtree;
    tree->children[LEFT] = leftSubtree;
    tree->userData = userData;

    return tree;
}

BinaryTree* BinaryTree_NewWithUserData(void* userData)
{
    return BinaryTree_NewWithSubtrees(userData, NULL/*no right subtree*/, NULL/*no left subtree*/);
}

BinaryTree* BinaryTree_New(void)
{
    return BinaryTree_NewWithUserData(NULL/*no user data*/);
}

void BinaryTree_Delete(BinaryTree* tree)
{
    assert(tree);
    if(tree->children[RIGHT]) BinaryTree_Delete(tree->children[RIGHT]);
    if(tree->children[LEFT]) BinaryTree_Delete(tree->children[LEFT]);
    free(tree);
}

size_t BinaryTree_Height(BinaryTree* tree)
{
    assert(tree);
    if(!BinaryTree_IsLeaf(tree))
    {
        size_t right = BinaryTree_Height(tree->children[RIGHT]);
        size_t left = BinaryTree_Height(tree->children[LEFT]);
        return MAX_OF(left, right) + 1;
    }
    return 0;
}

boolean BinaryTree_IsLeaf(BinaryTree* tree)
{
    assert(tree);
    return !tree->children[RIGHT] && !tree->children[LEFT];
}

BinaryTree* BinaryTree_Child(BinaryTree* tree, boolean left)
{
    assert(tree);
    return tree->children[left? LEFT : RIGHT];
}

void BinaryTree_SetChild(BinaryTree* tree, boolean left, BinaryTree* child)
{
    assert(tree);
    tree->children[left? LEFT : RIGHT] = child;
}

void* BinaryTree_UserData(BinaryTree* tree)
{
    assert(tree);
    return tree->userData;
}

void BinaryTree_SetUserData(BinaryTree* tree, void* userData)
{
    assert(tree);
    tree->userData = userData;
}

int BinaryTree_PreOrder(BinaryTree* tree, int (C_DECL *callback) (BinaryTree*, void*), void* parameters)
{
    int result;
    if(!tree || !callback) return false;

    // Visit this node.
    result = callback(tree, parameters);
    if(result) return result;

    if(!BinaryTree_IsLeaf(tree))
    {
        result = BinaryTree_PreOrder(tree->children[RIGHT], callback, parameters);
        if(result) return result;

        result = BinaryTree_PreOrder(tree->children[LEFT], callback, parameters);
        if(result) return result;
    }

    return false; // Continue iteration.
}

int BinaryTree_InOrder(BinaryTree* tree, int (C_DECL *callback) (BinaryTree*, void*), void* parameters)
{
    int result;
    if(!tree || !callback) return false; // Continue iteration.

    if(!BinaryTree_IsLeaf(tree))
    {
        result = BinaryTree_InOrder(tree->children[RIGHT], callback, parameters);
        if(result) return result;
    }

    // Visit this node.
    result = callback(tree, parameters);
    if(result) return result;

    if(!BinaryTree_IsLeaf(tree))
    {
        result = BinaryTree_InOrder(tree->children[LEFT], callback, parameters);
        if(result) return result;
    }

    return false; // Continue iteration.
}

int BinaryTree_PostOrder(BinaryTree* tree, int (C_DECL *callback) (BinaryTree*, void*), void* parameters)
{
    if(!tree || !callback) return false;

    if(!BinaryTree_IsLeaf(tree))
    {
        int result = BinaryTree_PostOrder(tree->children[RIGHT], callback, parameters);
        if(result) return result;

        result = BinaryTree_PostOrder(tree->children[LEFT], callback, parameters);
        if(result) return result;
    }

    // Visit this node.
    return callback(tree, parameters);
}
