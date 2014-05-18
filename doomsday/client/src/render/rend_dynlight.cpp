/** @file rend_dynlight.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include "render/rend_dynlight.h"

#include "de_graphics.h"
#include "de_render.h"
#include "DrawLists"
#include "WallEdge"

using namespace de;

static void drawDynlight(TexProjection const &tp, renderlightprojectionparams_t &p)
{
    RenderSystem &rendSys = ClientApp::renderSystem();
    WorldVBuf &vbuf       = rendSys.worldVBuf();

    // If multitexturing is in use we skip the first.
    if(!(Rend_IsMTexLights() && p.lastIdx == 0))
    {
        DrawListSpec listSpec;
        listSpec.group = LightGeom;
        listSpec.texunits[TU_PRIMARY] = GLTextureUnit(tp.texture, gl::ClampToEdge, gl::ClampToEdge);

        DrawList &lightList = rendSys.drawLists().find(listSpec);

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

                lightList.write(gl::TriangleFan, rightFanSize, indices + leftFanSize)
                         .write(gl::TriangleFan, leftFanSize, indices);

                rendSys.indicePool().release(indices);
            }
            else
            {
                WorldVBuf::Index vertCount = p.vertCount;
                WorldVBuf::Index *indices  = rendSys.indicePool().alloc(vertCount);

                vbuf.reserveElements(vertCount, indices);
                for(int i = 0; i < vertCount; ++i)
                {
                    WorldVBuf::Type &vertex = vbuf[indices[i]];
                    vertex.pos  = p.posCoords[i];// vbuf[p.indices[i]].pos;
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

                lightList.write(gl::TriangleStrip, vertCount, indices);

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
                vertex.rgba = tp.color;

                vertex.texCoord[WorldVBuf::PrimaryTex] =
                    Vector2f(((p.bottomRight->x - vertex.pos.x) / pDimensions.x * tp.topLeft.x) +
                             ((vertex.pos.x     - p.topLeft->x) / pDimensions.x * tp.bottomRight.x)
                             ,
                             ((p.bottomRight->y - vertex.pos.y) / pDimensions.y * tp.topLeft.y) +
                             ((vertex.pos.y     - p.topLeft->y)     / pDimensions.y * tp.bottomRight.y));
            }

            lightList.write(gl::TriangleFan, vertCount, indices);

            rendSys.indicePool().release(indices);
        }
    }
    p.lastIdx++;
}

/// Generates a new primitive for each light projection.
static int drawDynlightWorker(TexProjection const *tp, void *context)
{
    drawDynlight(*tp, *static_cast<renderlightprojectionparams_t *>(context));
    return 0; // Continue iteration.
}

uint Rend_DrawProjectedLights(uint listIdx, renderlightprojectionparams_t &p)
{
    uint numRendered = p.lastIdx;

    Rend_IterateProjectionList(listIdx, drawDynlightWorker, (void *)&p);

    numRendered = p.lastIdx - numRendered;
    if(Rend_IsMTexLights())
        numRendered -= 1;

    return numRendered;
}
