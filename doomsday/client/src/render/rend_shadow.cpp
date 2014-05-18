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

    if(p.leftSection) // A wall.
    {
        WallEdgeSection const &leftSection  = *p.leftSection;
        WallEdgeSection const &rightSection = *p.rightSection;
        bool const mustSubdivide            = (leftSection.divisionCount() || rightSection.divisionCount());

        if(mustSubdivide) // Draw as two triangle fans.
        {
            WorldVBuf::Index const rightFanSize = 3 + rightSection.divisionCount();
            WorldVBuf::Index const leftFanSize  = 3 + leftSection.divisionCount();
            WorldVBuf::Index *indices = rendSys.indicePool().alloc(leftFanSize + rightFanSize);

            vbuf.reserveElements(leftFanSize + rightFanSize, indices);

            R_DivVerts(indices, p.posCoords, leftSection, rightSection);

            Vector2f quadCoords[4] = {
                Vector2f(tp.topLeft.x,     tp.bottomRight.y),
                Vector2f(tp.topLeft.x,     tp.topLeft.y    ),
                Vector2f(tp.bottomRight.x, tp.bottomRight.y),
                Vector2f(tp.bottomRight.x, tp.topLeft.y    )
            };
            R_DivTexCoords(indices, quadCoords, leftSection, rightSection,
                           WorldVBuf::PrimaryTex);

            for(WorldVBuf::Index i = 0; i < leftFanSize + rightFanSize; ++i)
            {
                WorldVBuf::Type &vertex = vbuf[indices[i]];
                //vertex.pos  = vbuf[p.indices[i]].pos;
                vertex.rgba = tp.color;
            }

            shadowList.write(gl::TriangleFan, rightFanSize, indices + leftFanSize)
                      .write(gl::TriangleFan, leftFanSize, indices);

            rendSys.indicePool().release(indices);
        }
        else
        {
            WorldVBuf::Index vertCount = p.vertCount;
            WorldVBuf::Index *indices  = rendSys.indicePool().alloc(vertCount);

            vbuf.reserveElements(vertCount, indices);
            for(WorldVBuf::Index i = 0; i < vertCount; ++i)
            {
                WorldVBuf::Type &vertex = vbuf[indices[i]];
                vertex.pos  = p.posCoords[i];//vbuf[p.indices[i]].pos;
                vertex.rgba = tp.color;
            }

            vbuf[indices[1]].texCoord[WorldVBuf::PrimaryTex].x =
            vbuf[indices[0]].texCoord[WorldVBuf::PrimaryTex].x = tp.topLeft.x;

            vbuf[indices[1]].texCoord[WorldVBuf::PrimaryTex].y =
            vbuf[indices[3]].texCoord[WorldVBuf::PrimaryTex].y = tp.topLeft.y;

            vbuf[indices[3]].texCoord[WorldVBuf::PrimaryTex].x =
            vbuf[indices[2]].texCoord[WorldVBuf::PrimaryTex].x = tp.bottomRight.x;

            vbuf[indices[2]].texCoord[WorldVBuf::PrimaryTex].y =
            vbuf[indices[0]].texCoord[WorldVBuf::PrimaryTex].y = tp.bottomRight.y;

            shadowList.write(gl::TriangleStrip, vertCount, indices);

            rendSys.indicePool().release(indices);
        }
    }
    else // A flat.
    {
        Vector2f const pDimensions = p.bottomRight->xy() - p.topLeft->xy();
        WorldVBuf::Index vertCount = p.vertCount;
        WorldVBuf::Index *indices  = rendSys.indicePool().alloc(vertCount);

        vbuf.reserveElements(vertCount, indices);
        for(WorldVBuf::Index i = 0; i < vertCount; ++i)
        {
            WorldVBuf::Type &vertex = vbuf[indices[i]];

            vertex.pos  = vbuf[p.indices[i]].pos;
            vertex.rgba =    tp.color;

            vertex.texCoord[WorldVBuf::PrimaryTex] =
                Vector2f(((p.bottomRight->x - vertex.pos.x) / pDimensions.x * tp.topLeft.x) +
                         ((vertex.pos.x     - p.topLeft->x) / pDimensions.x * tp.bottomRight.x)
                         ,
                         ((p.bottomRight->y - vertex.pos.y) / pDimensions.y * tp.topLeft.y) +
                         ((vertex.pos.y     - p.topLeft->y) / pDimensions.y * tp.bottomRight.y));
        }

        shadowList.write(gl::TriangleFan, vertCount, indices);

        rendSys.indicePool().release(indices);
    }
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

void Rend_DrawProjectedShadows(uint listIdx, rendershadowprojectionparams_t &p)
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
