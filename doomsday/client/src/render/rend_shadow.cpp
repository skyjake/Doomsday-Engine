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
#include "ConvexSubspace"
#include "DrawLists"
#include "MaterialSnapshot"
#include "WallEdge"
#include "gl/gl_texmanager.h"
#include <de/Vector>

using namespace de;

/**
 * Generates a new primitive for the shadow projection.
 *
 * @param drawListSpec  Draw list specififcation for the projected geometry.
 * @param tp            The projected texture.
 * @param parm          Shadow drawer parameters.
 */
static void drawShadow(DrawList::Spec &drawListSpec, TexProjection const &tp,
    rendershadowprojectionparams_t &p)
{
    WorldVBuf &vbuf = ClientApp::renderSystem().worldVBuf();

    if(p.leftSection) // A wall.
    {
        WallEdgeSection const &leftSection  = *p.leftSection;
        WallEdgeSection const &rightSection = *p.rightSection;
        bool const mustSubdivide            = (leftSection.divisionCount() || rightSection.divisionCount());

        if(mustSubdivide) // Draw as two triangle fans.
        {
            WorldVBuf::Index const rightFanSize = 3 + rightSection.divisionCount();
            WorldVBuf::Index const leftFanSize  = 3 + leftSection.divisionCount();

            Vector2f quadCoords[4] = {
                Vector2f(tp.topLeft.x,     tp.bottomRight.y),
                Vector2f(tp.topLeft.x,     tp.topLeft.y    ),
                Vector2f(tp.bottomRight.x, tp.bottomRight.y),
                Vector2f(tp.bottomRight.x, tp.topLeft.y    )
            };

            Shard::Geom *shard = new Shard::Geom(drawListSpec);
            shard->indices.resize(leftFanSize + rightFanSize);

            vbuf.reserveElements(leftFanSize + rightFanSize, shard->indices);
            Rend_DivPosCoords(shard->indices.data(), p.posCoords, leftSection, rightSection);
            Rend_DivTexCoords(shard->indices.data(), quadCoords, leftSection, rightSection,
                              WorldVBuf::PrimaryTex);

            for(WorldVBuf::Index i = 0; i < leftFanSize + rightFanSize; ++i)
            {
                WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                //vertex.pos  = vbuf[p.indices[i]].pos;
                vertex.rgba = tp.color;
            }

            Shard::Geom::Primitive leftFan;
            leftFan.type             = gl::TriangleFan;
            leftFan.vertCount        = leftFanSize;
            leftFan.indices          = shard->indices.data();
            leftFan.texScale         = Vector2f(1, 1);
            leftFan.texOffset        = Vector2f(0, 0);
            leftFan.detailTexScale   = Vector2f(1, 1);
            leftFan.detailTexOffset  = Vector2f(0, 0);
            shard->primitives.append(leftFan);

            Shard::Geom::Primitive rightFan;
            rightFan.type            = gl::TriangleFan;
            rightFan.vertCount       = rightFanSize;
            rightFan.indices         = shard->indices.data() + leftFan.vertCount;
            rightFan.texScale        = Vector2f(1, 1);
            rightFan.texOffset       = Vector2f(0, 0);
            rightFan.detailTexScale  = Vector2f(1, 1);
            rightFan.detailTexOffset = Vector2f(0, 0);
            shard->primitives.append(rightFan);

            p.subspace->shards() << shard;
        }
        else
        {
            WorldVBuf::Index vertCount = p.vertCount;

            Shard::Geom *shard = new Shard::Geom(drawListSpec);
            shard->indices.resize(vertCount);

            vbuf.reserveElements(vertCount, shard->indices);
            for(WorldVBuf::Index i = 0; i < vertCount; ++i)
            {
                WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                vertex.pos  = p.posCoords[i];//vbuf[p.indices[i]].pos;
                vertex.rgba = tp.color;
            }

            vbuf[shard->indices[1]].texCoord[WorldVBuf::PrimaryTex].x =
            vbuf[shard->indices[0]].texCoord[WorldVBuf::PrimaryTex].x = tp.topLeft.x;

            vbuf[shard->indices[1]].texCoord[WorldVBuf::PrimaryTex].y =
            vbuf[shard->indices[3]].texCoord[WorldVBuf::PrimaryTex].y = tp.topLeft.y;

            vbuf[shard->indices[3]].texCoord[WorldVBuf::PrimaryTex].x =
            vbuf[shard->indices[2]].texCoord[WorldVBuf::PrimaryTex].x = tp.bottomRight.x;

            vbuf[shard->indices[2]].texCoord[WorldVBuf::PrimaryTex].y =
            vbuf[shard->indices[0]].texCoord[WorldVBuf::PrimaryTex].y = tp.bottomRight.y;

            Shard::Geom::Primitive prim;
            prim.type            = gl::TriangleStrip;
            prim.vertCount       = vertCount;
            prim.indices         = shard->indices.data();
            prim.texScale        = Vector2f(1, 1);
            prim.texOffset       = Vector2f(0, 0);
            prim.detailTexScale  = Vector2f(1, 1);
            prim.detailTexOffset = Vector2f(0, 0);
            shard->primitives.append(prim);

            p.subspace->shards() << shard;
        }
    }
    else // A flat.
    {
        Vector2f const pDimensions = p.bottomRight->xy() - p.topLeft->xy();

        WorldVBuf::Index vertCount = p.vertCount;

        Shard::Geom *shard = new Shard::Geom(drawListSpec);
        shard->indices.resize(vertCount);

        vbuf.reserveElements(vertCount, shard->indices);
        for(WorldVBuf::Index i = 0; i < vertCount; ++i)
        {
            WorldVBuf::Type &vertex = vbuf[shard->indices[i]];

            vertex.pos  = vbuf[p.indices[i]].pos;
            vertex.rgba =    tp.color;

            vertex.texCoord[WorldVBuf::PrimaryTex] =
                Vector2f(((p.bottomRight->x - vertex.pos.x) / pDimensions.x * tp.topLeft.x) +
                         ((vertex.pos.x     - p.topLeft->x) / pDimensions.x * tp.bottomRight.x)
                         ,
                         ((p.bottomRight->y - vertex.pos.y) / pDimensions.y * tp.topLeft.y) +
                         ((vertex.pos.y     - p.topLeft->y) / pDimensions.y * tp.bottomRight.y));
        }

        Shard::Geom::Primitive prim;
        prim.type            = gl::TriangleFan;
        prim.vertCount       = vertCount;
        prim.indices         = shard->indices.data();
        prim.texScale        = Vector2f(1, 1);
        prim.texOffset       = Vector2f(0, 0);
        prim.detailTexScale  = Vector2f(1, 1);
        prim.detailTexOffset = Vector2f(0, 0);
        shard->primitives.append(prim);

        p.subspace->shards() << shard;
    }
}

struct drawshadowworker_params_t
{
    DrawListSpec drawListSpec;
    rendershadowprojectionparams_t *drawShadowParms;
};

static int drawShadowWorker(TexProjection const *tp, void *context)
{
    drawshadowworker_params_t &p = *static_cast<drawshadowworker_params_t *>(context);
    drawShadow(p.drawListSpec, *tp, *p.drawShadowParms);
    return 0; // Continue iteration.
}

void Rend_DrawProjectedShadows(uint listIdx, rendershadowprojectionparams_t &p)
{
    // Write shadows to the render lists.
    drawshadowworker_params_t parm;
    parm.drawListSpec.group                = ShadowGeom;
    parm.drawListSpec.texunits[TU_PRIMARY] = GLTextureUnit(GL_PrepareLSTexture(LST_DYNAMIC), gl::ClampToEdge, gl::ClampToEdge);
    parm.drawShadowParms = &p;

    Rend_IterateProjectionList(listIdx, drawShadowWorker, &parm);
}
