/** @file angleclipper.cpp  Angle Clipper.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "render/angleclipper.h"

#include <de/list.h>
#include <de/error.h>
#include <de/log.h>
#include <doomsday/console/var.h>
#include <doomsday/mesh/hedge.h>
#include <doomsday/tab_tables.h>

#include "dd_def.h"
#include "render/rend_main.h"

using namespace de;

namespace internal
{
    /**
     * @param point  @em View-relative point in map-space.
     */
    static inline binangle_t pointToAngle(const Vec2d &point)
    {
        // Shift for more accuracy;
        return bamsAtan2(dint(point.y * 100), dint(point.x * 100));
    }

    /**
     * Simple data structure for pooling POD elements. Note that unlike a traditional
     * ObjectPool (pattern), the pooled elements are @em not owned by the pool!
     */
    class ElementPool
    {
    public:
        /**
         * Base for POD elements.
         */
        struct Element
        {
        private:
            Element *_prev, *_next;

            friend class ElementPool;
        };

        /**
         * Begin reusing elements in the pool.
         */
        void rewind() {
            _rover = _first;
        }

        /**
         * Add a new @em unused object to the pool.
         *
         * @param elem  Element to be linked in the pool. Ownership is unaffected.
         */
        void add(Element *elem)
        {
            // Link it to the start of the rover's list.
            if(!_last) _last = elem;
            if(_first) _first->_prev = elem;

            elem->_next = _first;
            elem->_prev = nullptr;

            _first = elem;
        }

        /**
         * Returns a pointer to the next unused element in the pool; otherwise @c nullptr.
         */
        void *get() {
            if(!_rover) return nullptr;

            // We'll use this.
            Element *next = _rover;
            _rover = _rover->_next;
            return next;
        }

        /**
         * Release the element @a elem (@important which is assumed to have been added
         * previously!), moving it to the list of used elements, for later reuse.
         */
        void release(Element *elem)
        {
            DE_ASSERT(_last);

            if(elem == _last)
            {
                DE_ASSERT(!_rover);

                // We can only remove the last if all elements are already in use.
                _rover = elem;
                return;
            }

            DE_ASSERT(elem->_next);

            // Unlink from the list entirely.
            elem->_next->_prev = elem->_prev;
            if(elem->_prev)
            {
                elem->_prev->_next = elem->_next;
            }
            else
            {
                _first = _first->_next;
                _first->_prev = nullptr;
            }

            // Put it back to the end of the list.
            _last->_next = elem;
            elem->_prev = _last;
            elem->_next = nullptr;
            _last = elem;

            // If all were in use, set the rover here. Otherwise the rover can stay
            // where it is.
            if(!_rover)
            {
                _rover = _last;
            }
        }

    private:
        Element *_first = nullptr;
        Element *_last  = nullptr;
        Element *_rover = nullptr;
    };
}  // namespace internal
using namespace ::internal;

DE_PIMPL_NOREF(AngleClipper)
{
    /// Specialized AngleRange for half-space clipping.
    struct Clipper : public ElementPool::Element, AngleRange
    {
        Clipper *prev;
        Clipper *next;
    };
    ElementPool clipNodes;          ///< The list of clipnodes.
    Clipper *   clipHead = nullptr; ///< Head of the clipped-range list.

    /// Specialized AngleRange for half-space occlusion.
    struct Occluder : public ElementPool::Element, AngleRange
    {
        Occluder *prev;
        Occluder *next;

        bool  topHalf; ///< @c true= top, rather than bottom, half.
        Vec3f normal;  ///< Of the occlusion plane.
    };
    ElementPool occNodes;          ///< The list of occlusion nodes.
    Occluder *  occHead = nullptr; ///< Head of the occlusion-range list.

    List<binangle_t> angleBuf;  ///< Scratch buffer for sorting angles.

    ~Impl()
    {
        clearRangeList(&clipHead);
        clearRangeList(&occHead);
    }

    template <typename NodeType>
    void clearRangeList(NodeType **head)
    {
        DE_ASSERT(head);
        while(*head)
        {
            auto *next = static_cast<NodeType *>((*head)->next);
            delete *head;
            *head = next;
        }
    }

    /**
     * The specified range must be safe!
     */
    dint isRangeVisible(binangle_t from, binangle_t to) const
    {
        for(Clipper *i = clipHead; i; i = i->next)
        {
            if(from >= i->from && to <= i->to)
                return false;
        }
        // No clip-node fully contained the specified range.
        return true;
    }

    /**
     * @return  Non-zero iff the range is not entirely clipped; otherwise @c 0.
     */
    dint safeCheckRange(binangle_t from, binangle_t to) const
    {
        if(from > to)
        {
            // The range wraps around.
            return (isRangeVisible(from, BANG_MAX) || isRangeVisible(0, to));
        }
        return isRangeVisible(from, to);
    }

    void removeRange(Clipper *crange)
    {
        // If this is the head, move it.
        if(clipHead == crange)
            clipHead = crange->next;

        if(crange->prev)
            crange->prev->next = crange->next;
        if(crange->next)
            crange->next->prev = crange->prev;

        // We're done with this range - mark it as free for reuse.
        clipNodes.release(crange);
    }

    Clipper *newClipNode(binangle_t from, binangle_t to)
    {
        // Perhaps a previously-used clip-node can be reused?
        auto *crange = reinterpret_cast<Clipper *>(clipNodes.get());
        if(!crange)
        {
            // No, allocate another.
            clipNodes.add(crange = new Clipper);
        }

        // (Re)Configure.
        crange->from = from;
        crange->to   = to;
        crange->prev = nullptr;
        crange->next = nullptr;

        return crange;
    }

    void addRange(binangle_t from, binangle_t to)
    {
        // This range becomes a solid segment: cut everything away from the
        // corresponding occlusion range.
        cutOcclusionRange(from, to);

        // If there is no head, this will be the first range.
        if(!clipHead)
        {
            clipHead = newClipNode(from, to);

            /*
            LOG_AS("AngleClipper::addRange");
            LOG_DEBUG(String("New head added: %1 => %2")
                        .arg(clipHead->from, 0, 16)
                        .arg(clipHead->to,   0, 16));
            */
            return;
        }

        // There are previous ranges. Check that the new range isn't contained
        // by any of them.
        for(Clipper *i = clipHead; i; i = i->next)
        {
            /*
            LOG_AS("AngleClipper::addRange");
            LOG_DEBUG(String("0x%1: %2 => %3")
                        .arg((quintptr)i, QT_POINTER_SIZE * 2, 16, QChar('0'))
                        .arg(i->from, 0, 16)
                        .arg(i->to,   0, 16));
            */

            if(from >= i->from && to <= i->to)
            {
                /*
                LOG_AS("AngleClipper::addRange");
                LOG_DEBUG("Range already exists");
                */
                return;  // The new range already exists.
            }

#ifdef DE_DEBUG
            if (i == i->next)
                throw Error("AngleClipper::addRange",
                            stringf("loop1 %p linked to itself: %x => %x", i, i->from, i->to));
#endif
        }

        // Now check if any of the old ranges are contained by the new one.
        for(Clipper *i = clipHead; i;)
        {
            if(i->from >= from && i->to <= to)
            {
                Clipper *contained = i;

                /*
                LOG_AS("AngleClipper::addRange");
                LOG_DEBUG(String("Removing contained range %1 => %2")
                            .arg(contained->from, 0, 16)
                            .arg(contained->to,   0, 16));
                */

                i = i->next;
                removeRange(contained);
                continue;
            }

            i = i->next;
        }

        // Now it is possible that the new range overlaps one or two old ranges.
        // If two are overlapped, they are consecutive. First we'll try to find
        // a range that overlaps the beginning.
        Clipper *crange = nullptr;
        for(Clipper *i = clipHead; i; i = i->next)
        {
            // In preparation for the next stage, find a good spot for the range.
            if(i->from < to)
            {
                // After this one.
                crange = i;
            }

            if(i->from >= from && i->from <= to)
            {
                // New range's end and i's beginning overlap. i's end is outside.
                // Otherwise it would have been already removed.
                // It suffices to adjust i.

                /*
                LOG_AS("AngleClipper::addRange");
                LOG_DEBUG(String("Overlapping start: %1 => %2 - adjusting to %3 => %4")
                            .arg(i->from, 0, 16)
                            .arg(i->to,   0, 16)
                            .arg(from,    0, 16)
                            .arg(i->to,   0, 16));
                */

                i->from = from;
                return;
            }

            // Check an overlapping end.
            if(i->to >= from && i->to <= to)
            {
                // Now it's possible that the i->next's beginning overlaps the
                // new range's end. In that case there will be a merger.

                /*
                LOG_AS("AngleClipper::addRange");
                LOG_DEBUG(String("Overlapping end: %1 => %2")
                            .arg(i->from, 0, 16)
                            .arg(i->to,   0, 16));
                */

                crange = i->next;
                if(!crange)
                {
                    i->to = to;

                    /*
                    LOG_AS("AngleClipper::addRange");
                    LOG_DEBUG(String("No next, adjusting end (now %1 => %2)")
                                .arg(i->from, 0, 16)
                                .arg(i->to,   0, 16));
                    */
                }
                else
                {
                    if(crange->from <= to)
                    {
                        // A fusion will commence. Ci will eat the new range
                        // *and* crange.
                        i->to = crange->to;

                        /*
                        LOG_AS("AngleClipper::addRange");
                        LOG_DEBUG(String("merging with the next (%1 => %2)")
                                    .arg(crange->from, 0, 16)
                                    .arg(crange->to,   0, 16));
                        */

                        removeRange(crange);
                    }
                    else
                    {
                        // Not overlapping.
                        i->to = to;

                        /*
                        LOG_AS("AngleClipper::addRange");
                        LOG_DEBUG(String("Not merger w/next (now %1 => %2)")
                                    .arg(i->from, 0, 16)
                                    .arg(i->to,   0, 16));
                        */
                    }
                }

                return;
            }
        }

        // Still here? Now we know for sure that the range is disconnected from
        // the others. We still need to find a good place for it. Crange will
        // mark the spot.

        if(!crange)
        {
            // We have a new head.
            crange = clipHead;
            clipHead = newClipNode(from, to);
            clipHead->next = crange;
            if(crange)
                crange->prev = clipHead;
        }
        else
        {
            // Add the new range after crange.
            Clipper *added = newClipNode(from, to);
            added->next = crange->next;
            if(added->next)
                added->next->prev = added;
            added->prev = crange;
            crange->next = added;
        }
    }

    void removeOcclusionRange(Occluder *orange)
    {
        // If this is the head, move it.
        if(occHead == orange)
            occHead = orange->next;

        if(orange->prev)
            orange->prev->next = orange->next;
        if(orange->next)
            orange->next->prev = orange->prev;

        // We're done with this range - mark it as free for reuse.
        occNodes.release(orange);
    }

    Occluder *newOcclusionRange(binangle_t from, binangle_t to, const Vec3f &normal,
        bool topHalf)
    {
        // Perhaps a previously-used occluder can be reused?
        auto *orange = reinterpret_cast<Occluder *>(occNodes.get());
        if(!orange)
        {
            // No, allocate another.
            occNodes.add(orange = new Occluder);
        }

        // (Re)Configure.
        orange->from    = from;
        orange->to      = to;
        orange->prev    = nullptr;
        orange->next    = nullptr;
        orange->topHalf = topHalf;
        orange->normal  = normal;

        return orange;
    }

    /**
     * @pre The given range is "safe".
     */
    void addOcclusionRange(binangle_t from, binangle_t to, const Vec3f &normal,
        bool topHalf)
    {
        // Is the range valid?
        if(from > to) return;

        // A new range will be added.
        Occluder *newor = newOcclusionRange(from, to, normal, topHalf);

        // Are there any previous occlusion nodes?
        if(!occHead)
        {
            // No; this is the first.
            occHead = newor;
            occHead->next = occHead->prev = nullptr;
            return;
        }

        /// @todo Optimize: Remove existing oranges that are fully contained by
        /// the new orange. But how to do the check efficiently?

        // Add the new occlusion range to the appropriate position.
        Occluder *after = nullptr;
        for(Occluder *orange = occHead; orange; orange = orange->next)
        {
            // The list of oranges is sorted by the start angle.
            // Find the first range whose start is greater than the new one.
            if(orange->from > from)
            {
                // Add before this one.
                newor->next = orange;
                newor->prev = orange->prev;
                orange->prev = newor;

                if(newor->prev)
                    newor->prev->next = newor;
                else
                    occHead = newor;  // We have a new head.

                return;
            }

            after = orange;
        }

        // Add the new range to the end of the list.
        after->next = newor;
        newor->prev = after;
        newor->next = nullptr;
    }

    /**
     * If necessary, cut the given range in two.
     */
    void safeAddOcclusionRange(binangle_t startAngle, binangle_t endAngle,
                               const Vec3f &normal, bool tophalf)
    {
        // Is this range already clipped?
        if(!safeCheckRange(startAngle, endAngle)) return;

        if(startAngle > endAngle)
        {
            // The range has to be added in two parts.
            addOcclusionRange(startAngle, BANG_MAX, normal, tophalf);

            DE_DEBUG_ONLY(occlusionRanger(3));

            addOcclusionRange(0, endAngle, normal, tophalf);

            DE_DEBUG_ONLY(occlusionRanger(4));
        }
        else
        {
            // Add the range as usual.
            addOcclusionRange(startAngle, endAngle, normal, tophalf);

            DE_DEBUG_ONLY(occlusionRanger(5));
        }
    }

    /**
     * Attempts to merge the two given occnodes.
     *
     * @return @c 0= Could not be merged.
     *         @c 1= orange was merged into other.
     *         @c 2= other was merged into orange.
     */
    dint tryMergeOccludes(Occluder *orange, Occluder *other)
    {
        // We can't test this steep planes.
        if(!orange->normal.z) return 0;

        // Where do they cross?
        Vec3f cross = orange->normal.cross(other->normal);
        if(!cross.x && !cross.y && !cross.z)
        {
            // These two planes are exactly the same! Remove one.
            removeOcclusionRange(orange);
            return 1;
        }

        // The cross angle must be outside the range.
        binangle_t crossAngle = bamsAtan2(dint(cross.y), dint(cross.x));
        if(crossAngle >= orange->from && crossAngle <= orange->to)
            return 0;  // Inside the range, can't do a thing.

        /// @todo Is it not possible to consistently determine the direction at
        /// which cross (vector) is pointing?
        crossAngle += BANG_180;
        if(crossAngle >= orange->from && crossAngle <= orange->to)
            return 0;  // Inside the range, can't do a thing.

        // Now we must determine which plane occludes which.
        // Pick a point in the middle of the range.
        crossAngle = (orange->from + orange->to) >> (1 + BAMS_BITS - 13);
        cross.x = 100 * FIX2FLT(finecosine[crossAngle]);
        cross.y = 100 * FIX2FLT(finesine  [crossAngle]);
        cross.z = -(orange->normal.x * cross.x +
                    orange->normal.y * cross.y) / orange->normal.z;

        // Is orange occluded by the other one?
        if(cross.dot(other->normal) < 0)
        {
            // No; then the other one is occluded by us. Remove it instead.
            removeOcclusionRange(other);
            return 2;
        }
        else
        {
            removeOcclusionRange(orange);
            return 1;
        }
    }

    /**
     * Try to merge oranges with matching ranges. (Quite a number may be produced
     * as a result of the cuts.)
     */
    void mergeOccludes()
    {
        for(Occluder *orange = occHead; orange && orange->next; )
        {
            // As orange might be removed - remember the next one.
            auto *next = orange->next;

            // Find a good one to test with.
            for(Occluder *other = next; other && orange->from == other->from; other = other->next)
            {
                if(other->topHalf != orange->topHalf) continue;
                if(orange->to != other->to) continue;

                // It is a candidate for merging.
                dint result = tryMergeOccludes(orange, other);
                if(result == 2)
                {
                    next = next->next;
                }
                break;
            }

            orange = next;
        }
    }

    /**
     * Everything in the given range is removed from the occlusion nodes.
     */
    void cutOcclusionRange(binangle_t from, binangle_t to)
    {
        DE_DEBUG_ONLY(occlusionRanger(1));

        // Find the range after which it's OK to add oranges cut in half.
        // (Must preserve the ascending order of the start angles.) We want the
        // orange with the smallest start angle, but one that starts after the
        // cut range has ended.
        Occluder *after  = nullptr;
        for(Occluder *orange = occHead; orange && orange->from < to; orange = orange->next)
        {
            after = orange;
        }

        for(Occluder *orange = occHead; orange; )
        {
            // As orange might be removed - remember the next one.
            auto *next = orange->next;

            // Does the cut range include this orange?
            if(from <= orange->to)
            {
                // No more cuts possible?
                if(orange->from >= to) break;

                // Four options:
                switch(orange->relationship(AngleRange(from, to)))
                {
                case 0:  // The cut range completely includes this orange.

                    // Fully contained; this orange will be removed.
                    removeOcclusionRange(orange);
                    break;

                case 1:  // The cut range contains the beginning of the orange.

                    // Cut away the beginning of this orange.
                    orange->from = to;
                    // Even though the start angle is modified, we don't need to
                    // move this orange anywhere. This is because after the cut
                    // there will be no oranges beginning inside the cut range.
                    break;

                case 2:  // The cut range contains the end of the orange.

                    // Cut away the end of this orange.
                    orange->to = from;
                    break;

                case 3: {  // The orange contains the whole cut range.

                    // The orange gets cut in two parts. Create a new orange that
                    // represents the end, and add it after the 'after' range, or
                    // to the head of the list.
                    Occluder *part = newOcclusionRange(to, orange->to, orange->normal,
                                                       orange->topHalf);

                    part->prev = after;
                    if(after)
                    {
                        part->next = after->next;
                        after->next = part;
                    }
                    else
                    {
                        // Add to the head.
                        part->next = occHead;
                        occHead = part;
                    }

                    if(part->next)
                        part->next->prev = part;

                    // Modify the start part.
                    orange->to = from;
                    break; }

                default:  // No meaningful relationship (in this context).
                    break;
                }
            }

            orange = next;
        }

        DE_DEBUG_ONLY(occlusionRanger(2));

        mergeOccludes();

        DE_DEBUG_ONLY(occlusionRanger(6));
    }

#ifdef DE_DEBUG
    void occlusionLister()
    {
        for (Occluder *orange = occHead; orange; orange = orange->next)
        {
            LOG_MSG("from: %x to: %x topHalf: %b") << orange->from << orange->to << orange->topHalf;
        }
    }

    void occlusionRanger(int mark)
    {
        for (Occluder *orange = occHead; orange; orange = orange->next)
        {
            if (orange->prev && orange->prev->from > orange->from)
            {
                occlusionLister();
                throw Error("AngleClipper::occlusionRanger", stringf("Order %i has failed", mark));
            }
        }
    }
#endif
};

AngleClipper::AngleClipper() : d(new Impl)
{}

dint AngleClipper::isFull() const
{
    if(::devNoCulling) return false;

    return d->clipHead && d->clipHead->from == 0 && d->clipHead->to == BANG_MAX;
}

dint AngleClipper::isAngleVisible(binangle_t bang) const
{
    if(::devNoCulling) return true;

    for(const Impl::Clipper *crange = d->clipHead; crange; crange = crange->next)
    {
        if(bang > crange->from && bang < crange->to)
            return false;
    }

    return true;  // Not occluded.
}

dint AngleClipper::isPointVisible(const Vec3d &point) const
{
    if(::devNoCulling) return true;

    const Vec3d viewRelPoint = point - Rend_EyeOrigin().xzy();
    const binangle_t angle      = pointToAngle(viewRelPoint);

    if(!isAngleVisible(angle)) return false;

    // Not clipped by the clipnodes. Perhaps it's occluded by an orange.
    for(const Impl::Occluder *orange = d->occHead; orange; orange = orange->next)
    {
        if(angle >= orange->from && angle <= orange->to)
        {
            if(orange->from > angle)
                return true;  // No more possibilities.

            // On which side of the occlusion plane is it?
            // The positive side is the occluded one.
            if(viewRelPoint.dot(orange->normal) > 0)
                return false;
        }
    }

    return true;  // Not occluded.
}

dint AngleClipper::isPolyVisible(const mesh::Face &poly) const
{
    DE_ASSERT(poly.isConvex());

    if(::devNoCulling) return true;

    // Do we need to resize the angle list buffer?
    if(poly.hedgeCount() > d->angleBuf.count())
    {
        d->angleBuf.resize(poly.hedgeCount());
    }

    // Find angles to all corners.
    const Vec2d eyeOrigin = Rend_EyeOrigin().xz();
    dint n = 0;
    const auto *hedge = poly.hedge();
    do
    {
        d->angleBuf[n++] = pointToAngle(hedge->origin() - eyeOrigin);

    } while((hedge = &hedge->next()) != poly.hedge());

    // Check each of the ranges defined by the edges. The last edge won't be checked.
    // This is because the edges define a closed, convex polygon and the last edge's
    // range is always already covered by the previous edges.
    for(dint i = 0; i < poly.hedgeCount() - 1; ++i)
    {
        // If even one of the edges is not contained by a clipnode, the leaf is at
        // least partially visible.
        binangle_t angLen = d->angleBuf.at(i + 1) - d->angleBuf.at(i);

        // The viewer is on an edge, the leaf should be visible.
        if(angLen == BANG_180) return true;

        // Choose the start and end points so that length is < 180.
        if(angLen < BANG_180)
        {
            if(d->safeCheckRange(d->angleBuf.at(i), d->angleBuf.at(i + 1)))
                return true;
        }
        else
        {
            if(d->safeCheckRange(d->angleBuf.at(i + 1), d->angleBuf.at(i)))
                return true;
        }
    }

    return false;  // Completely occluded.
}

void AngleClipper::clearRanges()
{
    d->clipHead = nullptr;
    d->clipNodes.rewind();  // Start reusing ranges.

    d->occHead = nullptr;
    d->occNodes.rewind();   // Start reusing ranges.
}

dint AngleClipper::safeAddRange(binangle_t from, binangle_t to)
{
    // The range may wrap around.
    if(from > to)
    {
        // The range has to added in two parts.
        d->addRange(from, BANG_MAX);
        d->addRange(0, to);
    }
    else
    {
        // Add the range as usual.
        d->addRange(from, to);
    }
    return true;
}

void AngleClipper::addRangeFromViewRelPoints(const Vec2d &from, const Vec2d &to)
{
    const Vec2d eyeOrigin = Rend_EyeOrigin().xz();
    safeAddRange(pointToAngle(to   - eyeOrigin),
                 pointToAngle(from - eyeOrigin));
}

/// @todo Optimize:: Check if the given line is already occluded?
void AngleClipper::addViewRelOcclusion(const Vec2d &from, const Vec2d &to,
    coord_t height, bool topHalf)
{
    // Calculate the occlusion plane normal.
    // We'll use the game's coordinate system (left-handed, but Y and Z are swapped).
    const Vec3d eyeOrigin    = Rend_EyeOrigin().xzy();
    const auto eyeToV1          = Vec3d(from, height) - eyeOrigin;
    const auto eyeToV2          = Vec3d(to,   height) - eyeOrigin;

    const binangle_t startAngle = pointToAngle(eyeToV2);
    const binangle_t endAngle   = pointToAngle(eyeToV1);

    // Do not attempt to occlude with a zero-length range.
    if(startAngle == endAngle) return;

    // The normal points to the half we want to occlude.
    const Vec3f normal = (topHalf? eyeToV2 : eyeToV1).cross(topHalf? eyeToV1 : eyeToV2);

#ifdef DE_DEBUG
    if(Vec3f(0, 0, (topHalf ? 1000 : -1000)).dot(normal) < 0)
    {
        LOG_AS("AngleClipper::addViewRelOcclusion");
        LOGDEV_GL_WARNING("Wrong side v1:%s v2:%s eyeOrigin:%s!")
                << from.asText() << to.asText()
                << Vec2d(eyeOrigin).asText();
        DE_ASSERT_FAIL("Failed AngleClipper::addViewRelOcclusion: Side test");
    }
#endif

    // Try to add this range.
    d->safeAddOcclusionRange(startAngle, endAngle, normal, topHalf);
}

dint AngleClipper::checkRangeFromViewRelPoints(const Vec2d &from, const Vec2d &to)
{
    if(::devNoCulling) return true;

    const Vec2d eyeOrigin = Rend_EyeOrigin().xz();
    return d->safeCheckRange(pointToAngle(to   - eyeOrigin) - BANG_45/90,
                             pointToAngle(from - eyeOrigin) + BANG_45/90);
}

#ifdef DE_DEBUG
void AngleClipper::validate()
{
    for(Impl::Clipper *i = d->clipHead; i; i = i->next)
    {
        if(i == d->clipHead)
        {
            if(i->prev)
                throw Error("AngleClipper::validate", "Cliphead->prev != NULL");
        }

        // Confirm that the links to prev and next are OK.
        if(i->prev)
        {
            if(i->prev->next != i)
                throw Error("AngleClipper::validate", "Prev->next != this");
        }
        else if(i != d->clipHead)
        {
            throw Error("AngleClipper::validate", "prev == NULL, this isn't clipHead");
        }

        if(i->next)
        {
            if(i->next->prev != i)
                throw Error("AngleClipper::validate", "Next->prev != this");
        }
    }
}
#endif
