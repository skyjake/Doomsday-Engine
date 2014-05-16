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

#include "de_base.h"
#include "clientapp.h"
#include "render/rend_shadow.h"

#include "de_graphics.h"
#include "de_play.h"
#include "de_render.h"
#include "de_system.h"
#include "world/map.h"
#include "DrawLists"
#include "MaterialSnapshot"
#include "WallEdge"
#include "gl/gl_texmanager.h"
#include <de/Vector>

using namespace de;

/**
 * Generates a new primitive for the shadow projection.
 *
 * @param shadowList  Draw list to write the projected geometry to.
 * @param tp          The projected texture.
 * @param parm        Shadow drawer parameters.
 */
static void drawShadow(DrawList &shadowList, TexProjection const &tp,
    rendershadowprojectionparams_t &p)
{
    RenderSystem &rendSys = ClientApp::renderSystem();
    WorldVBuf &vbuf       = rendSys.buffer();

    // Allocate enough for the divisions too.
    Vector3f *rvertices  = rendSys.posPool().alloc(p.realNumVertices);
    Vector2f *rtexcoords = rendSys.texPool().alloc(p.realNumVertices);
    Vector4f *rcolors    = rendSys.colorPool().alloc(p.realNumVertices);
    bool const mustSubdivide = (p.isWall && (p.wall.leftEdge->divisionCount() || p.wall.rightEdge->divisionCount() ));

    for(uint i = 0; i < p.numVertices; ++i)
    {
        rcolors[i] = tp.color;
    }

    if(p.isWall)
    {
        WallEdgeSection const &sectionLeft  = *p.wall.leftEdge;
        WallEdgeSection const &sectionRight = *p.wall.rightEdge;

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

            Vector3f origVerts[4]; std::memcpy(origVerts, p.rvertices, sizeof(Vector3f) * 4);
            Vector2f origTexCoords[4]; std::memcpy(origTexCoords, rtexcoords, sizeof(Vector2f) * 4);
            Vector4f origColors[4]; std::memcpy(origColors, rcolors, sizeof(Vector4f) * 4);

            R_DivVerts(rvertices, origVerts, sectionLeft, sectionRight);
            R_DivTexCoords(rtexcoords, origTexCoords, sectionLeft, sectionRight);
            R_DivVertColors(rcolors, origColors, sectionLeft, sectionRight);
        }
        else
        {
            std::memcpy(rvertices, p.rvertices, sizeof(Vector3f) * p.numVertices);
        }
    }
    else
    {
        // It's a flat.
        float const width  = p.bottomRight->x - p.topLeft->x;
        float const height = p.bottomRight->y - p.topLeft->y;

        for(uint i = 0; i < p.numVertices; ++i)
        {
            rtexcoords[i].x = ((p.bottomRight->x - p.rvertices[i].x) / width * tp.topLeft.x) +
                ((p.rvertices[i].x - p.topLeft->x) / width * tp.bottomRight.x);

            rtexcoords[i].y = ((p.bottomRight->y - p.rvertices[i].y) / height * tp.topLeft.y) +
                ((p.rvertices[i].y - p.topLeft->y) / height * tp.bottomRight.y);
        }

        std::memcpy(rvertices, p.rvertices, sizeof(Vector3f) * p.numVertices);
    }

    if(mustSubdivide)
    {
        WallEdgeSection const &sectionLeft  = *p.wall.leftEdge;
        WallEdgeSection const &sectionRight = *p.wall.rightEdge;

        {
            WorldVBuf::Index vertCount = 3 + sectionRight.divisionCount();
            WorldVBuf::Index *indices  = rendSys.indicePool().alloc(vertCount);
            vbuf.reserveElements(vertCount, indices);
            vbuf.setVertices(vertCount, indices,
                             rvertices  + 3 + sectionLeft.divisionCount(),
                             rcolors    + 3 + sectionLeft.divisionCount(),
                             rtexcoords + 3 + sectionLeft.divisionCount());

            shadowList.write(gl::TriangleFan, vertCount, indices);

            rendSys.indicePool().release(indices);
        }
        {
            WorldVBuf::Index vertCount = 3 + sectionLeft.divisionCount();
            WorldVBuf::Index *indices  = rendSys.indicePool().alloc(vertCount);
            vbuf.reserveElements(vertCount, indices);
            vbuf.setVertices(vertCount, indices,
                             rvertices, rcolors, rtexcoords);

            shadowList.write(gl::TriangleFan, vertCount, indices);

            rendSys.indicePool().release(indices);
        }
    }
    else
    {
        WorldVBuf::Index vertCount = p.numVertices;
        WorldVBuf::Index *indices  = rendSys.indicePool().alloc(vertCount);
        vbuf.reserveElements(vertCount, indices);
        vbuf.setVertices(vertCount, indices,
                         rvertices, rcolors, rtexcoords);

        shadowList.write(p.isWall? gl::TriangleStrip : gl::TriangleFan,
                         vertCount, indices);

        rendSys.indicePool().release(indices);
    }

    rendSys.posPool().release(rvertices);
    rendSys.texPool().release(rtexcoords);
    rendSys.colorPool().release(rcolors);
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
