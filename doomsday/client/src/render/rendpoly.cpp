/** @file rendpoly.cpp RendPoly data buffers
 * @ingroup render
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include "de_console.h"
#include "de_render.h"

#include "color.h"
#include "WallEdge"

using namespace de;

enum RPolyDataType
{
    RPT_VERT,
    RPT_COLOR,
    RPT_TEXCOORD
};

struct RPolyData
{
    boolean inUse;
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

    Con_Printf("RP Count: %-4i\n", numrendpolys);

    for(uint i = 0; i < numrendpolys; ++i)
    {
        RPolyData* rp = rendPolys[i];

        Con_Printf("RP: %-4u %c (vtxs=%u t=%c)\n", i,
                   (rp->inUse? 'Y':'N'), rp->num,
                   (rp->type == RPT_VERT? 'v' : rp->type == RPT_COLOR?'c' : 't'));
    }
}

void R_InitRendPolyPools()
{
    numrendpolys = maxrendpolys = 0;
    rendPolys = 0;

    rvertex_t *rvertices = R_AllocRendVertices(24);
    Vector4f *rcolors    = R_AllocRendColors(24);
    Vector2f *rtexcoords = R_AllocRendTexCoords(24);

    // Mark unused.
    R_FreeRendVertices(rvertices);
    R_FreeRendColors(rcolors);
    R_FreeRendTexCoords(rtexcoords);
}

rvertex_t* R_AllocRendVertices(uint num)
{
    uint idx;
    boolean found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse) continue;
        if(rendPolys[idx]->type != RPT_VERT) continue;

        if(rendPolys[idx]->num >= num)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return (rvertex_t*) rendPolys[idx]->data;
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
            RPolyData* newPolyData, *ptr;

            maxrendpolys = (maxrendpolys > 0? maxrendpolys * 2 : 8);

            rendPolys = (RPolyData**) Z_Realloc(rendPolys, sizeof(RPolyData*) * maxrendpolys, PU_MAP);

            newCount = maxrendpolys - numrendpolys + 1;

            newPolyData = (RPolyData*) Z_Malloc(sizeof(RPolyData) * newCount, PU_MAP, 0);

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
    rendPolys[idx]->data  = Z_Malloc(sizeof(rvertex_t) * num, PU_MAP, 0);

    return (rvertex_t*) rendPolys[idx]->data;
}

Vector4f *R_AllocRendColors(uint num)
{
    uint idx;
    boolean found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse) continue;
        if(rendPolys[idx]->type != RPT_COLOR) continue;

        if(rendPolys[idx]->num >= num)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return (Vector4f *) rendPolys[idx]->data;
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
    rendPolys[idx]->data  = Z_Malloc(sizeof(Vector4f) * num, PU_MAP, 0);

    return (Vector4f *) rendPolys[idx]->data;
}

Vector2f *R_AllocRendTexCoords(uint num)
{
    uint idx;
    boolean found = false;

    for(idx = 0; idx < maxrendpolys; ++idx)
    {
        if(rendPolys[idx]->inUse) continue;
        if(rendPolys[idx]->type != RPT_TEXCOORD) continue;

        if(rendPolys[idx]->num >= num)
        {
            // Use this one.
            rendPolys[idx]->inUse = true;
            return (Vector2f *) rendPolys[idx]->data;
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
    rendPolys[idx]->data  = Z_Malloc(sizeof(Vector2f) * num, PU_MAP, 0);

    return (Vector2f *) rendPolys[idx]->data;
}

void R_FreeRendVertices(rvertex_t* rvertices)
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

    DEBUG_Message(("R_FreeRendPoly: Dangling poly ptr!\n"));
}

void R_FreeRendColors(Vector4f *rcolors)
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

    DEBUG_Message(("R_FreeRendPoly: Dangling poly ptr!\n"));
}

void R_FreeRendTexCoords(Vector2f *rtexcoords)
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

    DEBUG_Message(("R_FreeRendPoly: Dangling poly ptr!\n"));
}

void Rtu_Init(rtexmapunit_t *rtu)
{
    DENG_ASSERT(rtu != 0);
    rtu->texture.gl.name    = 0;
    rtu->texture.gl.wrapS   = GL_REPEAT;
    rtu->texture.gl.wrapT   = GL_REPEAT;
    rtu->texture.gl.magMode = GL_LINEAR;
    rtu->texture.flags      = 0;
    rtu->blendMode = BM_NORMAL;
    rtu->opacity = 1;
    rtu->scale[0] = rtu->scale[1] = 1;
    rtu->offset[0] = rtu->offset[1] = 0;
}

void Rtu_SetScale(rtexmapunit_t *rtu, Vector2f const &st)
{
    DENG_ASSERT(rtu != 0);
    rtu->scale[0] = st.x;
    rtu->scale[1] = st.y;
}

void Rtu_Scale(rtexmapunit_t *rtu, float scalar)
{
    DENG_ASSERT(rtu != 0);
    rtu->scale[0] *= scalar;
    rtu->scale[1] *= scalar;
    rtu->offset[0] *= scalar;
    rtu->offset[1] *= scalar;
}

void Rtu_ScaleST(rtexmapunit_t *rtu, Vector2f const &scalarST)
{
    DENG_ASSERT(rtu != 0);
    rtu->scale[0] *= scalarST.x;
    rtu->scale[1] *= scalarST.y;
    rtu->offset[0] *= scalarST.x;
    rtu->offset[1] *= scalarST.y;
}

void Rtu_SetOffset(rtexmapunit_t *rtu, Vector2f const &st)
{
    DENG_ASSERT(rtu != 0);
    rtu->offset[0] = st.x;
    rtu->offset[1] = st.y;
}

void Rtu_TranslateOffset(rtexmapunit_t *rtu, Vector2f const &st)
{
    DENG_ASSERT(rtu != 0);
    rtu->offset[0] += st.x;
    rtu->offset[1] += st.y;
}

void R_DivVerts(rvertex_t *dst, rvertex_t const *src,
    WorldEdge const &leftEdge, WorldEdge const &rightEdge)
{
#define COPYVERT(d, s)  (d)->pos[VX] = (s)->pos[VX]; \
    (d)->pos[VY] = (s)->pos[VY]; \
    (d)->pos[VZ] = (s)->pos[VZ];

    int const numR = 3 + rightEdge.divisionCount();
    int const numL = 3 + leftEdge.divisionCount();

    if(numR + numL == 6) return; // Nothing to do.

    // Right fan:
    COPYVERT(&dst[numL + 0], &src[0])
    COPYVERT(&dst[numL + 1], &src[3]);
    COPYVERT(&dst[numL + numR - 1], &src[2]);

    for(int n = 0; n < rightEdge.divisionCount(); ++n)
    {
        WorldEdge::Event const &icpt = rightEdge.at(rightEdge.lastDivision() - n);
        Vector3d const &origin = icpt.origin();
        V3f_Set(dst[numL + 2 + n].pos, origin.x, origin.y, origin.z);
    }

    // Left fan:
    COPYVERT(&dst[0], &src[3]);
    COPYVERT(&dst[1], &src[0]);
    COPYVERT(&dst[numL - 1], &src[1]);

    for(int n = 0; n < leftEdge.divisionCount(); ++n)
    {
        WorldEdge::Event const &icpt = leftEdge.at(leftEdge.firstDivision() + n);
        Vector3d const &origin = icpt.origin();
        V3f_Set(dst[2 + n].pos, origin.x, origin.y, origin.z);
    }

#undef COPYVERT
}

void R_DivTexCoords(Vector2f *dst, Vector2f const *src,
    WorldEdge const &leftEdge, WorldEdge const &rightEdge)
{
    int const numR = 3 + rightEdge.divisionCount();
    int const numL = 3 + leftEdge.divisionCount();

    if(numR + numL == 6) return; // Nothing to do.

    // Right fan:
    dst[numL + 0] = src[0];
    dst[numL + 1] = src[3];
    dst[numL + numR-1] = src[2];

    for(int n = 0; n < rightEdge.divisionCount(); ++n)
    {
        WorldEdge::Event const &icpt = rightEdge.at(rightEdge.lastDivision() - n);
        dst[numL + 2 + n].x = src[3].x;
        dst[numL + 2 + n].y = src[2].y + (src[3].y - src[2].y) * icpt.distance();
    }

    // Left fan:
    dst[0] = src[3];
    dst[1] = src[0];
    dst[numL - 1] = src[1];

    for(int n = 0; n < leftEdge.divisionCount(); ++n)
    {
        WorldEdge::Event const &icpt = leftEdge.at(leftEdge.firstDivision() + n);
        dst[2 + n].x = src[0].x;
        dst[2 + n].y = src[0].y + (src[1].y - src[0].y) * icpt.distance();
    }
}

void R_DivVertColors(Vector4f *dst, Vector4f const *src,
    WorldEdge const &leftEdge, WorldEdge const &rightEdge)
{
    int const numR = 3 + rightEdge.divisionCount();
    int const numL = 3 + leftEdge.divisionCount();

    if(numR + numL == 6) return; // Nothing to do.

    // Right fan:
    dst[numL + 0] = src[0];
    dst[numL + 1] = src[3];
    dst[numL + numR-1] = src[2];

    for(int n = 0; n < rightEdge.divisionCount(); ++n)
    {
        WorldEdge::Event const &icpt = rightEdge.at(rightEdge.lastDivision() - n);
        dst[numL + 2 + n] = src[2] + (src[3] - src[2]) * icpt.distance();
    }

    // Left fan:
    dst[0] = src[3];
    dst[1] = src[0];
    dst[numL - 1] = src[1];

    for(int n = 0; n < leftEdge.divisionCount(); ++n)
    {
        WorldEdge::Event const &icpt = leftEdge.at(leftEdge.firstDivision() + n);
        dst[2 + n] = src[0] + (src[1] - src[0]) * icpt.distance();
    }
}
