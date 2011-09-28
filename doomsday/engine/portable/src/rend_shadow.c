/**\file rend_shadow.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
#include "de_play.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_system.h"

typedef struct {
    rvertex_t vertices[4];
    rcolor_t colors[4];
    rtexcoord_t texCoords[4];
    rtexmapunit_t texUnits[NUM_TEXMAP_UNITS];
} shadowprim_t;

/// \optimize This global shadow primitive is used to avoid repeated local
/// instantiation in drawShadowPrimitive()
static shadowprim_t rshadow, *rs = &rshadow;

boolean Rend_MobjShadowsEnabled(void)
{
    return (useShadows && !levelFullBright);
}

static void drawShadowPrimitive(const vectorcomp_t pos[3], float radius, float alpha)
{
    alpha = MINMAX_OF(0, alpha, 1);
    if(alpha <= 0) return;

    radius = MIN_OF(radius, (float) shadowMaxRadius);
    if(radius <= 0) return;

    rs->vertices[0].pos[VX] = pos[VX] - radius;
    rs->vertices[0].pos[VY] = pos[VY] + radius;
    rs->vertices[0].pos[VZ] = pos[VZ] + SHADOW_ZOFFSET;
    rs->colors[0].alpha = alpha;

    rs->vertices[1].pos[VX] = pos[VX] + radius;
    rs->vertices[1].pos[VY] = pos[VY] + radius;
    rs->vertices[1].pos[VZ] = pos[VZ] + SHADOW_ZOFFSET;
    rs->colors[0].alpha = alpha;

    rs->vertices[2].pos[VX] = pos[VX] + radius;
    rs->vertices[2].pos[VY] = pos[VY] - radius;
    rs->vertices[2].pos[VZ] = pos[VZ] + SHADOW_ZOFFSET;
    rs->colors[2].alpha = alpha;

    rs->vertices[3].pos[VX] = pos[VX] - radius;
    rs->vertices[3].pos[VY] = pos[VY] - radius;
    rs->vertices[3].pos[VZ] = pos[VZ] + SHADOW_ZOFFSET;
    rs->colors[3].alpha = alpha;

    RL_AddPoly(PT_FAN, RPT_SHADOW, rs->vertices, rs->texCoords, NULL, NULL, rs->colors, 4, 0, 0, NULL, rs->texUnits);
}

static void processMobjShadow(mobj_t* mo)
{
    float moz, moh, halfmoh, heightFromSurface, distanceFromViewer = 0;
    float mobjOrigin[3], shadowRadius, shadowStrength;
    plane_t* plane;

    R_VisualOrigin(mo, mobjOrigin);

    // Is this too far?
    if(shadowMaxDistance > 0)
    {
        distanceFromViewer = Rend_PointDist2D(mobjOrigin);
        if(distanceFromViewer > shadowMaxDistance) return;
    }

    shadowStrength = R_ShadowStrength(mo) * shadowFactor;
    if(usingFog) shadowStrength /= 2;
    if(shadowStrength <= 0) return;

    shadowRadius = R_VisualRadius(mo);
    if(shadowRadius <= 0) return;

    // Check the height.
    moz = mo->pos[VZ] - mo->floorClip;
    if(mo->ddFlags & DDMF_BOB)
        moz -= R_GetBobOffset(mo);
    heightFromSurface = moz - mo->floorZ;
    moh = mo->height;
    if(!moh) moh = 1;

    // Too far above or below the shadow plane?
    if(heightFromSurface > moh) return;
    if(moz + mo->height < mo->floorZ) return;

    // Calculate the final strength of the shadow's attribution to the surface.
    halfmoh = moh / 2;
    if(heightFromSurface > halfmoh)
    {
        shadowStrength *= 1 - (heightFromSurface - halfmoh) / (moh - halfmoh);
    }

    // Fade when nearing the maximum distance?
    shadowStrength *= R_ShadowAttenuationFactor(distanceFromViewer);

    // Figure out the visible floor height...
    plane = R_FindShadowPlane(mo);
    if(!plane) return;

    // Do not draw shadows above the shadow caster.
    if(plane->visHeight >= moz + mo->height) return; 

    // View height might prevent us from seeing the shadow.
    if(vy < plane->visHeight) return;

    // Glowing planes inversely diminish shadow strength.
    shadowStrength *= (1 - R_GlowStrength(plane));

    // Would this shadow be seen?
    if(!(shadowStrength >= SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)) return;

    mobjOrigin[VZ] = plane->visHeight;
    drawShadowPrimitive(mobjOrigin, shadowRadius, shadowStrength);
}

static void initShadowPrimitive(void)
{
#define SETCOLOR_BLACK(c) ((c).rgba[CR] = (c).rgba[CG] = (c).rgba[CB] = 0)

    memset(rs->texUnits, 0, sizeof(rs->texUnits));
    rs->texUnits[TU_PRIMARY].tex = GL_PrepareLSTexture(LST_DYNAMIC);
    rs->texUnits[TU_PRIMARY].magMode = GL_LINEAR;
    rs->texUnits[TU_PRIMARY].blend = 1;

    rs->texCoords[0].st[0] = 0;
    rs->texCoords[0].st[1] = 1;
    SETCOLOR_BLACK(rs->colors[0]);

    rs->texCoords[1].st[0] = 1;
    rs->texCoords[1].st[1] = 1;
    SETCOLOR_BLACK(rs->colors[1]);

    rs->texCoords[2].st[0] = 1;
    rs->texCoords[2].st[1] = 0;
    SETCOLOR_BLACK(rs->colors[2]);

    rs->texCoords[3].st[0] = 0;
    rs->texCoords[3].st[1] = 0;
    SETCOLOR_BLACK(rs->colors[3]);

#undef SETCOLOR_BLACK
}

void Rend_RenderMobjShadows(void)
{
    const sector_t* sec;
    mobj_t* mo;
    uint i;

    // Disabled for now, awaiting a heuristic analyser to enable it on selective mobjs.
    return;

    // Initialize the invariant parts of our global shadow primitive now.
    initShadowPrimitive();

    // Process all sectors:
    for(i = 0; i < numSectors; ++i)
    {
        sec = sectors + i;

        // We are only interested in those mobjs within sectors marked as
        // 'visible' for the current render frame (viewer dependent).
        if(!(sec->frameFlags & SIF_VISIBLE)) continue;

        // Process all mobjs linked to this sector:
        for(mo = sec->mobjList; mo; mo = mo->sNext)
        {
            processMobjShadow(mo);
        }
    }
}

/// Generates a new primitive for each shadow projection.
int RIT_RenderShadowProjectionIterator(const shadowprojection_t* sp, void* paramaters)
{
    static const float black[3] = { 0, 0, 0 };
    rendershadowprojectionparams_t* p = (rendershadowprojectionparams_t*)paramaters;
    rtexmapunit_t rTU[NUM_TEXMAP_UNITS];
    rvertex_t* rvertices;
    rtexcoord_t* rtexcoords;
    rcolor_t* rcolors;
    uint i, c;

    memset(rTU, 0, sizeof(rTU));

    // Allocate enough for the divisions too.
    rvertices = R_AllocRendVertices(p->realNumVertices);
    rtexcoords = R_AllocRendTexCoords(p->realNumVertices);
    rcolors = R_AllocRendColors(p->realNumVertices);

    rTU[TU_PRIMARY].tex = GL_PrepareLSTexture(LST_DYNAMIC);
    rTU[TU_PRIMARY].magMode = GL_LINEAR;
    rTU[TU_PRIMARY].blend = 1;

    rTU[TU_PRIMARY_DETAIL].tex = 0;
    rTU[TU_INTER].tex = 0;
    rTU[TU_INTER_DETAIL].tex = 0;

    for(i = 0; i < p->numVertices; ++i)
    {
        rcolor_t* col = &rcolors[i];
        // Shadows are black.
        for(c = 0; c < 3; ++c) col->rgba[c] = black[c];
        // Blend factor.
        col->alpha = sp->alpha;
    }

    if(p->isWall)
    {
        rtexcoords[1].st[0] = rtexcoords[0].st[0] = sp->s[0];
        rtexcoords[1].st[1] = rtexcoords[3].st[1] = sp->t[0];
        rtexcoords[3].st[0] = rtexcoords[2].st[0] = sp->s[1];
        rtexcoords[2].st[1] = rtexcoords[0].st[1] = sp->t[1];

        if(p->divs)
        {
            // We need to subdivide the projection quad.
            float bL, tL, bR, tR;
            rvertex_t origVerts[4];
            rcolor_t origColors[4];
            rtexcoord_t origTexCoords[4];

            /**
             * Need to swap indices around into fans set the position
             * of the division vertices, interpolate texcoords and
             * color.
             */

            memcpy(origVerts, p->rvertices, sizeof(rvertex_t) * 4);
            memcpy(origTexCoords, rtexcoords, sizeof(rtexcoord_t) * 4);
            memcpy(origColors, rcolors, sizeof(rcolor_t) * 4);

            bL = p->rvertices[0].pos[VZ];
            tL = p->rvertices[1].pos[VZ];
            bR = p->rvertices[2].pos[VZ];
            tR = p->rvertices[3].pos[VZ];

            R_DivVerts(rvertices, origVerts, p->divs);
            R_DivTexCoords(rtexcoords, origTexCoords, p->divs, bL, tL, bR, tR);
            R_DivVertColors(rcolors, origColors, p->divs, bL, tL, bR, tR);
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
            rtexcoords[i].st[0] = ((p->texBR[VX] - p->rvertices[i].pos[VX]) / width * sp->s[0]) +
                ((p->rvertices[i].pos[VX] - p->texTL[VX]) / width * sp->s[1]);

            rtexcoords[i].st[1] = ((p->texBR[VY] - p->rvertices[i].pos[VY]) / height * sp->t[0]) +
                ((p->rvertices[i].pos[VY] - p->texTL[VY]) / height * sp->t[1]);
        }

        memcpy(rvertices, p->rvertices, sizeof(rvertex_t) * p->numVertices);
    }

    if(p->isWall && p->divs)
    {
        RL_AddPoly(PT_FAN, RPT_SHADOW,
                   rvertices + 3 + p->divs[0].num,
                   rtexcoords + 3 + p->divs[0].num, NULL, NULL,
                   rcolors + 3 + p->divs[0].num,
                   3 + p->divs[1].num, 0,
                   0, NULL, rTU);
        RL_AddPoly(PT_FAN, RPT_SHADOW,
                   rvertices, rtexcoords, NULL, NULL,
                   rcolors, 3 + p->divs[0].num, 0,
                   0, NULL, rTU);
    }
    else
    {
        RL_AddPoly(p->isWall? PT_TRIANGLE_STRIP : PT_FAN, RPT_SHADOW,
                   rvertices, rtexcoords, NULL, NULL,
                   rcolors, p->numVertices, 0,
                   0, NULL, rTU);
    }

    R_FreeRendVertices(rvertices);
    R_FreeRendTexCoords(rtexcoords);
    R_FreeRendColors(rcolors);

    return 0; // Continue iteration.
}

void Rend_RenderShadowProjections(uint listIdx, rendershadowprojectionparams_t* p)
{
    R_IterateShadowProjections2(listIdx, RIT_RenderShadowProjectionIterator, (void*)p);
}
