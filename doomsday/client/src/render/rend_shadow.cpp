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
#include "de_graphics.h"
#include "de_play.h"
#include "de_render.h"
#include "de_system.h"

#include "world/map.h"
#include "MaterialSnapshot"
#include "WallEdge"

#include "render/rend_shadow.h"

using namespace de;

typedef struct {
    rvertex_t vertices[4];
    ColorRawf colors[4];
    rtexcoord_t texCoords[4];
} shadowprim_t;

/// This global shadow primitive is used to avoid repeated local
/// instantiation in drawShadowPrimitive()
static shadowprim_t rshadow, *rs = &rshadow;

bool Rend_MobjShadowsEnabled()
{
    return (useShadows && !levelFullBright);
}

static void drawShadowPrimitive(coord_t const pos[3], coord_t radius, float alpha)
{
    alpha = de::clamp(0.f, alpha, 1.f);
    if(alpha <= 0) return;

    radius = de::min(radius, (coord_t) shadowMaxRadius);
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

    RL_AddPolyWithCoords(PT_FAN, RPF_DEFAULT|RPF_SHADOW, 4,
        rs->vertices, rs->colors, rs->texCoords, NULL);
}

/// @return  Current glow strength for the plane.
static float planeGlowStrength(Plane &plane)
{
    Surface const &surface = plane.surface();
    if(surface.hasMaterial() && glowFactor > .0001f)
    {
        if(surface.material().isDrawable() && !surface.hasSkyMaskedMaterial())
        {
            MaterialSnapshot const &ms = surface.material().prepare(Rend_MapSurfaceMaterialSpec());

            float glowStrength = ms.glowStrength() * glowFactor; // Global scale factor.
            return glowStrength;
        }
    }
    return 0;
}

static void processMobjShadow(mobj_t *mo)
{
    DENG_ASSERT(mo != 0);

    coord_t mobjOrigin[3];
    Mobj_OriginSmoothed(mo, mobjOrigin);

    // Is this too far?
    coord_t distanceFromViewer = 0;
    if(shadowMaxDistance > 0)
    {
        distanceFromViewer = Rend_PointDist2D(mobjOrigin);
        if(distanceFromViewer > shadowMaxDistance) return;
    }

    float shadowStrength = R_ShadowStrength(mo) * shadowFactor;
    if(usingFog) shadowStrength /= 2;
    if(shadowStrength <= 0) return;

    coord_t shadowRadius = R_VisualRadius(mo);
    if(shadowRadius <= 0) return;

    // Check the height.
    coord_t moz = mo->origin[VZ] - mo->floorClip;
    if(mo->ddFlags & DDMF_BOB)
        moz -= R_GetBobOffset(mo);

    coord_t heightFromSurface = moz - mo->floorZ;
    coord_t moh = mo->height;
    if(!moh) moh = 1;

    // Too far above or below the shadow plane?
    if(heightFromSurface > moh) return;
    if(moz + mo->height < mo->floorZ) return;

    // Calculate the final strength of the shadow's attribution to the surface.
    coord_t halfmoh = moh / 2;
    if(heightFromSurface > halfmoh)
    {
        shadowStrength *= 1 - (heightFromSurface - halfmoh) / (moh - halfmoh);
    }

    // Fade when nearing the maximum distance?
    shadowStrength *= R_ShadowAttenuationFactor(distanceFromViewer);

    // Figure out the visible floor height...
    Plane *plane = R_FindShadowPlane(mo);
    if(!plane) return;

    // Do not draw shadows above the shadow caster.
    if(plane->visHeight() >= moz + mo->height) return;

    // View height might prevent us from seeing the shadow.
    if(vOrigin[VY] < plane->visHeight()) return;

    // Glowing planes inversely diminish shadow strength.
    shadowStrength *= (1 - de::min(1.f, planeGlowStrength(*plane)));

    // Would this shadow be seen?
    if(!(shadowStrength >= SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)) return;

    mobjOrigin[VZ] = plane->visHeight();
    drawShadowPrimitive(mobjOrigin, shadowRadius, shadowStrength);
}

static void initShadowPrimitive()
{
#define SETCOLOR_BLACK(c) ((c).rgba[CR] = (c).rgba[CG] = (c).rgba[CB] = 0)

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

void Rend_RenderMobjShadows()
{
    DENG_ASSERT(theMap != 0);

    // Disabled. (Awaiting a heuristic analyser to enable it on selective mobjs.)
    /// @todo Re-enable mobj shadows.
    return;

    // Disabled?
    if(!Rend_MobjShadowsEnabled()) return;

    // Configure the render list primitive writer's texture unit state now.
    RL_LoadDefaultRtus();
    RL_Rtu_SetTextureUnmanaged(RTU_PRIMARY, GL_PrepareLSTexture(LST_DYNAMIC),
                               GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    // Initialize the invariant parts of our shadow primitive now.
    initShadowPrimitive();

    foreach(Sector *sector, theMap->sectors())
    {
        // We are only interested in those mobjs within sectors marked as
        // 'visible' for the current render frame (viewer dependent).
        if(!(sector->frameFlags() & SIF_VISIBLE)) continue;

        // Process all mobjs linked to this sector:
        for(mobj_t *mo = sector->firstMobj(); mo; mo = mo->sNext)
        {
            processMobjShadow(mo);
        }
    }
}

/**
 * Generates a new primitive for the shadow projection.
 */
static void drawShadow(shadowprojection_t const &sp, rendershadowprojectionparams_t &parm)
{
    static float const black[3] = { 0, 0, 0 };

    // Allocate enough for the divisions too.
    rvertex_t *rvertices = R_AllocRendVertices(parm.realNumVertices);
    rtexcoord_t *rtexcoords = R_AllocRendTexCoords(parm.realNumVertices);
    ColorRawf *rcolors = R_AllocRendColors(parm.realNumVertices);
    bool const mustSubdivide = (parm.isWall && (parm.wall.leftEdge->divisionCount() || parm.wall.rightEdge->divisionCount() ));

    for(uint i = 0; i < parm.numVertices; ++i)
    {
        ColorRawf *col = &rcolors[i];
        // Shadows are black.
        for(uint c = 0; c < 3; ++c)
            col->rgba[c] = black[c];

        // Blend factor.
        col->alpha = sp.alpha;
    }

    if(parm.isWall)
    {
        WallEdge const &leftEdge = *parm.wall.leftEdge;
        WallEdge const &rightEdge = *parm.wall.rightEdge;

        rtexcoords[1].st[0] = rtexcoords[0].st[0] = sp.s[0];
        rtexcoords[1].st[1] = rtexcoords[3].st[1] = sp.t[0];
        rtexcoords[3].st[0] = rtexcoords[2].st[0] = sp.s[1];
        rtexcoords[2].st[1] = rtexcoords[0].st[1] = sp.t[1];

        if(mustSubdivide)
        {
            // We need to subdivide the projection quad.

            /*
             * Need to swap indices around into fans set the position
             * of the division vertices, interpolate texcoords and
             * color.
             */

            rvertex_t origVerts[4]; std::memcpy(origVerts, parm.rvertices, sizeof(rvertex_t) * 4);
            rtexcoord_t origTexCoords[4]; std::memcpy(origTexCoords, rtexcoords, sizeof(rtexcoord_t) * 4);
            ColorRawf origColors[4]; std::memcpy(origColors, rcolors, sizeof(ColorRawf) * 4);

            R_DivVerts(rvertices, origVerts, leftEdge, rightEdge);
            R_DivTexCoords(rtexcoords, origTexCoords, leftEdge, rightEdge);
            R_DivVertColors(rcolors, origColors, leftEdge, rightEdge);
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
            rtexcoords[i].st[0] = ((parm.texBR->x - parm.rvertices[i].pos[VX]) / width * sp.s[0]) +
                ((parm.rvertices[i].pos[VX] - parm.texTL->x) / width * sp.s[1]);

            rtexcoords[i].st[1] = ((parm.texBR->y - parm.rvertices[i].pos[VY]) / height * sp.t[0]) +
                ((parm.rvertices[i].pos[VY] - parm.texTL->y) / height * sp.t[1]);
        }

        std::memcpy(rvertices, parm.rvertices, sizeof(rvertex_t) * parm.numVertices);
    }

    if(mustSubdivide)
    {
        WallEdge const &leftEdge = *parm.wall.leftEdge;
        WallEdge const &rightEdge = *parm.wall.rightEdge;

        RL_AddPolyWithCoords(PT_FAN, RPF_DEFAULT|RPF_SHADOW,
                             3 + rightEdge.divisionCount(),
                             rvertices  + 3 + leftEdge.divisionCount(),
                             rcolors    + 3 + leftEdge.divisionCount(),
                             rtexcoords + 3 + leftEdge.divisionCount(),
                             0);
        RL_AddPolyWithCoords(PT_FAN, RPF_DEFAULT|RPF_SHADOW,
                             3 + leftEdge.divisionCount(),
                             rvertices, rcolors, rtexcoords, 0);
    }
    else
    {
        RL_AddPolyWithCoords(parm.isWall? PT_TRIANGLE_STRIP : PT_FAN, RPF_DEFAULT|RPF_SHADOW,
                             parm.numVertices, rvertices, rcolors, rtexcoords, 0);
    }

    R_FreeRendVertices(rvertices);
    R_FreeRendTexCoords(rtexcoords);
    R_FreeRendColors(rcolors);
}

static int drawShadowWorker(shadowprojection_t const *sp, void *parameters)
{
    rendershadowprojectionparams_t *p = (rendershadowprojectionparams_t *)parameters;
    drawShadow(*sp, *p);
    return 0; // Continue iteration.
}

void Rend_RenderShadowProjections(uint listIdx, rendershadowprojectionparams_t &p)
{
    // Configure the render list primitive writer's texture unit state now.
    RL_LoadDefaultRtus();
    RL_Rtu_SetTextureUnmanaged(RTU_PRIMARY, GL_PrepareLSTexture(LST_DYNAMIC),
                               GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    // Write shadows to the render lists.
    R_IterateShadowProjections(listIdx, drawShadowWorker, (void *)&p);
}
