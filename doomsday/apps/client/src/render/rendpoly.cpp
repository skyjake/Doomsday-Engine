/** @file rendpoly.cpp  RendPoly data buffers.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "render/rendpoly.h"

#include "misc/color.h"
#include "render/walledge.h"

using namespace de;

enum RPolyDataType
{
    RPT_VERT,
    RPT_COLOR,
    RPT_TEXCOORD
};

struct RPolyData
{
    dd_bool inUse;
    RPolyDataType type;
    uint num;
    void* data;
};

byte rendInfoRPolys = 0;

static unsigned int numrendpolys = 0;
static unsigned int maxrendpolys = 0;
static RPolyData** rendPolys;

void R_PrintRendPoolInfo()
{
    if(!rendInfoRPolys) return;

    LOGDEV_GL_MSG("RP Count: %-4i") << numrendpolys;

    for(uint i = 0; i < numrendpolys; ++i)
    {
        RPolyData* rp = rendPolys[i];

        LOGDEV_GL_VERBOSE(_E(m) "RP: %-4u %c (vtxs=%i t=%c)")
                << i << (rp->inUse? 'Y':'N') << rp->num
                << (rp->type == RPT_VERT? 'v' : rp->type == RPT_COLOR? 'c' : 't');
    }
}

void R_InitRendPolyPools()
{
    numrendpolys = maxrendpolys = 0;
    rendPolys = 0;

    Vec3f *rvertices  = R_AllocRendVertices(24);
    Vec4f *rcolors    = R_AllocRendColors(24);
    Vec2f *rtexcoords = R_AllocRendTexCoords(24);

    // Mark unused.
    R_FreeRendVertices(rvertices);
    R_FreeRendColors(rcolors);
    R_FreeRendTexCoords(rtexcoords);
}

Vec3f *R_AllocRendVertices(uint num)
{
    uint idx;
    dd_bool found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse) continue;
        if(rendPolys[idx]->type != RPT_VERT) continue;

        if(rendPolys[idx]->num >= num)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return (Vec3f *) rendPolys[idx]->data;
        }
        else if(rendPolys[idx]->num == 0)
        {
            // There is an unused one but we haven't allocated verts yet.
            numrendpolys++;
            found = true;
            break;
        }
    }

    if(!found)
    {
        // We may need to allocate more.
        if(++numrendpolys > maxrendpolys)
        {
            uint i, newCount;
            RPolyData *newPolyData, *ptr;

            maxrendpolys = (maxrendpolys > 0? maxrendpolys * 2 : 8);

            rendPolys = (RPolyData **) Z_Realloc(rendPolys, sizeof(RPolyData *) * maxrendpolys, PU_MAP);

            newCount = maxrendpolys - numrendpolys + 1;

            newPolyData = (RPolyData *) Z_Malloc(sizeof(RPolyData) * newCount, PU_MAP, 0);

            ptr = newPolyData;
            for(i = numrendpolys-1; i < maxrendpolys; ++i, ptr++)
            {
                ptr->inUse = false;
                ptr->num = 0;
                ptr->data = NULL;
                ptr->type = RPT_VERT;
                rendPolys[i] = ptr;
            }
        }
        idx = numrendpolys - 1;
    }

    rendPolys[idx]->inUse = true;
    rendPolys[idx]->type  = RPT_VERT;
    rendPolys[idx]->num   = num;
    rendPolys[idx]->data  = Z_Malloc(sizeof(Vec3f) * num, PU_MAP, 0);

    return (Vec3f *) rendPolys[idx]->data;
}

Vec4f *R_AllocRendColors(uint num)
{
    uint idx;
    dd_bool found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse) continue;
        if(rendPolys[idx]->type != RPT_COLOR) continue;

        if(rendPolys[idx]->num >= num)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return (Vec4f *) rendPolys[idx]->data;
        }
        else if(rendPolys[idx]->num == 0)
        {
            // There is an unused one but we haven't allocated verts yet.
            numrendpolys++;
            found = true;
            break;
        }
    }

    if(!found)
    {
        // We may need to allocate more.
        if(++numrendpolys > maxrendpolys)
        {
            uint i, newCount;
            RPolyData *newPolyData, *ptr;

            maxrendpolys = (maxrendpolys > 0? maxrendpolys * 2 : 8);

            rendPolys = (RPolyData **) Z_Realloc(rendPolys, sizeof(RPolyData *) * maxrendpolys, PU_MAP);

            newCount = maxrendpolys - numrendpolys + 1;

            newPolyData = (RPolyData *) Z_Malloc(sizeof(RPolyData) * newCount, PU_MAP, 0);

            ptr = newPolyData;
            for(i = numrendpolys-1; i < maxrendpolys; ++i, ptr++)
            {
                ptr->inUse = false;
                ptr->num = 0;
                ptr->data = NULL;
                ptr->type = RPT_COLOR;
                rendPolys[i] = ptr;
            }
        }
        idx = numrendpolys - 1;
    }

    rendPolys[idx]->inUse = true;
    rendPolys[idx]->type  = RPT_COLOR;
    rendPolys[idx]->num   = num;
    rendPolys[idx]->data  = Z_Malloc(sizeof(Vec4f) * num, PU_MAP, 0);

    return (Vec4f *) rendPolys[idx]->data;
}

Vec2f *R_AllocRendTexCoords(uint num)
{
    uint idx;
    dd_bool found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse) continue;
        if(rendPolys[idx]->type != RPT_TEXCOORD) continue;

        if(rendPolys[idx]->num >= num)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return (Vec2f *) rendPolys[idx]->data;
        }
        else if(rendPolys[idx]->num == 0)
        {
            // There is an unused one but we haven't allocated verts yet.
            numrendpolys++;
            found = true;
            break;
        }
    }

    if(!found)
    {
        // We may need to allocate more.
        if(++numrendpolys > maxrendpolys)
        {
            uint i, newCount;
            RPolyData *newPolyData, *ptr;

            maxrendpolys = (maxrendpolys > 0? maxrendpolys * 2 : 8);

            rendPolys = (RPolyData **) Z_Realloc(rendPolys, sizeof(RPolyData *) * maxrendpolys, PU_MAP);

            newCount = maxrendpolys - numrendpolys + 1;

            newPolyData = (RPolyData *) Z_Malloc(sizeof(RPolyData) * newCount, PU_MAP, 0);

            ptr = newPolyData;
            for(i = numrendpolys-1; i < maxrendpolys; ++i, ptr++)
            {
                ptr->inUse = false;
                ptr->num   = 0;
                ptr->data  = NULL;
                ptr->type  = RPT_TEXCOORD;

                rendPolys[i] = ptr;
            }
        }
        idx = numrendpolys - 1;
    }

    rendPolys[idx]->inUse = true;
    rendPolys[idx]->type  = RPT_TEXCOORD;
    rendPolys[idx]->num   = num;
    rendPolys[idx]->data  = Z_Malloc(sizeof(Vec2f) * num, PU_MAP, 0);

    return (Vec2f *) rendPolys[idx]->data;
}

void R_FreeRendVertices(Vec3f *rvertices)
{
    if(!rvertices) return;

    for(uint i = 0; i < numrendpolys; ++i)
    {
        if(rendPolys[i]->data == rvertices)
        {
            rendPolys[i]->inUse = false;
            return;
        }
    }

    LOGDEV_GL_WARNING("R_FreeRendPoly: Dangling poly ptr!");
}

void R_FreeRendColors(Vec4f *rcolors)
{
    if(!rcolors) return;

    for(uint i = 0; i < numrendpolys; ++i)
    {
        if(rendPolys[i]->data == rcolors)
        {
            rendPolys[i]->inUse = false;
            return;
        }
    }

    LOGDEV_GL_WARNING("R_FreeRendPoly: Dangling poly ptr!");
}

void R_FreeRendTexCoords(Vec2f *rtexcoords)
{
    if(!rtexcoords) return;

    for(uint i = 0; i < numrendpolys; ++i)
    {
        if(rendPolys[i]->data == rtexcoords)
        {
            rendPolys[i]->inUse = false;
            return;
        }
    }

    LOGDEV_GL_WARNING("R_FreeRendPoly: Dangling poly ptr!");
}

void R_DivVerts(Vec3f *dst, const Vec3f *src,
    const WorldEdge &leftEdge, const WorldEdge &rightEdge)
{
    const int numR = 3 + rightEdge.divisionCount();
    const int numL = 3 + leftEdge.divisionCount();

    if(numR + numL == 6) return; // Nothing to do.

    // Right fan:
    dst[numL + 0] = src[0];
    dst[numL + 1] = src[3];
    dst[numL + numR - 1] = src[2];

    for(int n = 0; n < rightEdge.divisionCount(); ++n)
    {
        const WorldEdge::Event &icpt = rightEdge.at(rightEdge.lastDivision() - n);
        dst[numL + 2 + n] = icpt.origin();
    }

    // Left fan:
    dst[0] = src[3];
    dst[1] = src[0];
    dst[numL - 1] = src[1];

    for(int n = 0; n < leftEdge.divisionCount(); ++n)
    {
        const WorldEdge::Event &icpt = leftEdge.at(leftEdge.firstDivision() + n);
        dst[2 + n] = icpt.origin();
    }
}

void R_DivTexCoords(Vec2f *dst, const Vec2f *src,
    const WorldEdge &leftEdge, const WorldEdge &rightEdge)
{
    const int numR = 3 + rightEdge.divisionCount();
    const int numL = 3 + leftEdge.divisionCount();

    if(numR + numL == 6) return; // Nothing to do.

    // Right fan:
    dst[numL + 0] = src[0];
    dst[numL + 1] = src[3];
    dst[numL + numR-1] = src[2];

    for(int n = 0; n < rightEdge.divisionCount(); ++n)
    {
        const WorldEdge::Event &icpt = rightEdge.at(rightEdge.lastDivision() - n);
        dst[numL + 2 + n].x = src[3].x;
        dst[numL + 2 + n].y = src[2].y + (src[3].y - src[2].y) * icpt.distance();
    }

    // Left fan:
    dst[0] = src[3];
    dst[1] = src[0];
    dst[numL - 1] = src[1];

    for(int n = 0; n < leftEdge.divisionCount(); ++n)
    {
        const WorldEdge::Event &icpt = leftEdge.at(leftEdge.firstDivision() + n);
        dst[2 + n].x = src[0].x;
        dst[2 + n].y = src[0].y + (src[1].y - src[0].y) * icpt.distance();
    }
}

void R_DivVertColors(Vec4f *dst, const Vec4f *src,
    const WorldEdge &leftEdge, const WorldEdge &rightEdge)
{
    const int numR = 3 + rightEdge.divisionCount();
    const int numL = 3 + leftEdge.divisionCount();

    if(numR + numL == 6) return; // Nothing to do.

    // Right fan:
    dst[numL + 0] = src[0];
    dst[numL + 1] = src[3];
    dst[numL + numR-1] = src[2];

    for(int n = 0; n < rightEdge.divisionCount(); ++n)
    {
        const WorldEdge::Event &icpt = rightEdge.at(rightEdge.lastDivision() - n);
        dst[numL + 2 + n] = src[2] + (src[3] - src[2]) * icpt.distance();
    }

    // Left fan:
    dst[0] = src[3];
    dst[1] = src[0];
    dst[numL - 1] = src[1];

    for(int n = 0; n < leftEdge.divisionCount(); ++n)
    {
        const WorldEdge::Event &icpt = leftEdge.at(leftEdge.firstDivision() + n);
        dst[2 + n] = src[0] + (src[1] - src[0]) * icpt.distance();
    }
}
