/** @file binarytree.h Binary tree template.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_BINARYTREE_H
#define LIBCORE_BINARYTREE_H

#include "de/libcore.h"
#include "de/error.h"
#include "de/string.h"
#include <algorithm>

namespace de {

/**
 * Tree data structure where each node has a left and a right child.
 *
 * BinaryTree owns the child nodes and deletes them when the parent node is
 * deleted. Each node additionally has a templated data payload.
 *
 * @ingroup data
 */
template <typename Type>
class BinaryTree
{
public:
    /// The referenced parent is not present. @ingroup errors
    DE_ERROR(MissingParentError);

    /// The referenced child is not present. @ingroup errors
    DE_ERROR(MissingChildError);

    /**
     * Child node identifiers.
     */
    enum ChildId
    {
        Right, Left
    };

private:
    static inline void assertValidChildId(ChildId DE_DEBUG_ONLY(child))
    {
        DE_ASSERT(child == Right || child == Left);
    }

public:
    /**
     * Constructs a new binary subtree.
     *
     * @param userData  User data value for the node.
     * @param parent    Parent node of this node.
     * @param right     Right child of this node. This node takes ownership.
     * @param left      Left child of this node. This node takes ownership.
     */
    BinaryTree(const Type &userData = Type(), BinaryTree *parent = 0, BinaryTree *right = 0, BinaryTree *left = 0)
        : _parent(parent), _rightChild(right), _leftChild(left), _userDataValue(userData) {}

    virtual ~BinaryTree()
    {
        clear();
    }

    /**
     * Removes both branches of the tree.
     */
    void clear()
    {
        delete _rightChild; _rightChild = 0;
        delete _leftChild;  _leftChild  = 0;
    }

    /**
     * Is this node a leaf?
     * @return  @c true iff this node is a leaf (no children).
     */
    inline bool isLeaf() const
    {
        return !_rightChild && !_leftChild;
    }

    /**
     * Retrieve the user data value associated with this node.
     * @return  User data pointer associated with this tree node else @c NULL.
     */
    Type userData() const
    {
        return _userDataValue;
    }

    /**
     * Set the user data value associated with this node.
     *
     * @param userData  User data value to set with this tree node.
     * @return  Reference to this BinaryTree.
     */
    BinaryTree &setUserData(Type userData)
    {
        _userDataValue = userData;
        return *this;
    }

    /**
     * Returns @c true iff subtree node has a parent node.
     */
    inline bool hasParent() const { return _parent != 0; }

    /**
     * Returns the parent of the subtree.
     *
     * @see hasChild()
     */
    BinaryTree &parent() const
    {
        if (_parent)
        {
            return *_parent;
        }
        /// @throw MissingParentError Attempted with no parent linked.
        throw MissingParentError("BinaryTree::parent", "No parent is linked");
    }

    /**
     * Returns a pointer to the parent of the subtree.
     *
     * @return  Pointer to the parent; otherwise @c 0.
     *
     * @see hasParent(), parent()
     */
    inline BinaryTree *parentPtr() const
    {
        return _parent;
    }

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
     * Returns @c true iff this node has the specifed @a childId node.
     */
    inline bool hasChild(ChildId which) const
    {
        assertValidChildId(which);
        if (which) return _leftChild  != 0;
        else      return _rightChild != 0;
    }

    /**
     * Returns @c true iff a Right child subtree is linked to the binary tree.
     */
    inline bool hasRight() const { return hasChild(Right); }

    /**
     * Returns @c true iff a Right child subtree is linked to the binary tree.
     */
    inline bool hasLeft()  const { return hasChild(Left); }

    /**
     * Convenient method for determining if a leaf is linked as the specified
     * child subtree of the binary tree.
     *
     * @param which     ChildId identifier of the child to inspect.
     *
     * @see hasChild(), isLeaf()
     */
    inline bool hasChildLeaf(ChildId which) const
    {
        return hasChild(which) && child(which).isLeaf();
    }

    /**
     * Convenient method for determining if a leaf is linked as the Right child
     * of the binary tree.
     *
     * @see hasChildLeaf()
     */
    inline bool hasRightLeaf() const { return hasChildLeaf(Right); }

    /**
     * Convenient method for determining if a leaf is linked as the Left child
     * of the binary tree.
     *
     * @see hasChildLeaf()
     */
    inline bool hasLeftLeaf() const { return hasChildLeaf(Left); }

    /**
     * Convenient method for determining if a subtree is linked as the specified
     * child of the binary tree.
     *
     * @param which     ChildId identifier of the child to inspect.
     *
     * @see hasChild(), isLeaf()
     */
    inline bool hasChildSubtree(ChildId which) const
    {
        return hasChild(which) && !child(which).isLeaf();
    }

    /**
     * Convenient method for determining if a subtree is linked as the Right
     * child of the binary tree.
     *
     * @see hasChildSubtree()
     */
    inline bool hasRightSubtree() const { return hasChildSubtree(Right); }

    /**
     * Convenient method for determining if a subtree is linked as the Left
     * child of the binary tree.
     *
     * @see hasChildSubtree()
     */
    inline bool hasLeftSubtree() const { return hasChildSubtree(Left); }

    /**
     * Returns the identified child subtree of the binary tree.
     *
     * @param which     ChildId identifier of the child to return.
     *
     * @return          The identified child subtree.
     *
     * @see hasChild()
     */
    BinaryTree &child(ChildId which) const
    {
        assertValidChildId(which);
        BinaryTree * const *adr = which? &_leftChild : &_rightChild;
        if (*adr)
        {
            return **adr;
        }
        /// @throw MissingChildError Attempted with no child linked.
        throw MissingChildError("BinaryTree::child",
                                stringf("No %s child is linked", which ? "Left" : "Right"));
    }

    /**
     * Returns the Right child subtree of the binary tree.
     *
     * @see child()
     */
    inline BinaryTree &right() const { return child(Right); }

    /**
     * Returns the Left child subtree of the binary tree.
     *
     * @see child()
     */
    inline BinaryTree &left() const { return child(Left); }

    /**
     * Returns a pointer to the identified child of the node.
     *
     * @param which     Identifier of the child to return.
     *
     * @return  Pointer to the identified child; otherwise @c 0.
     *
     * @see hasChild(), child()
     */
    inline BinaryTree *childPtr(ChildId which) const
    {
        return which == Left? _leftChild : _rightChild;
    }

    /**
     * Returns a pointer to the Right subtree of the binary tree.
     *
     * @return  Pointer to the Right subtree; otherwise @c 0.
     *
     * @see childPtr()
     */
    inline BinaryTree *rightPtr() const { return childPtr(Right); }

    /**
     * Returns a pointer to the Left subtree of the binary tree.
     *
     * @return  Pointer to the Left subtree; otherwise @c 0.
     *
     * @see childPtr()
     */
    inline BinaryTree *leftPtr()  const { return childPtr(Left);  }

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
        if (child == Left)
            _leftChild = subtree;
        else
            _rightChild = subtree;
        return *this;
    }

    /// Convenience methods for setting the right and left subtrees respectively.
    inline BinaryTree &setRight(BinaryTree *subtree) { return setChild(Right, subtree); }
    inline BinaryTree &setLeft(BinaryTree *subtree)  { return setChild(Left,  subtree); }

    /**
     * Retrieve the height of this tree.
     * @return  Tree height.
     */
    dsize height() const
    {
        if (!isLeaf())
        {
            size_t right = _rightChild? _rightChild->height() : 0;
            size_t left  = _leftChild ?  _leftChild->height() : 0;
            return (right > left? right : left) + 1;
        }
        return 0;
    }

    /**
     * Determine the total number of nodes in the tree.
     */
    int nodeCount() const
    {
        countsubtreeworker_params_t parm;
        parm.countNodes = true;
        parm.countLeafs = false;
        parm.total      = 0;
        const_cast<BinaryTree *>(this)->traversePreOrder(countSubtreeWorker, &parm);
        return parm.total;
    }

    /**
     * Determine the total number of leafs in the tree.
     */
    int leafCount() const
    {
        countsubtreeworker_params_t parm;
        parm.countNodes = false;
        parm.countLeafs = true;
        parm.total      = 0;
        const_cast<BinaryTree *>(this)->traversePreOrder(countSubtreeWorker, &parm);
        return parm.total;
    }

    /**
     * Makes a copy of another tree. The structure of the other tree is
     * duplicated in full, with copies of its user data.
     *
     * @param other  Tree to make copy of.
     */
    BinaryTree &operator = (const BinaryTree &other)
    {
        // Remove old children.
        clear();

        setUserData(other.userData());

        if (other.hasLeft())
        {
            setLeft(new BinaryTree);
            left() = other.left();
            left().setParent(this);
        }
        if (other.hasRight())
        {
            setRight(new BinaryTree);
            right() = other.right();
            right().setParent(this);
        }
        return *this;
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
    int traversePreOrder(int (*callback) (BinaryTree &, void *), void *parameters = nullptr)
    {
        if (!callback) return false; // Continue iteration.

        // Visit this node.
        if (int result = callback(*this, parameters)) return result;

        if (hasRight())
        {
            int result = right().traversePreOrder(callback, parameters);
            if (result) return result;
        }

        if (hasLeft())
        {
            int result = left().traversePreOrder(callback, parameters);
            if (result) return result;
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
    int traverseInOrder(int (*callback) (BinaryTree &, void *), void *parameters = nullptr)
    {
        if (!callback) return false; // Continue iteration.

        if (hasRight())
        {
            int result = right().traverseInOrder(callback, parameters);
            if (result) return result;
        }

        // Visit this node.
        if (int result = callback(*this, parameters)) return result;

        if (hasLeft())
        {
            int result = left().traverseInOrder(callback, parameters);
            if (result) return result;
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
    int traversePostOrder(int (*callback) (BinaryTree &, void *), void *parameters = nullptr)
    {
        if (!callback) return false; // Continue iteration.

        if (hasRight())
        {
            int result = right().traversePostOrder(callback, parameters);
            if (result) return result;
        }

        if (hasLeft())
        {
            int result = left().traversePostOrder(callback, parameters);
            if (result) return result;
        }

        // Visit this node.
        return callback(*this, parameters);
    }

    /**
     * Provides a textual summary of the tree structure.
     */
    String summary() const
    {
        String text = Stringf("%i nodes, %i leafs", nodeCount(), leafCount());
        if (!isLeaf())
        {
            text += Stringf(" (balance is %zu:%zu)",
                                   hasRight() ? right().height() : 0,
                                   hasLeft()  ? left().height()  : 0);
        }
        return text;
    }

private:
    struct countsubtreeworker_params_t
    {
        bool countNodes, countLeafs;
        int total;
    };

    static int countSubtreeWorker(BinaryTree &subtree, void *context)
    {
        countsubtreeworker_params_t &p = *static_cast<countsubtreeworker_params_t *>(context);
        if (subtree.isLeaf() && p.countLeafs)
        {
            p.total++;
        }
        if (!subtree.isLeaf() && p.countNodes)
        {
            p.total++;
        }
        return false; // Continue iteration.
    }

    /// Parent of this subtree (if any).
    BinaryTree *_parent;

    /// Subtrees (owned).
    BinaryTree *_rightChild, *_leftChild;

    /// User data at this node.
    Type _userDataValue;
};

} // namespace de

#endif // LIBCORE_BINARYTREE_H
