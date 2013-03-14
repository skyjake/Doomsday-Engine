/** @file p_sight.cpp Map Line of Sight Testing.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath>

#include "de_base.h"
#include "map/bspleaf.h"
#include "map/bspnode.h"
#include "map/hedge.h"
#include "map/linedef.h"
#include "map/polyobj.h"
#include "map/sector.h"
#include "map/gamemap.h"
#include "render/r_main.h" /// For validCount, @todo Remove me.

using namespace de;

/**
 * Models the logic, parameters and state of a line (of) sight (LOS) test.
 *
 * @todo Fixme: The state of a discrete trace is not fully encapsulated here
 *       due to the manipulation of the validCount properties of the various
 *       map data elements. (Which is used to avoid testing the same element
 *       multiple times during a trace.)
 *
 * @todo Optimize: Make use of the blockmap to take advantage of the inherent
 *       spatial locality in this data structure.
 *
 * @ingroup map
 */
class LineSightTest
{
public:
    /**
     * Constructs a new line (of) sight test.
     *
     * @param from          Trace origin point in the map coordinate space.
     * @param to            Trace target point in the map coordinate space.
     * @param bottomSlope   Lower limit to the Z axis angle/slope range.
     * @param topSlope      Upper limit to the Z axis angle/slope range.
     * @param flags         @ref lineSightFlags dictate trace behavior/logic.
     */
    LineSightTest(const_pvec3d_t from, const_pvec3d_t to,
                  float bottomSlope, float topSlope, int flags)
        : _flags(flags),
          _bottomSlope(bottomSlope),
          _topSlope(topSlope)
    {
        V3d_Copy(_from, from);
        V3d_Copy(_to,   to);

        // Configure the ray:
        _ray.origin[VX]    = FLT2FIX(float( _from[VX] ));
        _ray.origin[VY]    = FLT2FIX(float( _from[VY] ));
        _ray.direction[VX] = FLT2FIX(float( _to[VX] - _from[VX] ));
        _ray.direction[VY] = FLT2FIX(float( _to[VY] - _from[VY] ));

        if(_from[VX] > _to[VX])
        {
            _rayAABox.maxX = _from[VX];
            _rayAABox.minX = _to[VX];
        }
        else
        {
            _rayAABox.maxX = _to[VX];
            _rayAABox.minX = _from[VX];
        }

        if(_from[VY] > _to[VY])
        {
            _rayAABox.maxY = _from[VY];
            _rayAABox.minY = _to[VY];
        }
        else
        {
            _rayAABox.maxY = _to[VY];
            _rayAABox.minY = _from[VY];
        }
    }

    /**
     * Execute the trace (i.e., cast the ray).
     *
     * @param bspRoot  Root of BSP to be traced.
     *
     * @return  @c true iff an uninterrupted path exists between the preconfigured
     *          Start and End points of the trace line.
     */
    bool trace(MapElement const &bspRoot)
    {
        validCount++;

        _topSlope    = _to[VZ] + _topSlope    - _from[VZ];
        _bottomSlope = _to[VZ] + _bottomSlope - _from[VZ];

        return crossBspNode(&bspRoot);
    }

private:
    /// Ray origin.
    vec3d_t _from;

    /// Ray target.
    vec3d_t _to;

    /// LS_* flags @ref lineSightFlags
    int _flags;

    /// Slope to bottom of target.
    float _bottomSlope;

    /// Slope to top of target.
    float _topSlope;

    /// The ray to be traced.
    divline_t _ray;
    AABoxd _rayAABox;

    /**
     * @return  @c true if the ray passes @a line; otherwise @c false.
     */
    bool crossLine(LineDef const &line, int side)
    {
#define RTOP                    0x1 ///< Top range.
#define RBOTTOM                 0x2 ///< Bottom range.

        // Does the ray intercept the line on the X/Y plane?
        // Try a quick bounding-box rejection.
        if(line.aaBox().minX > _rayAABox.maxX ||
           line.aaBox().maxX < _rayAABox.minX ||
           line.aaBox().minY > _rayAABox.maxY ||
           line.aaBox().maxY < _rayAABox.minY)
            return true;

        if(Divline_PointOnSide(&_ray, line.v1Origin()) ==
           Divline_PointOnSide(&_ray, line.v2Origin()))
            return true;

        divline_t dl;
        line.configureDivline(dl);

        if(Divline_PointOnSide(&dl, _from) == Divline_PointOnSide(&dl, _to))
            return true;

        // Is this the passable side of a one-way BSP window?
        if(!line.hasSideDef(side))
            return true;

        if(!line.hasSector(side)) /*$degenleaf*/
            return false;

        Sector const *frontSec = line.sectorPtr(side);
        Sector const *backSec  = (line.hasBackSideDef()? line.sectorPtr(side^1) : NULL);

        bool noBack = !line.hasBackSideDef();
        if(!noBack && !(_flags & LS_PASSLEFT))
        {
            noBack = (!( backSec->floor().height() < frontSec->ceiling().height()) ||
                      !(frontSec->floor().height() <  backSec->ceiling().height()));
        }

        if(noBack)
        {
            // Does the ray pass from left to right?
            if(_flags & LS_PASSLEFT) // Allowed.
            {
                if(line.pointOnSide(_from) < 0)
                    return true;
            }

            // No back side is present so if the ray is not allowed to pass over/under the line
            // then end it right here.
            if(!(_flags & (LS_PASSOVER | LS_PASSUNDER)))
                return false;
        }

        // Handle the case of a zero height back side in the top range.
        byte ranges = 0;
        if(noBack)
        {
            ranges |= RTOP;
        }
        else
        {
            if(backSec->floor().height()   != frontSec->floor().height())
                ranges |= RBOTTOM;

            if(backSec->ceiling().height() != frontSec->ceiling().height())
                ranges |= RTOP;
        }

        // No partially closed ranges which require testing?
        if(!ranges)
            return true;

        float frac = FIX2FLT(Divline_Intersection(&dl, &_ray));

        // Does the ray pass over the top range?
        if(_flags & LS_PASSOVER) // Allowed.
        {
            if(_bottomSlope > (frontSec->ceiling().height() - _from[VZ]) / frac)
                return true;
        }

        // Does the ray pass under the bottom range?
        if(_flags & LS_PASSUNDER) // Allowed.
        {
            if(_topSlope < (frontSec->floor().height() - _from[VZ]) / frac)
                return true;
        }

        // Test a partially closed top range?
        if(ranges & RTOP)
        {
            float const top =                                      noBack ? frontSec->ceiling().height() :
                frontSec->ceiling().height() < backSec->ceiling().height()? frontSec->ceiling().height() :
                                                                             backSec->ceiling().height();

            float const slope = (top - _from[VZ]) / frac;

            if((slope < _topSlope) ^ (noBack && !(_flags & LS_PASSOVER)) ||
               (noBack && _topSlope > (frontSec->SP_floorheight - _from[VZ]) / frac))
                _topSlope = slope;

            if((slope < _bottomSlope) ^ (noBack && !(_flags & LS_PASSUNDER)) ||
               (noBack && _bottomSlope > (frontSec->SP_floorheight - _from[VZ]) / frac))
                _bottomSlope = slope;
        }

        // Test a partially closed bottom range?
        if(ranges & RBOTTOM)
        {
            float const bottom =                                noBack? frontSec->floor().height() :
                frontSec->floor().height() > backSec->floor().height()? frontSec->floor().height() :
                                                                         backSec->floor().height();
            float const slope = (bottom - _from[VZ]) / frac;

            if(slope > _bottomSlope)
                _bottomSlope = slope;

            if(slope > _topSlope)
                _topSlope = slope;
        }

        if(_topSlope <= _bottomSlope)
            return false; // Stop iteration.

        return true;

#undef RTOP
#undef RBOTTOM
    }

    /**
     * @return  @c true if the ray passes @a bspLeaf; otherwise @c false.
     */
    bool crossBspLeaf(BspLeaf const &bspLeaf)
    {
        if(Polyobj *po = bspLeaf.firstPolyobj())
        {
            // Check polyobj lines.
            for(LineDef **lineIter = po->lines; *lineIter; lineIter++)
            {
                LineDef &line = **lineIter;
                if(line.validCount() != validCount)
                {
                    line._validCount = validCount;

                    if(!crossLine(line, FRONT))
                        return false; // Stop iteration.
                }
            }
        }

        // Check edges.
        if(HEdge const *base = bspLeaf.firstHEdge())
        {
            HEdge const *hedge = base;
            do
            {
                if(hedge->lineDef && hedge->lineDef->validCount() != validCount)
                {
                    LineDef &line = *hedge->lineDef;
                    line._validCount = validCount;

                    if(!crossLine(line, hedge->side))
                        return false;
                }
            } while((hedge = hedge->next) != base);
        }

        return true; // Continue iteration.
    }

    /**
     * @return  @c true if the ray passes @a bspElement; otherwise @c false.
     */
    bool crossBspNode(MapElement const *bspElement)
    {
        while(bspElement->type() != DMU_BSPLEAF)
        {
            BspNode const &node        = *bspElement->castTo<BspNode>();
            Partition const &partition = node.partition();

            int const fromSide = partition.pointOnSide(_from);
            int const toSide   = partition.pointOnSide(_to);

            // Would the ray completely cross the partition?
            if(fromSide == toSide)
            {
                // Yes, descend!
                bspElement = node.childPtr(fromSide);
            }
            else
            {
                // No.
                if(!crossBspNode(node.childPtr(fromSide)))
                    return 0; // Cross the From side.

                bspElement = node.childPtr(fromSide ^ 1); // Cross the To side.
            }
        }

        BspLeaf const &leaf = *bspElement->castTo<BspLeaf>();
        return crossBspLeaf(leaf);
    }
};

boolean GameMap_CheckLineSight(GameMap *map, const_pvec3d_t from, const_pvec3d_t to,
    coord_t bottomSlope, coord_t topSlope, int flags)
{
    DENG_ASSERT(map && map->bsp);
    return LineSightTest(from, to, float(bottomSlope), float(topSlope), flags).trace(*map->bsp);
}
