/** @file binarytree.h Binary tree template.
 * @ingroup data
 *
 * @authors Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_BINARYTREE_H
#define LIBDENG2_BINARYTREE_H

#include "../libdeng2.h"
#include <algorithm>
#include <assert.h>

namespace de {

/**
 * Tree data structure where each node has a left and a right child.
 *
 * BinaryTree owns the child nodes and deletes them when the parent node is
 * deleted. Each node additionally has a templated data payload.
 */
template <typename Type>
class BinaryTree
{
public:
    enum ChildId { Right, Left };

    static inline void assertValidChildId(ChildId DENG2_DEBUG_ONLY(child))
    {
        assert(child == Right || child == Left);
    }

    /**
     * Construct a tree node.
     *
     * @param userData  User data value for the node.
     * @param parent    Parent node of this node.
     * @param right     Right child of this node. This node takes ownership.
     * @param left      Left child of this node. This node takes ownership.
     */
    BinaryTree(Type const &userData, BinaryTree *parent=0, BinaryTree *right=0, BinaryTree *left=0)
        : _parent(parent), rightChild(right), leftChild(left), userDataValue(userData)
    {}

    virtual ~BinaryTree()
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
    Type userData() const
    {
        return userDataValue;
    }

    /**
     * Set the user data value associated with this node.
     *
     * @param userData  User data value to set with this tree node.
     * @return  Reference to this BinaryTree.
     */
    BinaryTree &setUserData(Type userData)
    {
        userDataValue = userData;
        return *this;
    }

    /**
     * Retrieve the parent tree node (if present).
     * @return  The parent tree node else @c NULL.
     */
    BinaryTree *parent() const
    {
        return _parent;
    }

    /// @c true iff this node has a parent node.
    inline bool hasParent() const { return parent() != 0; }

    /**
     * Set the parent node of this node.
     *
     * @param parent  Parent node to be linked (can be @c NULL).
     * @return  Reference to this BinaryTree.
     */
    BinaryTree &setParent(BinaryTree *parent)
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
    BinaryTree *child(ChildId child) const
    {
        assertValidChildId(child);
        if(child) return leftChild;
        else      return rightChild;
    }

    /// Convenience methods for accessing the right and left subtrees respectively.
    inline BinaryTree *right() const { return child(Right); }
    inline BinaryTree *left()  const { return child(Left);  }

    /// @c true iff this node has the specifed @a childId node.
    inline bool hasChild(ChildId childId) const { return child(childId) != 0; }

    /// @c true iff this node has a right child node.
    inline bool hasRight() const { return hasChild(Right); }

    /// @c true iff this node has a left child node
    inline bool hasLeft()  const { return hasChild(Left); }

    /**
     * Set the specified node as a child of this node.
     *
     * @param child    Identifier of the child to return.
     * @param subtree  Child subtree to be linked (can be @c NULL).
     *
     * @return  Reference to this BinaryTree.
     */
    BinaryTree &setChild(ChildId child, BinaryTree *subtree)
    {
        assertValidChildId(child);
        if(child) leftChild  = subtree;
        else      rightChild = subtree;
        return *this;
    }

    /// Convenience methods for setting the right and left subtrees respectively.
    inline BinaryTree &setRight(BinaryTree *subtree) { return setChild(Right, subtree); }
    inline BinaryTree &setLeft(BinaryTree *subtree)  { return setChild(Left,  subtree); }

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
     * @param callback  Function to call for each object of the tree.
     * @param parameters  Passed to the callback.
     *
     * @return  @c 0= iff all callbacks complete wholly else the return value of the
     * callback last made.
     */
    int traversePreOrder(int (*callback) (BinaryTree &, void *), void *parameters = 0)
    {
        if(!callback) return false; // Continue iteration.

        // Visit this node.
        if(int result = callback(*this, parameters)) return result;

        if(!isLeaf())
        {
            int result = right()->traversePreOrder(callback, parameters);
            if(result) return result;
        }

        if(left())
        {
            int result = left()->traversePreOrder(callback, parameters);
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
     * @param callback  Function to call for each object of the tree.
     * @param parameters  Passed to the callback.
     *
     * @return  @c 0= iff all callbacks complete wholly else the return value of the
     * callback last made.
     */
    int traverseInOrder(int (*callback) (BinaryTree &, void *), void *parameters = 0)
    {
        if(!callback) return false; // Continue iteration.

        if(right())
        {
            int result = right()->traverseInOrder(callback, parameters);
            if(result) return result;
        }

        // Visit this node.
        if(int result = callback(*this, parameters)) return result;

        if(left())
        {
            int result = left()->traverseInOrder(callback, parameters);
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
     * @param callback  Function to call for each object of the tree.
     * @param parameters  Passed to the callback.
     *
     * @return  @c 0= iff all callbacks complete wholly else the return value of the
     * callback last made.
     */
    int traversePostOrder(int (*callback) (BinaryTree &, void *), void *parameters = 0)
    {
        if(!callback) return false; // Continue iteration.

        if(right())
        {
            int result = right()->traversePostOrder(callback, parameters);
            if(result) return result;
        }

        if(left())
        {
            int result = left()->traversePostOrder(callback, parameters);
            if(result) return result;
        }

        // Visit this node.
        return callback(*this, parameters);
    }

private:
    /// Parent of this subtree (if any).
    BinaryTree *_parent;

    /// Subtrees (owned).
    BinaryTree *rightChild, *leftChild;

    /// User data at this node.
    Type userDataValue;
};

} // namespace de

#endif /// LIBDENG2_BINARYTREE_H
