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

static boolean shadowPlaneIterator(sector_t* sector, void* paramaters)
{
    plane_t** highest = (plane_t**)paramaters;
    plane_t* compare = sector->SP_plane(PLN_FLOOR);
    if(compare->visHeight > (*highest)->visHeight)
        *highest = compare;
    return true; // Continue iteration.
}

static plane_t* findShadowPlane(mobj_t* mo)
{
    plane_t* plane = mo->subsector->sector->SP_plane(PLN_FLOOR);
    P_MobjSectorsIterator(mo, shadowPlaneIterator, (void*)&plane);
    return plane;
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

    mobjOrigin[VX] = mo->pos[VX];
    mobjOrigin[VY] = mo->pos[VY];
    mobjOrigin[VZ] = mo->pos[VZ];

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

    // Apply a Short Range Visual Offset?
    if(useSRVO && mo->state && mo->tics >= 0)
    {
        float mul = mo->tics / (float) mo->state->tics;
        mobjOrigin[VX] += mo->srvo[VX] * mul;
        mobjOrigin[VY] += mo->srvo[VY] * mul;
        mobjOrigin[VZ] += mo->srvo[VZ] * mul;
    }

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
    if(shadowMaxDistance > 0 && distanceFromViewer > 3 * shadowMaxDistance / 4)
    {
        shadowStrength *= (shadowMaxDistance - distanceFromViewer) / (shadowMaxDistance / 4);
    }

    // Figure out the visible floor height...
    plane = findShadowPlane(mo);

    // Glowing planes inversely diminish shadow strength.
    shadowStrength *= (1 - R_GlowStrength(plane));

    // Would this shadow be seen?
    if(!(shadowStrength >= SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)) return;

    // Do not draw shadows above the shadow caster.
    if(plane->visHeight >= moz + mo->height) return; 

    // View height might prevent us from seeing the shadow.
    if(vy < plane->visHeight) return;

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

boolean Rend_MobjShadowsEnabled(void)
{
    return (useShadows && !levelFullBright);
}

void Rend_RenderMobjShadows(void)
{
    const sector_t* sec;
    mobj_t* mo;
    uint i;

    if(!Rend_MobjShadowsEnabled()) return;

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
