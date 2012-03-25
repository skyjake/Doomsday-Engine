/**
 * @file m_binarytree.h
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

#ifndef LIBDENG_DATA_BINARYTREE
#define LIBDENG_DATA_BINARYTREE

#ifdef __cplusplus
extern "C" {
#endif

struct binarytree_s; // The binary tree instance (opaque).

/**
 * BinaryTree instance. Created with BinaryTree_New() or one of the other constructors.
 */
typedef struct binarytree_s BinaryTree;

/**
 * Create a new BinaryTree. The binary tree should be destroyed with
 * BinaryTree_Delete() once it is no longer needed.
 *
 * @return  New BinaryTree instance.
 */
BinaryTree* BinaryTree_New(void);

/**
 * Create a new BinaryTree.
 *
 * @param userData  User data to be associated with the new (sub)tree.
 * @return  New BinaryTree instance.
 */
BinaryTree* BinaryTree_NewWithUserData(void* userData);

/**
 * Create a new BinaryTree.
 *
 * @param userData  User data to be associated with the new (sub)tree.
 * @param rightSubtree  Right child subtree. Can be @a NULL.
 * @param leftSubtree   Left child subtree. Can be @c NULL.
 * @return  New BinaryTree instance.
 */
BinaryTree* BinaryTree_NewWithSubtrees(void* userData, BinaryTree* rightSubtree, BinaryTree* leftSubtree);

/**
 * Destroy the binary tree.
 * @param tree  BinaryTree instance.
 */
void BinaryTree_Delete(BinaryTree* tree);

/**
 * Given the specified node, return one of it's children.
 *
 * @param tree  BinaryTree instance.
 * @param left  @c true= retrieve the left child.
 *              @c false= retrieve the right child.
 * @return  The identified child if present else @c NULL.
 */
BinaryTree* BinaryTree_Child(BinaryTree* tree, boolean left);

/**
 * Retrieve the user data associated with the specified (sub)tree.
 *
 * @param tree  BinaryTree instance.
 *
 * @return  User data pointer associated with this tree node else @c NULL.
 */
void* BinaryTree_UserData(BinaryTree* tree);

/**
 * Set a child of the specified tree.
 *
 * @param tree  BinaryTree instance.
 * @param left  @c true= set the left child.
 *              @c false= set the right child.
 * @param subTree  Ptr to the (child) tree to be linked or @c NULL.
 */
void BinaryTree_SetChild(BinaryTree* tree, boolean left, BinaryTree* subtree);

/**
 * Set the user data assoicated with the specified (sub)tree.
 *
 * @param tree  BinaryTree instance.
 * @param userData  User data pointer to associate with this tree node.
 */
void BinaryTree_SetUserData(BinaryTree* tree, void* userData);

/**
 * Is this node a leaf?
 *
 * @param tree  BinaryTree instance.
 * @return  @c true iff this node is a leaf.
 */
boolean BinaryTree_IsLeaf(BinaryTree* tree);

/**
 * Calculate the height of the given tree.
 *
 * @param tree  BinaryTree instance.
 * @return  Height of the tree.
 */
size_t BinaryTree_Height(BinaryTree* tree);

/**
 * Standard traversals:
 */

/**
 * Traverse a binary tree in Preorder.
 *
 * Make a callback for all nodes of the tree (including the root). Traversal
 * continues until all nodes have been visited or a callback returns non-zero at
 * which point traversal is aborted.
 *
 * @param tree  BinaryTree instance.
 * @param callback  Function to call for each object of the tree.
 * @param parameters  Passed to the callback.
 *
 * @return  @c 0= iff all callbacks complete wholly else the return value of the
 * callback last made.
 */
int BinaryTree_PreOrder(BinaryTree* tree, int (C_DECL *callback) (BinaryTree*, void*), void* parameters);

/**
 * Traverse a binary tree in Inorder.
 *
 * Make a callback for all nodes of the tree (including the root). Traversal
 * continues until all nodes have been visited or a callback returns non-zero at
 * which point traversal is aborted.
 *
 * @param tree  BinaryTree instance.
 * @param callback  Function to call for each object of the tree.
 * @param parameters  Passed to the callback.
 *
 * @return  @c 0= iff all callbacks complete wholly else the return value of the
 * callback last made.
 */
int BinaryTree_InOrder(BinaryTree* tree, int (C_DECL *callback) (BinaryTree*, void*), void* parameters);

/**
 * Traverse a binary tree in Postorder.
 *
 * Make a callback for all nodes of the tree (including the root). Traversal
 * continues until all nodes have been visited or a callback returns non-zero at
 * which point traversal is aborted.
 *
 * @param tree  BinaryTree instance.
 * @param callback  Function to call for each object of the tree.
 * @param parameters  Passed to the callback.
 *
 * @return  @c 0= iff all callbacks complete wholly else the return value of the
 * callback last made.
 */
int BinaryTree_PostOrder(BinaryTree* tree, int (C_DECL *callback) (BinaryTree*, void*), void* parameters);

#ifdef __cplusplus
}
#endif

#endif /// LIBDENG_DATA_BINARYTREE
