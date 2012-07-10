/**\file rend_dynlight.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"

/// Generates a new primitive for each light projection.
int RIT_RenderLightProjectionIterator(const dynlight_t* dyn, void* paramaters)
{
    renderlightprojectionparams_t* p = (renderlightprojectionparams_t*)paramaters;
    // If multitexturing is in use we skip the first.
    if(!(RL_IsMTexLights() && p->lastIdx == 0))
    {
        rvertex_t* rvertices;
        rtexcoord_t* rtexcoords;
        ColorRawf* rcolors;
        uint i, c;

        // Allocate enough for the divisions too.
        rvertices = R_AllocRendVertices(p->realNumVertices);
        rtexcoords = R_AllocRendTexCoords(p->realNumVertices);
        rcolors = R_AllocRendColors(p->realNumVertices);

        for(i = 0; i < p->numVertices; ++i)
        {
            ColorRawf* col = &rcolors[i];
            for(c = 0; c < 4; ++c)
                col->rgba[c] = dyn->color.rgba[c];
        }

        if(p->isWall)
        {
            rtexcoords[1].st[0] = rtexcoords[0].st[0] = dyn->s[0];
            rtexcoords[1].st[1] = rtexcoords[3].st[1] = dyn->t[0];
            rtexcoords[3].st[0] = rtexcoords[2].st[0] = dyn->s[1];
            rtexcoords[2].st[1] = rtexcoords[0].st[1] = dyn->t[1];

            if(p->wall.left.divCount || p->wall.right.divCount)
            {
                // We need to subdivide the projection quad.
                float bL, tL, bR, tR;
                rvertex_t origVerts[4];
                ColorRawf origColors[4];
                rtexcoord_t origTexCoords[4];

                /**
                 * Need to swap indices around into fans set the position
                 * of the division vertices, interpolate texcoords and
                 * color.
                 */

                memcpy(origVerts, p->rvertices, sizeof(rvertex_t) * 4);
                memcpy(origTexCoords, rtexcoords, sizeof(rtexcoord_t) * 4);
                memcpy(origColors, rcolors, sizeof(ColorRawf) * 4);

                bL = p->rvertices[0].pos[VZ];
                tL = p->rvertices[1].pos[VZ];
                bR = p->rvertices[2].pos[VZ];
                tR = p->rvertices[3].pos[VZ];

                R_DivVerts(rvertices, origVerts, p->wall.left.firstDiv, p->wall.left.divCount, p->wall.right.firstDiv, p->wall.right.divCount);
                R_DivTexCoords(rtexcoords, origTexCoords, p->wall.left.firstDiv, p->wall.left.divCount, p->wall.right.firstDiv, p->wall.right.divCount, bL, tL, bR, tR);
                R_DivVertColors(rcolors, origColors, p->wall.left.firstDiv, p->wall.left.divCount, p->wall.right.firstDiv, p->wall.right.divCount, bL, tL, bR, tR);
            }
            else
            {
                memcpy(rvertices, p->rvertices, sizeof(rvertex_t) * p->numVertices);
            }
        }
        else
        {
            // It's a flat.
            float width, height;

            width  = p->texBR[VX] - p->texTL[VX];
            height = p->texBR[VY] - p->texTL[VY];

            for(i = 0; i < p->numVertices; ++i)
            {
                rtexcoords[i].st[0] = ((p->texBR[VX] - p->rvertices[i].pos[VX]) / width * dyn->s[0]) +
                    ((p->rvertices[i].pos[VX] - p->texTL[VX]) / width * dyn->s[1]);

                rtexcoords[i].st[1] = ((p->texBR[VY] - p->rvertices[i].pos[VY]) / height * dyn->t[0]) +
                    ((p->rvertices[i].pos[VY] - p->texTL[VY]) / height * dyn->t[1]);
            }

            memcpy(rvertices, p->rvertices, sizeof(rvertex_t) * p->numVertices);
        }

        RL_LoadDefaultRtus();
        RL_Rtu_SetTextureUnmanaged(RTU_PRIMARY, dyn->texture);

        if(p->isWall && (p->wall.left.divCount || p->wall.right.divCount))
        {
            RL_AddPolyWithCoords(PT_FAN, RPF_DEFAULT|RPF_LIGHT,
                3 + p->wall.right.divCount, rvertices + 3 + p->wall.left.divCount,
                rcolors + 3 + p->wall.left.divCount, rtexcoords + 3 + p->wall.left.divCount, NULL);
            RL_AddPolyWithCoords(PT_FAN, RPF_DEFAULT|RPF_LIGHT,
                3 + p->wall.left.divCount, rvertices, rcolors, rtexcoords, NULL);
        }
        else
        {
            RL_AddPolyWithCoords(p->isWall? PT_TRIANGLE_STRIP : PT_FAN, RPF_DEFAULT|RPF_LIGHT,
                p->numVertices, rvertices, rcolors, rtexcoords, NULL);
        }

        R_FreeRendVertices(rvertices);
        R_FreeRendTexCoords(rtexcoords);
        R_FreeRendColors(rcolors);
    }
    p->lastIdx++;

    return 0; // Continue iteration.
}

uint Rend_RenderLightProjections(uint listIdx, renderlightprojectionparams_t* p)
{
    uint numRendered = p->lastIdx;
    assert(p);

    LO_IterateProjections2(listIdx, RIT_RenderLightProjectionIterator, (void*)p);

    numRendered = p->lastIdx - numRendered;
    if(RL_IsMTexLights())
        numRendered -= 1;
    return numRendered;
}
