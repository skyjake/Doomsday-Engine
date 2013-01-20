/**\file r_util.cpp
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * Refresh Utility Routines.
 */

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_misc.h"

#include "render/r_main.h"

int Partition_PointXYOnSide(const partition_t* par, coord_t x, coord_t y)
{
    coord_t delta[2];
    coord_t left, right;

    if(!par->direction[VX])
    {
        if(x <= par->origin[VX])
            return (par->direction[VY] > 0? 1:0);
        else
            return (par->direction[VY] < 0? 1:0);
    }
    if(!par->direction[VY])
    {
        if(y <= par->origin[VY])
            return (par->direction[VX] < 0? 1:0);
        else
            return (par->direction[VX] > 0? 1:0);
    }

    delta[VX] = (x - par->origin[VX]);
    delta[VY] = (y - par->origin[VY]);

    // Try to quickly decide by looking at the signs.
    if(par->direction[VX] < 0)
    {
        if(par->direction[VY] < 0)
        {
            if(delta[VX] < 0)
            {
                if(delta[VY] >= 0) return 0;
            }
            else if(delta[VY] < 0)
            {
                return 1;
            }
        }
        else
        {
            if(delta[VX] < 0)
            {
                if(delta[VY] < 0) return 1;
            }
            else if(delta[VY] >= 0)
            {
                return 0;
            }
        }
    }
    else
    {
        if(par->direction[VY] < 0)
        {
            if(delta[VX] < 0)
            {
                if(delta[VY] < 0) return 0;
            }
            else if(delta[VY] >= 0)
            {
                return 1;
            }
        }
        else
        {
            if(delta[VX] < 0)
            {
                if(delta[VY] >= 0) return 1;
            }
            else if(delta[VY] < 0)
            {
                return 0;
            }
        }
    }

    left = par->direction[VY] * delta[VX];
    right = delta[VY] * par->direction[VX];

    if(right < left)
        return 0; // front side
    else
        return 1; // back side
}

int Partition_PointOnSide(const partition_t* par, coord_t const point[2])
{
    return Partition_PointXYOnSide(par, point[VX], point[VY]);
}

angle_t R_ViewPointXYToAngle(coord_t x, coord_t y)
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
    x -= viewData->current.origin[VX];
    y -= viewData->current.origin[VY];
    return M_PointXYToAngle(x, y);
}

coord_t R_ViewPointXYDistance(coord_t x, coord_t y)
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
    coord_t point[2] = { x, y };
    return M_PointDistance(viewData->current.origin, point);
}

void R_ProjectViewRelativeLine2D(coord_t const center[2], boolean alignToViewPlane,
    coord_t width, coord_t offset, coord_t start[2], coord_t end[2])
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
    float sinrv, cosrv;

    if(alignToViewPlane)
    {
        // Should be fully aligned to view plane.
        sinrv = -viewData->viewCos;
        cosrv = viewData->viewSin;
    }
    else
    {
        coord_t trX, trY;
        float thangle;

        // Transform the origin point.
        trX = center[VX] - viewData->current.origin[VX];
        trY = center[VY] - viewData->current.origin[VY];

        thangle = BANG2RAD(bamsAtan2(trY * 10, trX * 10)) - float(de::PI) / 2;
        sinrv = sin(thangle);
        cosrv = cos(thangle);
    }

    start[VX] = center[VX];
    start[VY] = center[VY];

    start[VX] -= cosrv * ((width / 2) + offset);
    start[VY] -= sinrv * ((width / 2) + offset);
    end[VX] = start[VX] + cosrv * width;
    end[VY] = start[VY] + sinrv * width;
}

void R_AmplifyColor(float rgb[3])
{
    float max = 0;
    int i;

    for(i = 0; i < 3; ++i)
    {
        if(rgb[i] > max)
            max = rgb[i];
    }
    if(!max || max == 1) return;

    for(i = 0; i < 3; ++i)
    {
        rgb[i] = rgb[i] / max;
    }
}

void R_ScaleAmbientRGB(float *out, const float *in, float mul)
{
    int                 i;
    float               val;

    if(mul < 0)
        mul = 0;
    else if(mul > 1)
        mul = 1;

    for(i = 0; i < 3; ++i)
    {
        val = in[i] * mul;

        if(out[i] < val)
            out[i] = val;
    }
}

boolean R_GenerateTexCoords(pvec2f_t s, pvec2f_t t, const_pvec3d_t point, float xScale, float yScale,
    const_pvec3d_t v1, const_pvec3d_t v2, const_pvec3f_t tangent, const_pvec3f_t bitangent)
{
    vec3d_t vToPoint;

    V3d_Subtract(vToPoint, v1, point);
    s[0] = V3d_DotProductf(vToPoint, tangent)   * xScale + .5f;
    t[0] = V3d_DotProductf(vToPoint, bitangent) * yScale + .5f;

    // Is the origin point visible?
    if(s[0] >= 1 || t[0] >= 1)
        return false; // Right on the X axis or below on the Y axis.

    V3d_Subtract(vToPoint, v2, point);
    s[1] = V3d_DotProductf(vToPoint, tangent)   * xScale + .5f;
    t[1] = V3d_DotProductf(vToPoint, bitangent) * yScale + .5f;

    // Is the end point visible?
    if(s[1] <= 0 || t[1] <= 0)
        return false; // Left on the X axis or above on the Y axis.

    return true;
}

#undef R_ChooseAlignModeAndScaleFactor
DENG_EXTERN_C boolean R_ChooseAlignModeAndScaleFactor(float* scale, int width, int height,
    int availWidth, int availHeight, scalemode_t scaleMode)
{
    if(SCALEMODE_STRETCH == scaleMode)
    {
        if(NULL != scale)
            *scale = 1;
        return true;
    }
    else
    {
        const float availRatio = (float)availWidth / availHeight;
        const float origRatio  = (float)width  / height;
        float sWidth, sHeight; // Scaled dimensions.

        if(availWidth >= availHeight)
        {
            sWidth  = availWidth;
            sHeight = sWidth  / availRatio;
        }
        else
        {
            sHeight = availHeight;
            sWidth  = sHeight * availRatio;
        }

        if(origRatio > availRatio)
        {
            if(NULL != scale)
                *scale = sWidth / width;
            return false;
        }
        else
        {
            if(NULL != scale)
                *scale = sHeight / height;
            return true;
        }
    }
}

#undef R_ChooseScaleMode2
DENG_EXTERN_C scalemode_t R_ChooseScaleMode2(int width, int height, int availWidth, int availHeight,
    scalemode_t overrideMode, float stretchEpsilon)
{
    const float availRatio = (float)availWidth / availHeight;
    const float origRatio  = (float)width / height;

    // Considered identical?
    if(INRANGE_OF(availRatio, origRatio, .001f))
        return SCALEMODE_STRETCH;

    if(SCALEMODE_STRETCH == overrideMode || SCALEMODE_NO_STRETCH  == overrideMode)
        return overrideMode;

    // Within tolerable stretch range?
    return INRANGE_OF(availRatio, origRatio, stretchEpsilon)? SCALEMODE_STRETCH : SCALEMODE_NO_STRETCH;
}

#undef R_ChooseScaleMode
DENG_EXTERN_C scalemode_t R_ChooseScaleMode(int width, int height, int availWidth, int availHeight,
    scalemode_t overrideMode)
{
    return R_ChooseScaleMode2(availWidth, availHeight, width, height, overrideMode,
        DEFAULT_SCALEMODE_STRETCH_EPSILON);
}
