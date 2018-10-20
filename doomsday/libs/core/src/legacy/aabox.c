/** @file aabox.c  Axis-aligned bounding box.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/legacy/aabox.h"
#include "de/legacy/vector1.h"
#include "de/legacy/mathutil.h"

int M_BoxOnLineSide(const AABoxd* box, double const linePoint[], double const lineDirection[])
{
    int a, b;

    switch(M_SlopeType(lineDirection))
    {
    default: // Shut up compiler.
    case ST_HORIZONTAL:
        a = box->maxY > linePoint[VY]? -1 : 1;
        b = box->minY > linePoint[VY]? -1 : 1;
        if(lineDirection[VX] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_VERTICAL:
        a = box->maxX < linePoint[VX]? -1 : 1;
        b = box->minX < linePoint[VX]? -1 : 1;
        if(lineDirection[VY] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_POSITIVE: {
        double topLeft[2]     = { box->minX, box->maxY };
        double bottomRight[2] = { box->maxX, box->minY };
        a = V2d_PointOnLineSide(topLeft,     linePoint, lineDirection) < 0 ? -1 : 1;
        b = V2d_PointOnLineSide(bottomRight, linePoint, lineDirection) < 0 ? -1 : 1;
        break; }

    case ST_NEGATIVE:
        a = V2d_PointOnLineSide(box->max, linePoint, lineDirection) < 0 ? -1 : 1;
        b = V2d_PointOnLineSide(box->min, linePoint, lineDirection) < 0 ? -1 : 1;
        break;
    }

    if(a == b) return a;
    return 0;
}

int M_BoxOnLineSide_FixedPrecision(const fixed_t box[], const fixed_t linePoint[], const fixed_t lineDirection[])
{
    int a = 0, b = 0;

    switch(M_SlopeTypeXY_FixedPrecision(lineDirection[0], lineDirection[1]))
    {
    case ST_HORIZONTAL:
        a = box[BOXTOP]    > linePoint[VY]? -1 : 1;
        b = box[BOXBOTTOM] > linePoint[VY]? -1 : 1;
        if(lineDirection[VX] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_VERTICAL:
        a = box[BOXRIGHT] < linePoint[VX]? -1 : 1;
        b = box[BOXLEFT]  < linePoint[VX]? -1 : 1;
        if(lineDirection[VY] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_POSITIVE: {
        fixed_t topLeft[2]     = { box[BOXLEFT],  box[BOXTOP]    };
        fixed_t bottomRight[2] = { box[BOXRIGHT], box[BOXBOTTOM] };
        a = V2x_PointOnLineSide(topLeft,     linePoint, lineDirection)? -1 : 1;
        b = V2x_PointOnLineSide(bottomRight, linePoint, lineDirection)? -1 : 1;
        break; }

    case ST_NEGATIVE: {
        fixed_t boxMax[2] = { box[BOXRIGHT], box[BOXTOP]    };
        fixed_t boxMin[2] = { box[BOXLEFT],  box[BOXBOTTOM] };
        a = V2x_PointOnLineSide(boxMax, linePoint, lineDirection)? -1 : 1;
        b = V2x_PointOnLineSide(boxMin, linePoint, lineDirection)? -1 : 1;
        break; }
    }

    if(a == b) return a;
    return 0; // on the line
}

int M_BoxOnLineSide2(const AABoxd* box, double const linePoint[], double const lineDirection[],
    double linePerp, double lineLength, double epsilon)
{
#define NORMALIZE(v)        ((v) < 0? -1 : (v) > 0? 1 : 0)

    double delta;
    int a, b;

    switch(M_SlopeType(lineDirection))
    {
    default: // Shut up compiler.
    case ST_HORIZONTAL:
        a = box->maxY > linePoint[VY]? -1 : 1;
        b = box->minY > linePoint[VY]? -1 : 1;
        if(lineDirection[VX] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_VERTICAL:
        a = box->maxX < linePoint[VX]? -1 : 1;
        b = box->minX < linePoint[VX]? -1 : 1;
        if(lineDirection[VY] < 0)
        {
            a = -a;
            b = -b;
        }
        break;

    case ST_POSITIVE: {
        double topLeft[2]       = { box->minX, box->maxY };
        double bottomRight[2]   = { box->maxX, box->minY };

        delta = V2d_PointOnLineSide2(topLeft,     lineDirection, linePerp, lineLength, epsilon);
        a = NORMALIZE(delta);

        delta = V2d_PointOnLineSide2(bottomRight, lineDirection, linePerp, lineLength, epsilon);
        b = NORMALIZE(delta);
        break; }

    case ST_NEGATIVE:
        delta = V2d_PointOnLineSide2(box->max, lineDirection, linePerp, lineLength, epsilon);
        a = NORMALIZE(delta);

        delta = V2d_PointOnLineSide2(box->min, lineDirection, linePerp, lineLength, epsilon);
        b = NORMALIZE(delta);
        break;
    }

    if(a == b) return a;
    return 0;

#undef NORMALIZE
}
