/** @file kdtreeatlasallocator.cpp  KD-tree based atlas allocator.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright (c) 2009-2010 Daniel Swanson <danij@dengine.net>
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

#include "de/kdtreeatlasallocator.h"
#include <de/binarytree.h>
#include <de/list.h>

namespace de {

DE_PIMPL(KdTreeAtlasAllocator)
{
    Atlas::Size size;
    int margin;
    Allocations allocs;

    struct Partition {
        Rectanglei area;
        Id alloc;   ///< Id of the allocation in this partition (or Id::None).
        Partition() : alloc(Id::None) {}
    };
    typedef BinaryTree<Partition> Node;
    Node root;

    Impl(Public *i) : Base(i), margin(0), root(Partition()) {}

    void initTree(Node &rootNode)
    {
        Partition full;
        full.area = Rectanglei(margin, margin, size.x - margin, size.y - margin);
        rootNode.setUserData(full);
    }

    Node *treeInsert(Node *parent, const Atlas::Size &allocSize)
    {
        if (!parent->isLeaf())
        {
            Node *subTree;

            // Try to insert into the right subtree.
            if ((subTree = treeInsert(parent->rightPtr(), allocSize)) != nullptr)
                return subTree;

            // Try to insert into the left subtree.
            return treeInsert(parent->leftPtr(), allocSize);
        }

        /*
         * We have arrived at a leaf.
         */

        Partition pnode = parent->userData();

        // Is this leaf already in use?
        if (!pnode.alloc.isNone())
            return nullptr;

        // Is this leaf big enough to hold the texture?
        if (pnode.area.width() < allocSize.x || pnode.area.height() < allocSize.y)
            return nullptr; // No, too small.

        // Is this leaf the exact size required?
        if (pnode.area.size() == allocSize)
            return parent;

        /*
         * The leaf will be split.
         */

        // Create right and left subtrees.
        Partition rnode, lnode;

        // Are we splitting horizontally or vertically?
        if (pnode.area.width() - allocSize.x > pnode.area.height() - allocSize.y)
        {
            // Horizontal split.
            rnode.area = Rectanglei(pnode.area.left(), pnode.area.top(),
                                    allocSize.x, pnode.area.height());

            lnode.area = Rectanglei(pnode.area.left() + allocSize.x, pnode.area.top(),
                                    pnode.area.width() - allocSize.x, pnode.area.height());
        }
        else
        {
            // Vertical split.
            rnode.area = Rectanglei(pnode.area.left(), pnode.area.top(),
                                    pnode.area.width(), allocSize.y);

            lnode.area = Rectanglei(pnode.area.left(), pnode.area.top() + allocSize.y,
                                    pnode.area.width(), pnode.area.height() - allocSize.y);
        }

        parent->setRight(new Node(rnode, parent));
        parent->setLeft(new Node(lnode, parent));

        // Decend to the right node.
        return treeInsert(parent->rightPtr(), allocSize);
    }

    /**
     * Attempts to find a large enough free space for the requested size.
     * Room is left for margins.
     *
     * @param rootNode  Partitioning root node to use for allocation.
     * @param size      Size to allocate.
     * @param rect      Found rectangle is returned here.
     * @param preallocId  Id for the allocation, if one has already been determined.
     *
     * @return Allocated Id, if successful.
     */
    Id allocate(Node &rootNode, const Atlas::Size &size, Rectanglei &rect, const Id &preallocId = 0)
    {
        // Margin is included only in the bottom/right edges.
        Atlas::Size const allocSize(size.x + margin, size.y + margin);

        if (Node *inserted = treeInsert(&rootNode, allocSize))
        {
            // Got it, give it a new Id.
            Partition part = inserted->userData();
            part.alloc = (preallocId.isNone()? Id() : preallocId);
            inserted->setUserData(part);

            // Remove the margin for the actual allocated rectangle.
            rect = part.area.adjusted(Vec2i(), Vec2i(-margin, -margin));
            return part.alloc;
        }

        return Id::None;
    }

    struct EraseArgs { Id id; };

    static int allocationEraser(Node &node, void *argsPtr)
    {
        EraseArgs *args = reinterpret_cast<EraseArgs *>(argsPtr);

        Partition part = node.userData();
        if (part.alloc == args->id)
        {
            part.alloc = Id::None;
            node.setUserData(part);
            return 1; // Can stop here.
        }
        return 0;
    }

    void releaseAlloc(const Id &id)
    {
        allocs.remove(id);

        EraseArgs args;
        args.id = id;
        root.traverseInOrder(allocationEraser, &args);
    }

    struct ContentSize {
        Id::Type id;
        duint64 area;

        ContentSize(const Id &allocId, const Vec2ui &size)
            : id(allocId), area(size.x * size.y) {}

        // Sort descending.
        bool operator < (const ContentSize &other) const {
            return area > other.area;
        }
    };

    bool optimize()
    {
        // Set up a LUT based on descending allocation width.
        List<ContentSize> descending;
        DE_FOR_EACH(Allocations, i, allocs)
        {
            descending.emplace_back(i->first, i->second.size());
        }
        descending.sort();

        Allocations optimal;
        Node optimalRoot;
        initTree(optimalRoot);

        /*
         * Attempt to optimize space usage by placing the largest allocations
         * first.
         */
        for (const auto &i : descending)
        {
            Rectanglei newRect;
            if (allocate(optimalRoot, allocs[i.id].size(), newRect, i.id).isNone())
            {
                // Could not find a place for this any more.
                return false;
            }

            // This'll do.
            optimal.insert(i.id, newRect);
        }

        // Use the new layout.
        root = optimalRoot;
        allocs = std::move(optimal);
        return true;
    }
};

KdTreeAtlasAllocator::KdTreeAtlasAllocator() : d(new Impl(this))
{}

void KdTreeAtlasAllocator::setMetrics(const Atlas::Size &totalSize, int margin)
{
    DE_ASSERT(d->allocs.isEmpty());

    d->size   = totalSize;
    d->margin = margin;

    d->initTree(d->root);
}

void KdTreeAtlasAllocator::clear()
{
    d->allocs.clear();
    d->root.clear();
}

Id KdTreeAtlasAllocator::allocate(const Atlas::Size &size, Rectanglei &rect,
                                  const Id &knownId)
{
    Id newId = d->allocate(d->root, size, rect, knownId);
    if (newId.isNone())
    {
        // No large enough free space available.
        return 0;
    }

    // Map it for quick access.
    d->allocs[newId] = rect;
    return newId;
}

void KdTreeAtlasAllocator::release(const Id &id)
{
    DE_ASSERT(d->allocs.contains(id));

    d->releaseAlloc(id);
}

int KdTreeAtlasAllocator::count() const
{
    return d->allocs.size();
}

Atlas::Ids KdTreeAtlasAllocator::ids() const
{
    Atlas::Ids ids;
    for (const auto &i : d->allocs)
    {
        ids.insert(i.first);
    }
    return ids;
}

void KdTreeAtlasAllocator::rect(const Id &id, Rectanglei &rect) const
{
    DE_ASSERT(d->allocs.contains(id));
    rect = d->allocs[id];
}

KdTreeAtlasAllocator::Allocations KdTreeAtlasAllocator::allocs() const
{
    return d->allocs;
}

bool KdTreeAtlasAllocator::optimize()
{
    return d->optimize();
}

} // namespace de
