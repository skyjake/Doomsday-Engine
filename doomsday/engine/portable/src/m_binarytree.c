/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * m_binarytree.c: A fairly standard binary tree implementation.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "m_binarytree.h"

// MACROS ------------------------------------------------------------------

#define RIGHT                   0
#define LEFT                    1

#define IS_LEAF(node) (!((node)->children[RIGHT] || (node)->children[LEFT]))

// TYPES -------------------------------------------------------------------

typedef struct treenode_s {
    void               *data;
    struct treenode_s  *children[2]; // {RIGHT, LEFT}
} treenode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static __inline treenode_t *allocNode(void)
{
    return malloc(sizeof(treenode_t));
}

static __inline void freeNode(treenode_t *node)
{
    free(node);
}

static treenode_t *createTree(const void *userData, treenode_t *right,
                              treenode_t *left)
{
    treenode_t         *n;

    n = allocNode();
    n->data = (void*) userData;
    n->children[RIGHT] = right;
    n->children[LEFT] = left;

    return n;
}

static void destroyTree(treenode_t *tree)
{
    treenode_t         *n;

    if(!tree)
        return;

    n = (treenode_t*) tree;
    destroyTree(n->children[RIGHT]);
    destroyTree(n->children[LEFT]);

    freeNode(n);
}

static size_t getHeight(const treenode_t *n)
{
    if(n && !IS_LEAF(n))
    {
        size_t      left, right;

        right = getHeight(n->children[RIGHT]);
        left  = getHeight(n->children[LEFT]);

        return MAX_OF(left, right) + 1;
    }

    return 0;
}

static boolean preOrder(treenode_t *n,
                        boolean (C_DECL *func) (binarytree_t *tree, void *data),
                        void *data)
{
    if(!n)
        return true;

    // Visit this node.
    if(!func((binarytree_t*) n, data))
        return false;

    if(!IS_LEAF(n))
    {
        if(!preOrder(n->children[RIGHT], func, data))
            return false;

        if(!preOrder(n->children[LEFT], func, data))
            return false;
    }

    return true;
}

static boolean inOrder(treenode_t *n,
                       boolean (C_DECL *func) (binarytree_t *tree, void *data),
                       void *data)
{
    if(!n)
        return true;

    if(!IS_LEAF(n))
    {
        if(!inOrder(n->children[RIGHT], func, data))
            return false;
    }

    // Visit this node.
    if(!func((binarytree_t*) n, data))
        return false;

    if(!IS_LEAF(n))
    {
        if(!inOrder(n->children[LEFT], func, data))
            return false;
    }

    return true;
}

static boolean postOrder(treenode_t *n,
                         boolean (C_DECL *func) (binarytree_t *tree, void *data),
                         void *data)
{
    if(!n)
        return true;

    if(!IS_LEAF(n))
    {
        if(!postOrder(n->children[RIGHT], func, data))
            return false;

        if(!postOrder(n->children[LEFT], func, data))
            return false;
    }

    // Visit this node.
    if(!func((binarytree_t*) n, data))
        return false;

    return true;
}

/**
 * Create a new binary (sub)tree.
 *
 * @param data          User-data to be associated with the new (sub)tree.
 *
 * @return              Ptr to the newly created (sub)tree.
 */
binarytree_t *BinaryTree_Create(const void *data)
{
    return (binarytree_t*) createTree(data, NULL, NULL);
}

/**
 * Create a new binary (sub)tree.
 *
 * @param data          User-data to be associated with the new (sub)tree.
 * @param initialRight  Ptr to the (sub)tree to link as the right child.
 * @param initialLeft   Ptr to the (sub)tree to link as the left child.
 *
 * @return              Ptr to the newly created (sub)tree.
 */
binarytree_t *BinaryTree_Create2(const void *data, binarytree_t *initialRight,
                                 binarytree_t *initialLeft)
{
    return (binarytree_t*) createTree(data, (treenode_t*) initialRight,
                                      (treenode_t*) initialLeft);
}

/**
 * Destroy the given binary tree.
 *
 * @param tree          Ptr to the tree to be destroyed.
 */
void BinaryTree_Destroy(binarytree_t *tree)
{
    if(!tree)
        return;

    destroyTree((treenode_t*) tree);
}

/**
 * Calculate the height of the given tree.
 *
 * @param rootNode      Node to begin at.
 *
 * @return              Height of the tree.
 */
size_t BinaryTree_GetHeight(binarytree_t *tree)
{
    if(!tree)
        return 0;

    return getHeight((treenode_t*) tree);
}

/**
 * Is this node a leaf?
 *
 * @param node          Ptr to the node to test.
 * @return              @c true iff this node is a leaf.
 */
boolean BinaryTree_IsLeaf(binarytree_t *tree)
{
    if(!tree)
        return false;

    return IS_LEAF((treenode_t*) tree);
}

/**
 * Given the specified node, return one of it's children.
 *
 * @param node          Ptr to the parent.
 * @param left          @c true= retrieve the left child.
 *                      @c false= retrieve the right child.
 * @return              Ptr to the requested child if present ELSE @c NULL.
 */
binarytree_t *BinaryTree_GetChild(binarytree_t *tree, boolean left)
{
    treenode_t   *n;

    if(!tree)
        return NULL;

    n = (treenode_t*) tree;
    return (binarytree_t*) n->children[left? LEFT : RIGHT];
}

/**
 * Set a child of the specified tree.
 *
 * @param tree          Ptr to the (parent) tree to have its child set.
 * @param left          @c true= set the left child.
 *                      @c false= set the right child.
 * @param subTree       Ptr to the (child) tree to be linked or @c NULL.
 */
void BinaryTree_SetChild(binarytree_t *tree, boolean left,
                         binarytree_t *subTree)
{
    treenode_t         *parent, *child;

    if(!tree)
        return;

    parent = (treenode_t*) tree;
    child = (treenode_t*) subTree;
    parent->children[left? LEFT : RIGHT] = child;
}

/**
 * Retrieve the user data associated with the specified (sub)tree.
 *
 * @param               Ptr to the node.
 *
 * @return              Ptr to the user data if present ELSE @c NULL.
 */
void *BinaryTree_GetData(binarytree_t *tree)
{
    treenode_t   *n;

    if(!tree)
        return NULL;

    n = (treenode_t*) tree;
    return n->data;
}

/**
 * Set the user data assoicated with the specified (sub)tree.
 *
 * @param tree          Ptr to the tree.
 * @param data          Ptr to the user data.
 */
void BinaryTree_SetData(binarytree_t *tree, const void *data)
{
    treenode_t         *n;

    if(!tree)
        return;

    n = (treenode_t*) tree;
    n->data = (void*) data;
}

/**
 * Traverse a binary tree in Preorder.
 *
 * Make a callback for all nodes of the tree (including the root).
 * Traversal continues until all nodes have been visited or a callback
 * returns @c false; at which point traversal is aborted.
 *
 * @param tree          Ptr to the (sub)tree to be traversed.
 * @param callback      Function to call for each object of the tree.
 * @param data          Used to pass additional data to func.
 *
 * @return              @c true, iff all callbacks return @c true;
 */
boolean BinaryTree_PreOrder(binarytree_t *tree,
                            boolean (C_DECL *callback) (binarytree_t *tree, void *data),
                            void *data)
{
    if(!tree)
        return true;

    return preOrder((treenode_t*) tree, callback, data);
}

/**
 * Traverse a binary tree in Inorder.
 *
 * Make a callback for all nodes of the tree (including the root).
 * Traversal continues until all nodes have been visited or a callback
 * returns @c false; at which point traversal is aborted.
 *
 * @param tree          Ptr to the (sub)tree to be traversed.
 * @param callback      Function to call for each object of the tree.
 * @param data          Used to pass additional data to func.
 *
 * @return              @c true, iff all callbacks return @c true;
 */
boolean BinaryTree_InOrder(binarytree_t *tree,
                           boolean (C_DECL *callback) (binarytree_t *tree, void *data),
                           void *data)
{
    if(!tree)
        return true;

    return inOrder((treenode_t*) tree, callback, data);
}

/**
 * Traverse a binary tree in Postorder.
 *
 * Make a callback for all nodes of the tree (including the root).
 * Traversal continues until all nodes have been visited or a callback
 * returns @c false; at which point traversal is aborted.
 *
 * @param tree          Ptr to the (sub)tree to be traversed.
 * @param callback      Function to call for each object of the tree.
 * @param data          Used to pass additional data to func.
 *
 * @return              @c true, iff all callbacks return @c true;
 */
boolean BinaryTree_PostOrder(binarytree_t *tree,
                             boolean (C_DECL *callback) (binarytree_t *tree, void *data),
                             void *data)
{
    if(!tree)
        return true;

    return postOrder((treenode_t*) tree, callback, data);
}
