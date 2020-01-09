/** @file interceptor.cpp  World map element/object ray trace interceptor.
 *
 * @authors Copyright © 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#include "doomsday/world/interceptor.h"
#include "doomsday/world/blockmap.h"
#include "doomsday/world/line.h"
#include "doomsday/world/lineblockmap.h"
#include "doomsday/world/lineopening.h"
#include "doomsday/world/mobj.h"
#include "doomsday/world/world.h"

#include <de/legacy/memoryzone.h>
#include <de/legacy/vector1.h>

namespace world {

using namespace de;

struct ListNode
{
    ListNode *next;
    ListNode *prev;

    intercepttype_t type;
    void *object;
    dfloat distance;

    template <class ObjectType>
    ObjectType &objectAs() const {
        DE_ASSERT(object);
        return *static_cast<ObjectType *>(object);
    }
};

// Blockset from which intercepts are allocated.
static zblockset_t *interceptNodeSet;
// Head of the used intercept list.
static ListNode *interceptFirst;

// Trace nodes.
static ListNode head;
static ListNode tail;
static ListNode *mru;

DE_PIMPL_NOREF(Interceptor)
{
    traverser_t callback;
    void *context;
    Vec2d from;
    Vec2d to;
    dint flags; ///< @ref pathTraverseFlags

    world::Map *map = nullptr;
    LineOpening opening;

    // Array representation for ray geometry (used with legacy code).
    vec2d_t fromV1;
    vec2d_t directionV1;

    Impl(traverser_t callback, const Vec2d &from, const Vec2d &to,
             dint flags, void *context)
        : callback(callback)
        , context (context)
        , from    (from)
        , to      (to)
        , flags   (flags)
    {
        V2d_Set(fromV1, from.x, from.y);
        V2d_Set(directionV1, to.x - from.x, to.y - from.y);
    }

    inline bool isSentinel(const ListNode &node)
    {
        return &node == &tail || &node == &head;
    }

    /**
     * Empties the intercepts array and makes sure it has been allocated.
     */
    void clearIntercepts()
    {
#define MININTERCEPTS       128

        if(!interceptNodeSet)
        {
            interceptNodeSet = ZBlockSet_New(sizeof(ListNode), MININTERCEPTS, PU_APPSTATIC);

            // Configure the static head and tail.
            head.distance = 0.0f;
            head.next     = &tail;
            head.prev     = nullptr;

            tail.distance = 1.0f;
            tail.prev     = &head;
            tail.next     = nullptr;
        }

        // Start reusing intercepts (may point to a sentinel but that is Ok).
        if(!interceptFirst)
        {
            interceptFirst = head.next;
        }
        else if(head.next != &tail)
        {
            ListNode *existing = interceptFirst;
            interceptFirst = head.next;
            tail.prev->next = existing;
        }

        // Reset the trace.
        head.next = &tail;
        tail.prev = &head;

        mru = nullptr;

#undef MININTERCEPTS
    }

    /**
     * You must clear intercepts before the first time this is called.
     * The intercepts array grows if necessary.
     *
     * @param type      Type of interception.
     * @param distance  Distance along the trace vector that the interception occured [0...1].
     * @param object    Object being intercepted.
     */
    void addIntercept(intercepttype_t type, dfloat distance, void *object)
    {
        DE_ASSERT(object);

        // First reject vs our sentinels
        if(distance < head.distance) return;
        if(distance > tail.distance) return;

        // Find the new intercept's ordered place along the trace.
        ListNode *before;
        if(mru && mru->distance <= distance)
        {
            before = mru->next;
        }
        else
        {
            before = head.next;
        }

        while(before->next && distance >= before->distance)
        {
            before = before->next;
        }

        ListNode *icpt;
        // Can we reuse an existing intercept?
        if(!isSentinel(*interceptFirst))
        {
            icpt = interceptFirst;
            interceptFirst = icpt->next;
        }
        else
        {
            icpt = (ListNode *) ZBlockSet_Allocate(interceptNodeSet);
        }
        icpt->type     = type;
        icpt->object   = object;
        icpt->distance = distance;

        // Link it in.
        icpt->next = before;
        icpt->prev = before->prev;

        icpt->prev->next = icpt;
        icpt->next->prev = icpt;

        mru = icpt;
    }

    void intercept(Line &line)
    {
        fixed_t origin[2]    = { DBL2FIX(from.x), DBL2FIX(from.y) };
        fixed_t direction[2] = { DBL2FIX(to.x - from.x), DBL2FIX(to.y - from.y) };

        fixed_t lineFromX[2] = { DBL2FIX(line.from().x()), DBL2FIX(line.from().y()) };
        fixed_t lineToX[2]   = { DBL2FIX(line.to  ().x()), DBL2FIX(line.to  ().y()) };

        // Is this line crossed?
        // Avoid precision problems with two routines.
        dint s1, s2;
        if (   direction[0] >  FRACUNIT * 16 || direction[1] >  FRACUNIT * 16
            || direction[0] < -FRACUNIT * 16 || direction[1] < -FRACUNIT * 16)
        {
            s1 = V2x_PointOnLineSide(lineFromX, origin, direction);
            s2 = V2x_PointOnLineSide(lineToX,   origin, direction);
        }
        else
        {
            s1 = line.pointOnSide(Vec2d(FIX2FLT(origin[0]), FIX2FLT(origin[1]))) < 0;
            s2 = line.pointOnSide(Vec2d(FIX2FLT(origin[0] + direction[0]),
                                           FIX2FLT(origin[1] + direction[1]))) < 0;
        }

        // Is this line crossed?
        if (s1 == s2) return;

        // Calculate interception point.
        fixed_t lineDirectionX[2] = { DBL2FIX(line.direction().x), DBL2FIX(line.direction().y) };
        dfloat distance = FIX2FLT(V2x_Intersection(lineFromX, lineDirectionX, origin, direction));

        // On the correct side of the trace origin?
        if (distance >= 0)
        {
            addIntercept(ICPT_LINE, distance, &line);
        }
    }

    void intercept(mobj_t &mob)
    {
        // Ignore cameras.
        if (mob.dPlayer && (mob.dPlayer->flags & DDPF_CAMERA))
            return;

        fixed_t origin[2]    = { DBL2FIX(from.x), DBL2FIX(from.y) };
        fixed_t direction[2] = { DBL2FIX(to.x - from.x), DBL2FIX(to.y - from.y) };

        // Check a corner to corner crossection for hit.
        const AABoxd &box = Mobj_Bounds(mob);

        fixed_t icptFrom[2], icptTo[2];
        if ((direction[0] ^ direction[1]) > 0)
        {
            // \ Slope
            V2x_Set(icptFrom, DBL2FIX(box.minX), DBL2FIX(box.maxY));
            V2x_Set(icptTo,   DBL2FIX(box.maxX), DBL2FIX(box.minY));
        }
        else
        {
            // / Slope
            V2x_Set(icptFrom, DBL2FIX(box.minX), DBL2FIX(box.minY));
            V2x_Set(icptTo,   DBL2FIX(box.maxX), DBL2FIX(box.maxY));
        }

        // Is this line crossed?
        if (   V2x_PointOnLineSide(icptFrom, origin, direction)
            == V2x_PointOnLineSide(icptTo,   origin, direction))
            return;

        // Calculate interception point.
        fixed_t icptDirection[2] = { icptTo[0] - icptFrom[0], icptTo[1] - icptFrom[1] };
        dfloat distance = FIX2FLT(V2x_Intersection(icptFrom, icptDirection, origin, direction));

        // On the correct side of the trace origin?
        if (distance >= 0)
        {
            addIntercept(ICPT_MOBJ, distance, &mob);
        }
    }

    void runTrace()
    {
        clearIntercepts();
        const dint localValidCount = ++World::validCount;

        if(flags & PTF_LINE)
        {
            // Process polyobj lines.
            if(map->polyobjCount())
            {
                map->polyobjBlockmap().forAllInPath(from, to, [this, &localValidCount] (void *object)
                {
                    auto &pob = *(Polyobj *)object;
                    if(pob.validCount != localValidCount)  // not yet processed
                    {
                        pob.validCount = localValidCount;
                        for(Line *line : pob.lines())
                        {
                            if(line->validCount() != localValidCount)  // not yet processed
                            {
                                line->setValidCount(localValidCount);
                                intercept(*line);
                            }
                        }
                    }
                    return LoopContinue;
                });
            }

            // Process sector lines.
            map->lineBlockmap().forAllInPath(from, to, [this, &localValidCount] (void *object)
            {
                auto &line = *(Line *)object;
                if(line.validCount() != localValidCount)  // not yet processed
                {
                    line.setValidCount(localValidCount);
                    intercept(line);
                }
                return LoopContinue;
            });
        }

        if(flags & PTF_MOBJ)
        {
            // Process map objects.
            map->mobjBlockmap().forAllInPath(from, to, [this, &localValidCount] (void *object)
            {
                auto &mob = *(mobj_t *)object;
                if(mob.validCount != localValidCount)  // not yet processed
                {
                    mob.validCount = localValidCount;
                    intercept(mob);
                }
                return LoopContinue;
            });
        }
    }
};

Interceptor::Interceptor(traverser_t  callback,
                         const Vec2d &from,
                         const Vec2d &to,
                         dint         flags,
                         void *       context)
    : d(new Impl(callback, from, to, flags, context))
{}

const ddouble *Interceptor::origin() const
{
    return d->fromV1;
}

const ddouble *Interceptor::direction() const
{
    return d->directionV1;
}

const LineOpening &Interceptor::opening() const
{
    return d->opening;
}

bool Interceptor::adjustOpening(const Line *line)
{
    DE_ASSERT(d->map != 0);
    if (line)
    {
        if (d->map == &line->map())
        {
            d->opening = LineOpening(*line);
        }
        else
        {
            debug("Ignoring alien line %i in Interceptor::adjustOpening", line);
        }
    }
    return d->opening.range > 0;
}

dint Interceptor::trace(const world::Map &map)
{
    // Step #1: Collect and sort intercepts.
    d->map = const_cast<world::Map *>(&map);
    d->runTrace();

    // Step #2: Process intercepts.
    for(ListNode *node = head.next; !d->isSentinel(*node); node = node->next)
    {
        // Prepare the intercept info.
        Intercept icpt;
        icpt.trace    = this;
        icpt.distance = node->distance;
        icpt.type     = node->type;
        switch(node->type)
        {
        case ICPT_MOBJ: icpt.mobj = &node->objectAs<mobj_t>(); break;
        case ICPT_LINE: icpt.line = &node->objectAs<Line>();   break;
        }

        // Make the callback.
        if(int result = d->callback(&icpt, d->context))
            return result;
    }

    return false; // Intercept traversal completed wholly.
}

} // namespace world
