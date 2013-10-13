/** @file interceptor.cpp World map element/object ray trace interceptor.
 *
 * @authors Copyright © 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <de/vector1.h>

#include "de_platform.h"
#include "world/p_intercept.h"
#include "world/p_object.h"

#include "render/r_main.h" // validCount

#include "world/interceptor.h"

using namespace de;

DENG2_PIMPL_NOREF(Interceptor)
{
    traverser_t callback;
    void *context;
    Vector2d from;
    Vector2d to;
    int flags; ///< @ref pathTraverseFlags

    Map *map;
    LineOpening opening;

    // Fixed-point ray representation for use with legacy code.
    fixed_t origin[2];
    fixed_t direction[2];

    Instance(traverser_t callback, Vector2d const &from,
             Vector2d const &to, int flags, void *context)
        : callback(callback),
          context(context),
          from(from),
          to(to),
          flags(flags),
          map(0)
    {
        origin[VX]    = FLT2FIX(from.x);
        origin[VY]    = FLT2FIX(from.y);
        direction[VX] = FLT2FIX(to.x - from.x);
        direction[VY] = FLT2FIX(to.y - from.y);
    }

    void intercept(Line &line)
    {
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
            P_AddIntercept(ICPT_LINE, distance, &line);
        }
    }

    void intercept(mobj_t &mobj)
    {
        // Ignore cameras.
        if(mobj.dPlayer && (mobj.dPlayer->flags & DDPF_CAMERA))
            return;

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
            P_AddIntercept(ICPT_MOBJ, distance, &mobj);
        }
    }

    static int interceptCellLineWorker(Line *line, void *context)
    {
        static_cast<Instance *>(context)->intercept(*line);
        return false; // Continue iteration.
    }

    static int interceptCellMobjWorker(mobj_t *mobj, void *context)
    {
        static_cast<Instance *>(context)->intercept(*mobj);
        return false; // Continue iteration.
    }
};

Interceptor::Interceptor(traverser_t callback, Vector2d const &from,
    Vector2d const &to, int flags, void *context)
    : d(new Instance(callback, from, to, flags, context))
{}

fixed_t const *Interceptor::origin() const
{
    return d->origin;
}

fixed_t const *Interceptor::direction() const
{
    return d->direction;
}

LineOpening const &Interceptor::opening() const
{
    return d->opening;
}

void Interceptor::adjustOpening(Line const &line)
{
    if(!d->map) return;
    if(d->map != &line.map())
    {
        qDebug() << "Ignoring alien line" << de::dintptr(&line) << "in Interceptor::adjustOpening";
        return;
    }

    d->opening = LineOpening(line);
}

int Interceptor::trace(Map const &map)
{
    d->map = const_cast<Map *>(&map);

    // Step #1: Collect intercepts.
    /// @todo Store the intercept list internally.
    P_ClearIntercepts();
    validCount++;

    if(d->flags & PTF_LINE)
    {
        d->map->linePathIterator(d->from, d->to, Instance::interceptCellLineWorker, d);
    }
    if(d->flags & PTF_MOBJ)
    {
        d->map->mobjPathIterator(d->from, d->to, Instance::interceptCellMobjWorker, d);
    }

    // Step #2: Process sorted intercepts.
    return P_TraverseIntercepts(*this, d->callback, d->context);
}
