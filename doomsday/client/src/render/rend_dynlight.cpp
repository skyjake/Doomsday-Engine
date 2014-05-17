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

//#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"
#include "DrawLists"
#include "WallEdge"

using namespace de;

static void drawDynlight(TexProjection const &tp, renderlightprojectionparams_t &p)
{
    RenderSystem &rendSys = ClientApp::renderSystem();
    WorldVBuf &vbuf       = rendSys.buffer();

    // If multitexturing is in use we skip the first.
    if(!(Rend_IsMTexLights() && p.lastIdx == 0))
    {
        // Allocate enough for the divisions too.
        Vector3f *posCoords   = rendSys.posPool().alloc(p.realNumVertices);
        Vector2f *texCoords   = rendSys.texPool().alloc(p.realNumVertices);
        Vector4f *colorCoords = rendSys.colorPool().alloc(p.realNumVertices);
        bool const mustSubdivide = (p.isWall && (p.wall.leftEdge->divisionCount() || p.wall.rightEdge->divisionCount() ));

        for(uint i = 0; i < p.numVertices; ++i)
        {
            colorCoords[i] = tp.color;
        }

        if(p.isWall)
        {
            WallEdgeSection const &leftSection  = *p.wall.leftEdge;
            WallEdgeSection const &rightSection = *p.wall.rightEdge;

            texCoords[1].x = texCoords[0].x = tp.topLeft.x;
            texCoords[1].y = texCoords[3].y = tp.topLeft.y;
            texCoords[3].x = texCoords[2].x = tp.bottomRight.x;
            texCoords[2].y = texCoords[0].y = tp.bottomRight.y;

            if(mustSubdivide)
            {
                /*
                 * Need to swap indices around into fans set the position
                 * of the division vertices, interpolate texcoords and
                 * color.
                 */

                Vector3f origPosCoords[4];   std::memcpy(origPosCoords,   p.rvertices, sizeof(Vector3f) * 4);
                Vector2f origTexCoords[4];   std::memcpy(origTexCoords,   texCoords,   sizeof(Vector2f) * 4);
                Vector4f origColorCoords[4]; std::memcpy(origColorCoords, colorCoords, sizeof(Vector4f) * 4);

                R_DivVerts(posCoords, origPosCoords, leftSection, rightSection);
                R_DivTexCoords(texCoords, origTexCoords, leftSection, rightSection);
                R_DivVertColors(colorCoords, origColorCoords, leftSection, rightSection);
            }
            else
            {
                std::memcpy(posCoords, p.rvertices, sizeof(Vector3f) * p.numVertices);
            }
        }
        else
        {
            // It's a flat.
            float const width  = p.bottomRight->x - p.topLeft->x;
            float const height = p.bottomRight->y - p.topLeft->y;

            for(uint i = 0; i < p.numVertices; ++i)
            {
                texCoords[i].x = ((p.bottomRight->x - p.rvertices[i].x) / width  * tp.topLeft.x)
                               + ((p.rvertices[i].x - p.topLeft->x)     / width  * tp.bottomRight.x);

                texCoords[i].y = ((p.bottomRight->y - p.rvertices[i].y) / height * tp.topLeft.y)
                               + ((p.rvertices[i].y - p.topLeft->y)     / height * tp.bottomRight.y);
            }

            std::memcpy(posCoords, p.rvertices, sizeof(Vector3f) * p.numVertices);
        }

        DrawListSpec listSpec;
        listSpec.group = LightGeom;
        listSpec.texunits[TU_PRIMARY] =
            GLTextureUnit(tp.texture, gl::ClampToEdge, gl::ClampToEdge);

        DrawList &lightList = rendSys.drawLists().find(listSpec);

        if(mustSubdivide)
        {
            WallEdgeSection const &leftSection  = *p.wall.leftEdge;
            WallEdgeSection const &rightSection = *p.wall.rightEdge;

            {
                WorldVBuf::Index vertCount = 3 + rightSection.divisionCount();
                WorldVBuf::Index *indices  = rendSys.indicePool().alloc(vertCount);
                vbuf.reserveElements(vertCount, indices);
                vbuf.setVertices(vertCount, indices,
                                 posCoords   + 3 + leftSection.divisionCount(),
                                 colorCoords + 3 + leftSection.divisionCount(),
                                 texCoords   + 3 + leftSection.divisionCount());

                lightList.write(gl::TriangleFan, vertCount, indices);

                rendSys.indicePool().release(indices);
            }
            {
                WorldVBuf::Index vertCount = 3 + leftSection.divisionCount();
                WorldVBuf::Index *indices  = rendSys.indicePool().alloc(vertCount);
                vbuf.reserveElements(vertCount, indices);
                vbuf.setVertices(vertCount, indices,
                                 posCoords, colorCoords, texCoords);

                lightList.write(gl::TriangleFan, vertCount, indices);

                rendSys.indicePool().release(indices);
            }
        }
        else
        {
            WorldVBuf::Index vertCount = p.numVertices;
            WorldVBuf::Index *indices  = rendSys.indicePool().alloc(vertCount);
            vbuf.reserveElements(vertCount, indices);
            vbuf.setVertices(vertCount, indices,
                             posCoords, colorCoords, texCoords);

            lightList.write(p.isWall? gl::TriangleStrip : gl::TriangleFan,
                            vertCount, indices);

            rendSys.indicePool().release(indices);
        }

        rendSys.posPool().release(posCoords);
        rendSys.texPool().release(texCoords);
        rendSys.colorPool().release(colorCoords);
    }
    p.lastIdx++;
}

/// Generates a new primitive for each light projection.
static int drawDynlightWorker(TexProjection const *tp, void *context)
{
    drawDynlight(*tp, *static_cast<renderlightprojectionparams_t *>(context));
    return 0; // Continue iteration.
}

uint Rend_RenderLightProjections(uint listIdx, renderlightprojectionparams_t &p)
{
    uint numRendered = p.lastIdx;

    Rend_IterateProjectionList(listIdx, drawDynlightWorker, (void *)&p);

    numRendered = p.lastIdx - numRendered;
    if(Rend_IsMTexLights())
        numRendered -= 1;

    return numRendered;
}
