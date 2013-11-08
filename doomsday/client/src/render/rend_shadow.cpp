/** @file rend_shadow.cpp
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

#include <de/Vector>

#include "de_base.h"
#include "de_graphics.h"
#include "de_play.h"
#include "de_render.h"
#include "de_system.h"
#include "clientapp.h"

#include "world/map.h"
#include "MaterialSnapshot"
#include "WallEdge"

#include "render/rend_shadow.h"

using namespace de;

/**
 * Generates a new primitive for the shadow projection.
 *
 * @param shadowList  Draw list to write the projected geometry to.
 * @param tp          The projected texture.
 * @param parm        Shadow drawer parameters.
 */
static void drawShadow(DrawList &shadowList, TexProjection const &tp,
    rendershadowprojectionparams_t &parm)
{
    // Allocate enough for the divisions too.
    Vector3f *rvertices  = R_AllocRendVertices(parm.realNumVertices);
    Vector2f *rtexcoords = R_AllocRendTexCoords(parm.realNumVertices);
    Vector4f *rcolors    = R_AllocRendColors(parm.realNumVertices);
    bool const mustSubdivide = (parm.isWall && (parm.wall.leftEdge->divisionCount() || parm.wall.rightEdge->divisionCount() ));

    for(uint i = 0; i < parm.numVertices; ++i)
    {
        rcolors[i] = tp.color;
    }

    if(parm.isWall)
    {
        WallEdge const &leftEdge  = *parm.wall.leftEdge;
        WallEdge const &rightEdge = *parm.wall.rightEdge;

        rtexcoords[1].x = rtexcoords[0].x = tp.topLeft.x;
        rtexcoords[1].y = rtexcoords[3].y = tp.topLeft.y;
        rtexcoords[3].x = rtexcoords[2].x = tp.bottomRight.x;
        rtexcoords[2].y = rtexcoords[0].y = tp.bottomRight.y;

        if(mustSubdivide)
        {
            /*
             * Need to swap indices around into fans set the position
             * of the division vertices, interpolate texcoords and
             * color.
             */

            Vector3f origVerts[4]; std::memcpy(origVerts, parm.rvertices, sizeof(Vector3f) * 4);
            Vector2f origTexCoords[4]; std::memcpy(origTexCoords, rtexcoords, sizeof(Vector2f) * 4);
            Vector4f origColors[4]; std::memcpy(origColors, rcolors, sizeof(Vector4f) * 4);

            R_DivVerts(rvertices, origVerts, leftEdge, rightEdge);
            R_DivTexCoords(rtexcoords, origTexCoords, leftEdge, rightEdge);
            R_DivVertColors(rcolors, origColors, leftEdge, rightEdge);
        }
        else
        {
            std::memcpy(rvertices, parm.rvertices, sizeof(Vector3f) * parm.numVertices);
        }
    }
    else
    {
        // It's a flat.
        float const width  = parm.bottomRight->x - parm.topLeft->x;
        float const height = parm.bottomRight->y - parm.topLeft->y;

        for(uint i = 0; i < parm.numVertices; ++i)
        {
            rtexcoords[i].x = ((parm.bottomRight->x - parm.rvertices[i].x) / width * tp.topLeft.x) +
                ((parm.rvertices[i].x - parm.topLeft->x) / width * tp.bottomRight.x);

            rtexcoords[i].y = ((parm.bottomRight->y - parm.rvertices[i].y) / height * tp.topLeft.y) +
                ((parm.rvertices[i].y - parm.topLeft->y) / height * tp.bottomRight.y);
        }

        std::memcpy(rvertices, parm.rvertices, sizeof(Vector3f) * parm.numVertices);
    }

    if(mustSubdivide)
    {
        WallEdge const &leftEdge  = *parm.wall.leftEdge;
        WallEdge const &rightEdge = *parm.wall.rightEdge;

        shadowList.write(gl::TriangleFan,
                         BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                         Vector2f(1, 1), Vector2f(0, 0),
                         0, 3 + rightEdge.divisionCount(),
                         rvertices  + 3 + leftEdge.divisionCount(),
                         rcolors    + 3 + leftEdge.divisionCount(),
                         rtexcoords + 3 + leftEdge.divisionCount())
                  .write(gl::TriangleFan,
                         BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                         Vector2f(1, 1), Vector2f(0, 0),
                         0, 3 + leftEdge.divisionCount(),
                         rvertices, rcolors, rtexcoords);
    }
    else
    {
        shadowList.write(parm.isWall? gl::TriangleStrip : gl::TriangleFan,
                         BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                         Vector2f(1, 1), Vector2f(0, 0),
                         0, parm.numVertices,
                         rvertices, rcolors, rtexcoords);
    }

    R_FreeRendVertices(rvertices);
    R_FreeRendTexCoords(rtexcoords);
    R_FreeRendColors(rcolors);
}

struct drawshadowworker_params_t
{
    DrawList *shadowList;
    rendershadowprojectionparams_t *drawShadowParms;
};

static int drawShadowWorker(TexProjection const *tp, void *context)
{
    drawshadowworker_params_t &p = *static_cast<drawshadowworker_params_t *>(context);
    drawShadow(*p.shadowList, *tp, *p.drawShadowParms);
    return 0; // Continue iteration.
}

void Rend_RenderShadowProjections(uint listIdx, rendershadowprojectionparams_t &p)
{
    DrawListSpec listSpec;
    listSpec.group = ShadowGeom;
    listSpec.texunits[TU_PRIMARY] =
        GLTextureUnit(GL_PrepareLSTexture(LST_DYNAMIC), gl::ClampToEdge, gl::ClampToEdge);

    // Write shadows to the render lists.
    drawshadowworker_params_t parm; zap(parm);
    parm.shadowList      = &ClientApp::renderSystem().drawLists().find(listSpec);
    parm.drawShadowParms = &p;

    Rend_IterateProjectionList(listIdx, drawShadowWorker, &parm);
}
