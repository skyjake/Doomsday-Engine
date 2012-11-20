/**
 * @file binarytree.h
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

#include "dd_types.h"
#include <algorithm>
#include <assert.h>

namespace de {

#ifdef RIGHT
#undef RIGHT
#endif
#ifdef LEFT
#undef LEFT
#endif

template <typename value>
class BinaryTree
{
public:
    enum ChildId
    {
        RIGHT = 0,
        LEFT  = 1
    };

    static inline void assertValidChildId(ChildId DENG_DEBUG_ONLY(child))
    {
        assert(child == RIGHT || child == LEFT);
    }

    BinaryTree(value userData, BinaryTree* parent=0, BinaryTree* right=0, BinaryTree* left=0)
        : _parent(parent), rightChild(right), leftChild(left), userDataValue(userData)
    {}

    ~BinaryTree()
    {
        if(rightChild) delete rightChild;
        if(leftChild)  delete leftChild;
    }

    /**
     * Is this node a leaf?
     * @return  @c true iff this node is a leaf (no children).
     */
    inline bool isLeaf() const
    {
        return !rightChild && !leftChild;
    }

    /**
     * Retrieve the user data value associated with this node.
     * @return  User data pointer associated with this tree node else @c NULL.
     */
    value userData() const
    {
        return userDataValue;
    }

    /**
     * Set the user data value associated with this node.
     *
     * @param userData  User data value to set with this tree node.
     * @return  Reference to this BinaryTree.
     */
    BinaryTree& setUserData(value userData)
    {
        userDataValue = userData;
        return *this;
    }

    /**
     * Retrieve the parent tree node (if present).
     * @return  The parent tree node else @c NULL.
     */
    BinaryTree* parent() const
    {
        return _parent;
    }

    /// @c true iff this node has a parent node.
    inline bool hasParent() const { return 0 != parent(); }

    /**
     * Set the parent node of this node.
     *
     * @param parent  Parent node to be linked (can be @c NULL).
     * @return  Reference to this BinaryTree.
     */
    BinaryTree& setParent(BinaryTree* parent)
    {
        _parent = parent;
        return *this;
    }

    /**
     * Retrieve the identified child of this node (if present).
     *
     * @param child  Identifier of the child to return.
     * @return  The identified child if present else @c NULL.
     */
    BinaryTree* child(ChildId child) const
    {
        assertValidChildId(child);
        if(child) return leftChild;
        else      return rightChild;
    }

    /// Convenience methods for accessing the right and left subtrees respectively.
    inline BinaryTree* right() const { return child(RIGHT); }
    inline BinaryTree* left()  const { return child(LEFT);  }

    /// @c true iff this node has the specifed @a childId node.
    inline bool hasChild(ChildId childId) const { return 0 != child(childId); }

    /// @c true iff this node has a right child node.
    inline bool hasRight() const { return hasChild(RIGHT); }

    /// @c true iff this node has a left child node
    inline bool hasLeft()  const { return hasChild(LEFT); }

    /**
     * Set the specified node as a child of this node.
     *
     * @param child    Identifier of the child to return.
     * @param subtree  Child subtree to be linked (can be @c NULL).
     *
     * @return  Reference to this BinaryTree.
     */
    BinaryTree& setChild(ChildId child, BinaryTree* subtree)
    {
        assertValidChildId(child);
        if(child) leftChild  = subtree;
        else      rightChild = subtree;
        return *this;
    }

    /// Convenience methods for setting the right and left subtrees respectively.
    inline BinaryTree& setRight(BinaryTree* subtree) { return setChild(RIGHT, subtree); }
    inline BinaryTree& setLeft(BinaryTree* subtree)  { return setChild(LEFT,  subtree); }

    /**
     * Retrieve the height of this tree.
     * @return  Tree height.
     */
    size_t height() const
    {
        if(!isLeaf())
        {
            size_t right = rightChild? rightChild->height() : 0;
            size_t left  = leftChild ?  leftChild->height() : 0;
            return (right > left? right : left) + 1;
        }
        return 0;
    }

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
    static int PreOrder(BinaryTree& tree, int (*callback) (BinaryTree&, void*), void* parameters=NULL)
    {
        if(!callback) return false; // Continue iteration.

        // Visit this node.
        if(int result = callback(tree, parameters)) return result;

        if(!tree.isLeaf())
        {
            int result = PreOrder(*tree.right(), callback, parameters);
            if(result) return result;
        }

        if(tree.left())
        {
            int result = PreOrder(*tree.left(), callback, parameters);
            if(result) return result;
        }

        return false; // Continue iteration.
    }

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
    static int InOrder(BinaryTree& tree, int (*callback) (BinaryTree&, void*), void* parameters=NULL)
    {
        if(!callback) return false; // Continue iteration.

        if(tree.right())
        {
            int result = InOrder(*tree.right(), callback, parameters);
            if(result) return result;
        }

        // Visit this node.
        if(int result = callback(tree, parameters)) return result;

        if(tree.left())
        {
            int result = InOrder(*tree.left(), callback, parameters);
            if(result) return result;
        }

        return false; // Continue iteration.
    }

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
    static int PostOrder(BinaryTree& tree, int (*callback) (BinaryTree&, void*), void* parameters=NULL)
    {
        if(!callback) return false; // Continue iteration.

        if(tree.right())
        {
            int result = PostOrder(*tree.right(), callback, parameters);
            if(result) return result;
        }

        if(tree.left())
        {
            int result = PostOrder(*tree.left(), callback, parameters);
            if(result) return result;
        }

        // Visit this node.
        return callback(tree, parameters);
    }

private:
    /// Parent of this subtree (if any).
    BinaryTree* _parent;

    /// Subtrees.
    BinaryTree* rightChild, *leftChild;

    /// User data at this node.
    value userDataValue;
};

} // namespace de

#endif // __cplusplus

/**
 * C Wrapper implementation for a general purpose de::BinaryTree<void*>
 */

#include "dd_types.h"

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
 * @param parent  Parent node to associate with the new (sub)tree.
 * @return  New BinaryTree instance.
 */
BinaryTree* BinaryTree_NewWithParent(void* userData, BinaryTree* parent);

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

BinaryTree* BinaryTree_Parent(BinaryTree* tree);

boolean BinaryTree_HasParent(BinaryTree* tree);

BinaryTree* BinaryTree_SetParent(BinaryTree* tree, BinaryTree* parent);

/**
 * Given the specified node, return one of it's children.
 *
 * @param tree  BinaryTree instance.
 * @param left  @c true= retrieve the left child.
 *              @c false= retrieve the right child.
 * @return  The identified child if present else @c NULL.
 */
BinaryTree* BinaryTree_Child(BinaryTree* tree, boolean left);

#define BinaryTree_Right(tree) BinaryTree_Child((tree), false)
#define BinaryTree_Left(tree)  BinaryTree_Child((tree), true)

/**
 * Retrieve the user data associated with the specified (sub)tree.
 *
 * @param tree  BinaryTree instance.
 * @return  User data pointer associated with this tree node else @c NULL.
 */
void* BinaryTree_UserData(BinaryTree* tree);

/**
 * Set a child of the specified tree.
 *
 * @param tree  BinaryTree instance.
 * @param left  @c true= set the left child.
 *              @c false= set the right child.
 * @param subtree  Ptr to the (child) tree to be linked or @c NULL.
 */
BinaryTree* BinaryTree_SetChild(BinaryTree* tree, boolean left, BinaryTree* subtree);

#define BinaryTree_SetRight(tree, subtree) BinaryTree_SetChild((tree), false, (subtree))
#define BinaryTree_SetLeft(tree, subtree)  BinaryTree_SetChild((tree), true, (subtree))

boolean BinaryTree_HasChild(BinaryTree* tree, boolean left);

#define BinaryTree_HasRight(tree, subtree) BinaryTree_HasChild((tree), false)
#define BinaryTree_HasLeft(tree, subtree)  BinaryTree_HasChild((tree), true)

/**
 * Set the user data assoicated with the specified (sub)tree.
 *
 * @param tree  BinaryTree instance.
 * @param userData  User data pointer to associate with this tree node.
 */
BinaryTree *BinaryTree_SetUserData(BinaryTree* tree, void* userData);

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
int BinaryTree_PreOrder(BinaryTree* tree, int (*callback) (BinaryTree*, void*), void* parameters);

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
int BinaryTree_InOrder(BinaryTree* tree, int (*callback) (BinaryTree*, void*), void* parameters);

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
int BinaryTree_PostOrder(BinaryTree* tree, int (*callback) (BinaryTree*, void*), void* parameters);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_DATA_BINARYTREE
