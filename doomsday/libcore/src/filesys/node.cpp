/** @file filesys/node.cpp  File system node.
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

#include "de/filesys/Node"
#include "de/Guard"

namespace de {
namespace filesys {

DENG2_PIMPL_NOREF(Node)
{
    String name;
    Node *parent;

    Instance(String const &name_) : name(name_), parent(0) {}
};

Node::Node(String const &name) : d(new Instance(name))
{}

Node::~Node()
{}

String Node::name() const
{
    DENG2_GUARD(this);

    return d->name;
}

void Node::setParent(Node *parent)
{
    DENG2_GUARD(this);

    d->parent = parent;
}

Node *Node::parent() const
{
    DENG2_GUARD(this);

    return d->parent;
}

String const Node::path() const
{
    DENG2_GUARD(this);

    String thePath = name();
    for(Node *i = d->parent; i; i = i->d->parent)
    {
        thePath = i->name() / thePath;
    }
    return "/" + thePath;
}

Node const *Node::tryFollowPath(PathRef const &path) const
{
    if(path.isEmpty())
    {
        // As a special case, a null path always refers to self.
        return this;
    }

    DENG2_GUARD(this);

    // Extract the next component.
    Path::Segment const &component = path.firstSegment();
    if(path.segmentCount() == 1)
    {
        // This is the final component.
        return tryGetChild(component);
    }

    PathRef const remainder = path.subPath(Rangei(1, path.segmentCount()));

    static QString const singleDot(".");
    static QString const doubleDot("..");

    // Check for some special cases.
    if(component == singleDot)
    {
        return tryFollowPath(remainder);
    }
    if(component == doubleDot)
    {
        if(!parent())
        {
            // Can't go there.
            return 0;
        }
        return parent()->tryFollowPath(remainder);
    }

    // Continue recursively to the next component.
    if(Node const *child = tryGetChild(component))
    {
        return child->tryFollowPath(remainder);
    }

    // Dead end.
    return 0;
}

Node const *Node::tryGetChild(String const &) const
{
    // We have no knowledge of children.
    return 0;
}

} // namespace filesys
} // namespace de
