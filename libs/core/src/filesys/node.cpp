/** @file filesys/node.cpp  File system node.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/filesys/node.h"

namespace de {
namespace filesys {

DE_PIMPL_NOREF(Node)
{
    String name; // lower-cased
    Node *parent;

    Impl(const String &name_)
        : name(name_.lower())
        , parent(nullptr)
    {}
};

Node::Node(const String &name) : d(new Impl(name))
{}

Node::~Node()
{}

String Node::name() const
{
    return d->name;
}

String Node::extension() const
{
    return d->name.fileNameExtension();
}

void Node::setParent(Node *parent)
{
    d->parent = parent;
}

Node *Node::parent() const
{
    return d->parent;
}

bool Node::hasAncestor(const Node &possibleAncestor) const
{
    for (const Node *iter = parent(); iter; iter = iter->parent())
    {
        if (iter == &possibleAncestor) return true;
    }
    return false;
}

String Node::path() const
{
    const Node *p = parent();
    if (!p)
    {
        return "/" + name();
    }
    return p->path() / name();
}

const Node *Node::tryFollowPath(const PathRef &path) const
{
    static const char *DOT_SINGLE = ".";
    static const char *DOT_DOUBLE = "..";

    if (path.isEmpty())
    {
        // As a special case, a null path always refers to self.
        return this;
    }

    // Extract the next component.
    const String &component = path.firstSegment().toLowercaseString();

    // Check if this is the end of the path.
    if (path.segmentCount() == 1 && component != DOT_DOUBLE)
    {
        if (component == DOT_SINGLE)
        {
            // Special case: not a child, but a reference to this node.
            return this;
        }
        return tryGetChild(component);
    }

    const PathRef remainder = path.subPath(Rangei(1, path.segmentCount()));

    // Check for some special cases.
    if (component == DOT_SINGLE)
    {
        return tryFollowPath(remainder);
    }
    if (component == DOT_DOUBLE)
    {
        if (!parent())
        {
            // Can't go there.
            return nullptr;
        }
        return parent()->tryFollowPath(remainder);
    }

    // Continue recursively to the next component.
    if (const Node *child = tryGetChild(component))
    {
        return child->tryFollowPath(remainder);
    }

    // Dead end.
    return nullptr;
}

const Node *Node::tryGetChild(const String &) const
{
    // We have no knowledge of children.
    return nullptr;
}

} // namespace filesys
} // namespace de
