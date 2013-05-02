/** @file render/rend_dynlight.cpp
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
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"

#include "render/rend_dynlight.h"

static void drawDynlight(dynlight_t const &dyn, renderlightprojectionparams_t &parm)
{
    // If multitexturing is in use we skip the first.
    if(!(RL_IsMTexLights() && parm.lastIdx == 0))
    {
        // Allocate enough for the divisions too.
        rvertex_t *rvertices = R_AllocRendVertices(parm.realNumVertices);
        rtexcoord_t *rtexcoords = R_AllocRendTexCoords(parm.realNumVertices);
        ColorRawf *rcolors = R_AllocRendColors(parm.realNumVertices);
        bool const mustSubdivide = (parm.isWall && (parm.wall.leftEdge->divisionCount() || parm.wall.rightEdge->divisionCount() ));

        for(uint i = 0; i < parm.numVertices; ++i)
        {
            ColorRawf *col = &rcolors[i];
            for(uint c = 0; c < 4; ++c)
                col->rgba[c] = dyn.color.rgba[c];
        }

        if(parm.isWall)
        {
            SectionEdge const &leftEdge = *parm.wall.leftEdge;
            SectionEdge const &rightEdge = *parm.wall.rightEdge;

            rtexcoords[1].st[0] = rtexcoords[0].st[0] = dyn.s[0];
            rtexcoords[1].st[1] = rtexcoords[3].st[1] = dyn.t[0];
            rtexcoords[3].st[0] = rtexcoords[2].st[0] = dyn.s[1];
            rtexcoords[2].st[1] = rtexcoords[0].st[1] = dyn.t[1];

            if(mustSubdivide)
            {
                /*
                 * Need to swap indices around into fans set the position
                 * of the division vertices, interpolate texcoords and
                 * color.
                 */

                rvertex_t origVerts[4]; std::memcpy(origVerts, parm.rvertices, sizeof(rvertex_t) * 4);
                rtexcoord_t origTexCoords[4]; std::memcpy(origTexCoords, rtexcoords, sizeof(rtexcoord_t) * 4);
                ColorRawf origColors[4]; std::memcpy(origColors, rcolors, sizeof(ColorRawf) * 4);

                float bL = parm.rvertices[0].pos[VZ];
                float tL = parm.rvertices[1].pos[VZ];
                float bR = parm.rvertices[2].pos[VZ];
                float tR = parm.rvertices[3].pos[VZ];

                R_DivVerts(rvertices, origVerts, leftEdge, rightEdge);
                R_DivTexCoords(rtexcoords, origTexCoords, leftEdge, rightEdge, bL, tL, bR, tR);
                R_DivVertColors(rcolors, origColors, leftEdge, rightEdge, bL, tL, bR, tR);
            }
            else
            {
                std::memcpy(rvertices, parm.rvertices, sizeof(rvertex_t) * parm.numVertices);
            }
        }
        else
        {
            // It's a flat.
            float const width  = parm.texBR->x - parm.texTL->x;
            float const height = parm.texBR->y - parm.texTL->y;

            for(uint i = 0; i < parm.numVertices; ++i)
            {
                rtexcoords[i].st[0] = ((parm.texBR->x - parm.rvertices[i].pos[VX]) / width * dyn.s[0]) +
                    ((parm.rvertices[i].pos[VX] - parm.texTL->x) / width * dyn.s[1]);

                rtexcoords[i].st[1] = ((parm.texBR->y - parm.rvertices[i].pos[VY]) / height * dyn.t[0]) +
                    ((parm.rvertices[i].pos[VY] - parm.texTL->y) / height * dyn.t[1]);
            }

            std::memcpy(rvertices, parm.rvertices, sizeof(rvertex_t) * parm.numVertices);
        }

        RL_LoadDefaultRtus();
        RL_Rtu_SetTextureUnmanaged(RTU_PRIMARY, dyn.texture,
                                   GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        if(mustSubdivide)
        {
            SectionEdge const &leftEdge = *parm.wall.leftEdge;
            SectionEdge const &rightEdge = *parm.wall.rightEdge;

            RL_AddPolyWithCoords(PT_FAN, RPF_DEFAULT|RPF_LIGHT,
                                 3 + rightEdge.divisionCount(),
                                 rvertices  + 3 + leftEdge.divisionCount(),
                                 rcolors    + 3 + leftEdge.divisionCount(),
                                 rtexcoords + 3 + leftEdge.divisionCount(), 0);

            RL_AddPolyWithCoords(PT_FAN, RPF_DEFAULT|RPF_LIGHT,
                                 3 + leftEdge.divisionCount(),
                                 rvertices, rcolors, rtexcoords, 0);
        }
        else
        {
            RL_AddPolyWithCoords(parm.isWall? PT_TRIANGLE_STRIP : PT_FAN, RPF_DEFAULT|RPF_LIGHT,
                                 parm.numVertices, rvertices, rcolors, rtexcoords, 0);
        }

        R_FreeRendVertices(rvertices);
        R_FreeRendTexCoords(rtexcoords);
        R_FreeRendColors(rcolors);
    }
    parm.lastIdx++;
}

/// Generates a new primitive for each light projection.
static int drawDynlightWorker(dynlight_t const *dyn, void *parameters)
{
    renderlightprojectionparams_t *p = (renderlightprojectionparams_t *)parameters;
    drawDynlight(*dyn, *p);
    return 0; // Continue iteration.
}

uint Rend_RenderLightProjections(uint listIdx, renderlightprojectionparams_t &p)
{
    uint numRendered = p.lastIdx;

    LO_IterateProjections(listIdx, drawDynlightWorker, (void *)&p);

    numRendered = p.lastIdx - numRendered;
    if(RL_IsMTexLights())
        numRendered -= 1;
    return numRendered;
}
