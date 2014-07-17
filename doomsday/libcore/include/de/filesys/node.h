/** @file filesys/node.h  File system node.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_FILESYS_NODE_H
#define LIBDENG2_FILESYS_NODE_H

#include "../String"
#include "../Path"
#include "../Lockable"

namespace de {
namespace filesys {

/**
 * File system node. Base class for a file.
 * @ingroup fs
 *
 * @par Thread-safety
 *
 * All nodes are Lockable so that multiple threads can use them simultaneously. As a
 * general rule, the user of a node does not need to lock the file manually; nodes will
 * lock themselves as appropriate. A user may lock the node manually if long-term
 * exclusive access is required.
 */
class DENG2_PUBLIC Node : public Lockable
{
public:
    virtual ~Node();

    /// Returns the name of the file.
    String name() const;

    /**
     * Sets the parent node of this file.
     */
    void setParent(Node *parent);

    /**
     * Returns the parent node. May be NULL.
     */
    Node *parent() const;

    /**
     * Forms the complete path of this node.
     *
     * @return Absolute path of the node.
     */
    String const path() const;

    /**
     * Locates another node starting with a path from this node. The basic logic
     * of interpreting the segments of a path in sequence is implemented here. Also,
     * the special segments "." and ".." are handled by this method.
     *
     * @param path  Relative path to look for. The path is followed starting from
     *              this node. Absolute paths are not supported.
     *
     * @return The located node, or @c NULL if the path was not found.
     */
    virtual Node const *tryFollowPath(PathRef const &path) const;

    /**
     * Gets a child node with a specific name. The default implementation does nothing,
     * because Node does not keep track of children, just the parent. Subclasses should
     * override this if they have children.
     *
     * @param name  Name of a node.
     *
     * @return The child node, or @c NULL if there is no child named @a name.
     */
    virtual Node const *tryGetChild(String const &name) const;

    DENG2_AS_IS_METHODS()

protected:
    /**
     * Constructs a new node.
     *
     * @param name  Name of the node.
     */
    explicit Node(String const &name = "");

private:
    DENG2_PRIVATE(d)
};

} // namespace filesys
} // namespace de

#endif // LIBDENG2_FILESYS_NODE_H
