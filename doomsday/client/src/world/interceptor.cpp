/** @file interceptor.cpp  World map element/object ray trace interceptor.
 *
 * @authors Copyright © 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "world/interceptor.h"

#include <de/memoryzone.h>
#include <de/vector1.h>
#include "world/blockmap.h"
#include "world/p_object.h"
#include "world/worldsystem.h" // validCount

using namespace de;

struct ListNode
{
    ListNode *next;
    ListNode *prev;

    intercepttype_t type;
    void *object;
    float distance;

    template <class ObjectType>
    ObjectType &objectAs() const {
        DENG2_ASSERT(object);
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

DENG2_PIMPL_NOREF(Interceptor)
{
    traverser_t callback;
    void *context;
    Vector2d from;
    Vector2d to;
    int flags; ///< @ref pathTraverseFlags

    Map *map = nullptr;
    LineOpening opening;

    // Array representation for ray geometry (used with legacy code).
    vec2d_t fromV1;
    vec2d_t directionV1;

    Instance(traverser_t callback, Vector2d const &from, Vector2d const &to,
             int flags, void *context)
        : callback(callback)
        , context (context)
        , from    (from)
        , to      (to)
        , flags   (flags)
    {
        V2d_Set(fromV1, from.x, from.y);
        V2d_Set(directionV1, to.x - from.x, to.y - from.y);
    }

    inline bool isSentinel(ListNode const &node)
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
    void addIntercept(intercepttype_t type, float distance, void *object)
    {
        DENG2_ASSERT(object);

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

        fixed_t lineFromX[2] = { DBL2FIX(line.fromOrigin().x), DBL2FIX(line.fromOrigin().y) };
        fixed_t lineToX[2]   = { DBL2FIX(  line.toOrigin().x), DBL2FIX(  line.toOrigin().y) };

        // Is this line crossed?
        // Avoid precision problems with two routines.
        int s1, s2;
        if(direction[VX] >  FRACUNIT * 16 || direction[VY] >  FRACUNIT * 16 ||
           direction[VX] < -FRACUNIT * 16 || direction[VY] < -FRACUNIT * 16)
        {
            s1 = V2x_PointOnLineSide(lineFromX, origin, direction);
            s2 = V2x_PointOnLineSide(lineToX,   origin, direction);
        }
        else
        {
            s1 = line.pointOnSide(Vector2d(FIX2FLT(origin[VX]), FIX2FLT(origin[VY]))) < 0;
            s2 = line.pointOnSide(Vector2d(FIX2FLT(origin[VX] + direction[VX]),
                                           FIX2FLT(origin[VY] + direction[VY]))) < 0;
        }

        // Is this line crossed?
        if(s1 == s2) return;

        // Calculate interception point.
        fixed_t lineDirectionX[2] = { DBL2FIX(line.direction().x), DBL2FIX(line.direction().y) };
        float distance = FIX2FLT(V2x_Intersection(lineFromX, lineDirectionX, origin, direction));

        // On the correct side of the trace origin?
        if(distance >= 0)
        {
            addIntercept(ICPT_LINE, distance, &line);
        }
    }

    void intercept(mobj_t &mobj)
    {
        // Ignore cameras.
        if(mobj.dPlayer && (mobj.dPlayer->flags & DDPF_CAMERA))
            return;

        fixed_t origin[2]    = { DBL2FIX(from.x), DBL2FIX(from.y) };
        fixed_t direction[2] = { DBL2FIX(to.x - from.x), DBL2FIX(to.y - from.y) };

        // Check a corner to corner crossection for hit.
        AABoxd const aaBox = Mobj_AABox(mobj);

        fixed_t icptFrom[2], icptTo[2];
        if((direction[VX] ^ direction[VY]) > 0)
        {
            // \ Slope
            V2x_Set(icptFrom, DBL2FIX(aaBox.minX), DBL2FIX(aaBox.maxY));
            V2x_Set(icptTo,   DBL2FIX(aaBox.maxX), DBL2FIX(aaBox.minY));
        }
        else
        {
            // / Slope
            V2x_Set(icptFrom, DBL2FIX(aaBox.minX), DBL2FIX(aaBox.minY));
            V2x_Set(icptTo,   DBL2FIX(aaBox.maxX), DBL2FIX(aaBox.maxY));
        }

        // Is this line crossed?
        if(V2x_PointOnLineSide(icptFrom, origin, direction) ==
           V2x_PointOnLineSide(icptTo,   origin, direction))
            return;

        // Calculate interception point.
        fixed_t icptDirection[2] = { icptTo[VX] - icptFrom[VX], icptTo[VY] - icptFrom[VY] };
        float distance = FIX2FLT(V2x_Intersection(icptFrom, icptDirection, origin, direction));

        // On the correct side of the trace origin?
        if(distance >= 0)
        {
            addIntercept(ICPT_MOBJ, distance, &mobj);
        }
    }

    static int interceptPathLineWorker(Line *line, void *context)
    {
        static_cast<Instance *>(context)->intercept(*line);
        return false; // Continue iteration.
    }

    void runTrace()
    {
        /// @todo Store the intercept list internally?
        clearIntercepts();

        int const localValidCount = ++validCount;
        if(flags & PTF_LINE)
        {
            map->forAllLinesInPath(from, to, interceptPathLineWorker, this);
        }
        if(flags & PTF_MOBJ)
        {
            map->mobjBlockmap().forAllInPath(from, to, [this, &localValidCount] (void *object)
            {
                mobj_t &mob = *(mobj_t *)object;
                if(mob.validCount != localValidCount) // not yet processed
                {
                    mob.validCount = localValidCount;
                    intercept(mob);
                }
                return LoopContinue;
            });
        }
    }
};

Interceptor::Interceptor(traverser_t callback, Vector2d const &from,
    Vector2d const &to, int flags, void *context)
    : d(new Instance(callback, from, to, flags, context))
{}

coord_t const *Interceptor::origin() const
{
    return d->fromV1;
}

coord_t const *Interceptor::direction() const
{
    return d->directionV1;
}

LineOpening const &Interceptor::opening() const
{
    return d->opening;
}

bool Interceptor::adjustOpening(Line const *line)
{
    DENG2_ASSERT(d->map != 0);
    if(line)
    {
        if(d->map == &line->map())
        {
            d->opening = LineOpening(*line);
        }
        else
        {
            qDebug() << "Ignoring alien line" << de::dintptr(line) << "in Interceptor::adjustOpening";
        }
    }
    return d->opening.range > 0;
}

int Interceptor::trace(Map const &map)
{
    // Step #1: Collect and sort intercepts.
    d->map = const_cast<Map *>(&map);
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
