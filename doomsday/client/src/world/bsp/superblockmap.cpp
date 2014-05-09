/** @file superblockmap.cpp  BSP Builder Super Blockmap.
 *
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include "world/bsp/superblockmap.h"
#include <de/vector1.h>
#include <de/Log>

using namespace de;
using namespace de::bsp;

DENG2_PIMPL_NOREF(SuperBlockmap::NodeData)
{
    SuperBlockmap &owner;
    AABox bounds;

    Segments segments;    ///< Line segments contained by the node (not owned).
    int mapNum;           ///< Running total of map-line segments at/under this node.
    int partNum;          ///< Running total of partition-line segments at/under this node.

    Instance(SuperBlockmap &owner, AABox const &bounds)
        : owner  (owner)
        , bounds (bounds)
        , mapNum (0)
        , partNum(0)
    {}

    inline void link(LineSegmentSide &seg) {
        segments.prepend(&seg);
    }

    inline void addRef(LineSegmentSide const &seg) {
        if(seg.hasMapSide()) mapNum++;
        else                 partNum++;
    }

    inline void decRef(LineSegmentSide const &seg) {
        if(seg.hasMapSide()) mapNum--;
        else                 partNum--;
    }
};

SuperBlockmap::NodeData::NodeData(SuperBlockmap &owner, AABox const &bounds)
    : _node(0)
    , d(new Instance(owner, bounds))
{}

AABox const &SuperBlockmap::NodeData::bounds() const
{
    return d->bounds;
}

void SuperBlockmap::NodeData::clearSegments()
{
    d->segments.clear();
}

SuperBlockmap::NodeData::Segments SuperBlockmap::NodeData::collateAllSegments()
{
    Segments allSegs;

#ifdef DENG2_QT_4_7_OR_NEWER
    allSegs.reserve(totalSegmentCount());
#endif

    // Iterative pre-order traversal.
    Node *cur = _node;
    Node *prev = 0;
    while(cur)
    {
        while(cur)
        {
            LineSegmentSide *seg;
            while((seg = cur->userData()->pop()))
            {
                allSegs << seg;
            }

            if(prev == cur->parentPtr())
            {
                // Descending - right first, then left.
                prev = cur;
                if(cur->hasRight()) cur = cur->rightPtr();
                else                cur = cur->leftPtr();
            }
            else if(prev == cur->rightPtr())
            {
                // Last moved up the right branch - descend the left.
                prev = cur;
                cur = cur->leftPtr();
            }
            else if(prev == cur->leftPtr())
            {
                // Last moved up the left branch - continue upward.
                prev = cur;
                cur = cur->parentPtr();
            }
        }

        if(prev)
        {
            // No left child - back up.
            cur = prev->parentPtr();
        }
    }

    return allSegs;
}

SuperBlockmap::NodeData::Segments const &SuperBlockmap::NodeData::segments() const
{
    return d->segments;
}

int SuperBlockmap::NodeData::segmentCount(bool addMap, bool addPart) const
{
    int total = 0;
    if(addMap)  total += d->mapNum;
    if(addPart) total += d->partNum;
    return total;
}

SuperBlockmap::Node &SuperBlockmap::NodeData::push(LineSegmentSide &seg)
{
    Node *sb = _node;
    forever
    {
        NodeData &ndata = *sb->userData();
        AABox const &nodeBounds = ndata.bounds();

        // Update line segment counts.
        ndata.d->addRef(seg);

        // Determine whether further subdivision is necessary/possible.
        Vector2i dimensions(Vector2i(nodeBounds.max) - Vector2i(nodeBounds.min));
        if(dimensions.x <= 256 && dimensions.y <= 256)
        {
            ndata.d->link(seg);
            break;
        }

        // On which axis are we splitting?
        bool splitVertical;
        Node::ChildId fromSide, toSide;
        if(nodeBounds.maxX - nodeBounds.minX >= nodeBounds.maxY - nodeBounds.minY)
        {
            splitVertical = false;

            int midX = (nodeBounds.minX + nodeBounds.maxX) / 2;
            fromSide = Node::ChildId(seg.from().origin().x >= midX);
            toSide   = Node::ChildId(seg.to  ().origin().x >= midX);
        }
        else
        {
            splitVertical = true;

            int midY = (nodeBounds.minY + nodeBounds.maxY) / 2;
            fromSide = Node::ChildId(seg.from().origin().y >= midY);
            toSide   = Node::ChildId(seg.to  ().origin().y >= midY);
        }

        if(fromSide != toSide)
        {
            // Line crosses midpoint; link it in and return.
            ndata.d->link(seg);
            break;
        }

        // The segments lies in one half of this block. Create the sub-block
        // if it doesn't already exist, and loop back to add the seg.
        if(!sb->hasChild(fromSide))
        {
            bool const toLeft = (fromSide == Node::Left);

            AABox subBounds;
            if(splitVertical)
            {
                int division = nodeBounds.minY + 0.5 + (nodeBounds.maxY - nodeBounds.minY) / 2;

                subBounds.minX = nodeBounds.minX;
                subBounds.minY = (toLeft? division : nodeBounds.minY);

                subBounds.maxX = nodeBounds.maxX;
                subBounds.maxY = (toLeft? nodeBounds.maxY : division);
            }
            else
            {
                int division = nodeBounds.minX + 0.5 + (nodeBounds.maxX - nodeBounds.minX) / 2;

                subBounds.minX = (toLeft? division : nodeBounds.minX);
                subBounds.minY = nodeBounds.minY;

                subBounds.maxX = (toLeft? nodeBounds.maxX : division);
                subBounds.maxY = nodeBounds.maxY;
            }

            NodeData *subData = new NodeData(ndata.d->owner, subBounds);
            subData->_node = &sb->setChild(fromSide, new Node(subData, sb));
        }

        sb = sb->childPtr(fromSide);
    }

    return *sb;
}

LineSegmentSide *SuperBlockmap::NodeData::pop()
{
    if(!d->segments.isEmpty())
    {
        LineSegmentSide *seg = d->segments.takeFirst();
        d->decRef(*seg);
        return seg;
    }
    return 0;
}

DENG2_PIMPL(SuperBlockmap)
{
    Node rootNode;

    Instance(Public *i, AABox const &bounds) : Base(i)
    {
        // Attach the root Node.
        NodeData *ndata = new NodeData(*thisPublic, bounds);
        rootNode.setUserData(ndata);
        ndata->_node = &rootNode;
    }

    ~Instance() { clear(); }

    static int clearUserDataWorker(Node &subtree, void *)
    {
        delete subtree.userData();
        return 0;
    }

    void clear()
    {
        rootNode.traversePostOrder(clearUserDataWorker);
        rootNode.clear();
    }

    /**
     * Find the axis-aligned bounding box defined by the vertices of all line
     * segments in the block. If empty an AABox initialized to the "cleared"
     * state (i.e., min > max) will be returned.
     *
     * @return  Determined line segment bounds.
     *
     * @todo Optimize: Cache this result.
     */
    void findSegmentBounds(NodeData &data, AABoxd &retBounds, bool *initialized)
    {
        DENG2_ASSERT(initialized);
        if(data.segmentCount(true, true))
        {
            AABoxd bounds;
            bool first = false;

            foreach(LineSegmentSide *seg, data.segments())
            {
                AABoxd segBounds = seg->aaBox();
                if(first)
                {
                    V2d_UniteBox(bounds.arvec2, segBounds.arvec2);
                }
                else
                {
                    V2d_CopyBox(bounds.arvec2, segBounds.arvec2);
                    first = true;
                }
            }

            if(*initialized)
            {
                V2d_AddToBox(retBounds.arvec2, bounds.min);
            }
            else
            {
                V2d_InitBox(retBounds.arvec2, bounds.min);
                *initialized = true;
            }
            V2d_AddToBox(retBounds.arvec2, bounds.max);
        }
    }
};

SuperBlockmap::SuperBlockmap(AABox const &bounds)
    : d(new Instance(this, bounds))
{}

SuperBlockmap::operator SuperBlockmap::Node /*const*/ &()
{
    return d->rootNode;
}

AABoxd SuperBlockmap::findSegmentBounds()
{
    bool initialized = false;
    AABoxd bounds;

    // Iterative pre-order traversal.
    Node *cur = &d->rootNode;
    Node *prev = 0;
    while(cur)
    {
        while(cur)
        {
            d->findSegmentBounds(*cur->userData(), bounds, &initialized);

            if(prev == cur->parentPtr())
            {
                // Descending - right first, then left.
                prev = cur;
                if(cur->hasRight()) cur = cur->rightPtr();
                else                cur = cur->leftPtr();
            }
            else if(prev == cur->rightPtr())
            {
                // Last moved up the right branch - descend the left.
                prev = cur;
                cur = cur->leftPtr();
            }
            else if(prev == cur->leftPtr())
            {
                // Last moved up the left branch - continue upward.
                prev = cur;
                cur = cur->parentPtr();
            }
        }

        if(prev)
        {
            // No left child - back up.
            cur = prev->parentPtr();
        }
    }

    if(!initialized)
    {
        bounds.clear();
    }

    return bounds;
}
