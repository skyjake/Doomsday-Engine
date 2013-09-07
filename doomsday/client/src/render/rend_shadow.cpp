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

#include <de/Vector>

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
    Vector3f vertices[4];
    Vector4f colors[4];
    Vector2f texCoords[4];
} shadowprim_t;

/// This global shadow primitive is used to avoid repeated local
/// instantiation in drawShadowPrimitive()
static shadowprim_t rshadow, *rs = &rshadow;

bool Rend_MobjShadowsEnabled()
{
    return (useShadows && !levelFullBright);
}

static void drawShadowPrimitive(Vector3f const &pos, coord_t radius, float alpha)
{
    alpha = de::clamp(0.f, alpha, 1.f);
    if(alpha <= 0) return;

    radius = de::min(radius, (coord_t) shadowMaxRadius);
    if(radius <= 0) return;

    rs->vertices[0] = Vector3f(pos) + Vector3f(-radius,  radius, SHADOW_ZOFFSET);
    rs->colors[0].w = alpha;

    rs->vertices[1] = Vector3f(pos) + Vector3f( radius,  radius, SHADOW_ZOFFSET);
    rs->colors[0].w = alpha;

    rs->vertices[2] = Vector3f(pos) + Vector3f( radius, -radius, SHADOW_ZOFFSET);
    rs->colors[2].w = alpha;

    rs->vertices[3] = Vector3f(pos) + Vector3f(-radius, -radius, SHADOW_ZOFFSET);
    rs->colors[3].w = alpha;

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

    float shadowStrength = Mobj_ShadowStrength(mo) * shadowFactor;
    if(usingFog) shadowStrength /= 2;
    if(shadowStrength <= 0) return;

    coord_t shadowRadius = Mobj_VisualRadius(mo);
    if(shadowRadius <= 0) return;

    // Check the height.
    coord_t moz = mo->origin[VZ] - mo->floorClip;
    if(mo->ddFlags & DDMF_BOB)
        moz -= Mobj_BobOffset(mo);

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
    shadowStrength *= Rend_ShadowAttenuationFactor(distanceFromViewer);

    // Figure out the visible floor height...
    Plane *plane = Mobj_ShadowPlane(mo);
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
    drawShadowPrimitive(Vector3f(mobjOrigin[VX], mobjOrigin[VY], mobjOrigin[VZ]),
                        shadowRadius, shadowStrength);
}

static void initShadowPrimitive()
{
    Vector4f black(0, 0, 0, 0);

    rs->texCoords[0] = Vector2f(0, 1);
    rs->colors[0]    = black;

    rs->texCoords[1] = Vector2f(1, 1);
    rs->colors[1]    = black;

    rs->texCoords[2] = Vector2f(1, 0);
    rs->colors[2]    = black;

    rs->texCoords[3] = Vector2f(0, 0);
    rs->colors[3]    = black;
}

void Rend_RenderMobjShadows()
{
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

    foreach(Sector *sector, App_World().map().sectors())
    {
        // We are only interested in those mobjs within sectors marked as
        // 'visible' for the current render frame (viewer dependent).
        if(!sector->isVisible()) continue;

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
static void drawShadow(TexProjection const &tp, rendershadowprojectionparams_t &parm)
{
    // Allocate enough for the divisions too.
    Vector3f *rvertices  = R_AllocRendVertices(parm.realNumVertices);
    Vector2f *rtexcoords = R_AllocRendTexCoords(parm.realNumVertices);
    Vector4f *rcolors    = R_AllocRendColors(parm.realNumVertices);
    bool const mustSubdivide = (parm.isWall && (parm.wall.leftEdge->divisionCount() || parm.wall.rightEdge->divisionCount() ));

    for(uint i = 0; i < parm.numVertices; ++i)
    {
        rcolors[i] = tp.color;
    }

    if(parm.isWall)
    {
        WallEdge const &leftEdge  = *parm.wall.leftEdge;
        WallEdge const &rightEdge = *parm.wall.rightEdge;

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

            Vector3f origVerts[4]; std::memcpy(origVerts, parm.rvertices, sizeof(Vector3f) * 4);
            Vector2f origTexCoords[4]; std::memcpy(origTexCoords, rtexcoords, sizeof(Vector2f) * 4);
            Vector4f origColors[4]; std::memcpy(origColors, rcolors, sizeof(Vector4f) * 4);

            R_DivVerts(rvertices, origVerts, leftEdge, rightEdge);
            R_DivTexCoords(rtexcoords, origTexCoords, leftEdge, rightEdge);
            R_DivVertColors(rcolors, origColors, leftEdge, rightEdge);
        }
        else
        {
            std::memcpy(rvertices, parm.rvertices, sizeof(Vector3f) * parm.numVertices);
        }
    }
    else
    {
        // It's a flat.
        float const width  = parm.bottomRight->x - parm.topLeft->x;
        float const height = parm.bottomRight->y - parm.topLeft->y;

        for(uint i = 0; i < parm.numVertices; ++i)
        {
            rtexcoords[i].x = ((parm.bottomRight->x - parm.rvertices[i].x) / width * tp.topLeft.x) +
                ((parm.rvertices[i].x - parm.topLeft->x) / width * tp.bottomRight.x);

            rtexcoords[i].y = ((parm.bottomRight->y - parm.rvertices[i].y) / height * tp.topLeft.y) +
                ((parm.rvertices[i].y - parm.topLeft->y) / height * tp.bottomRight.y);
        }

        std::memcpy(rvertices, parm.rvertices, sizeof(Vector3f) * parm.numVertices);
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

static int drawShadowWorker(TexProjection const *tp, void *parameters)
{
    rendershadowprojectionparams_t *p = (rendershadowprojectionparams_t *)parameters;
    drawShadow(*tp, *p);
    return 0; // Continue iteration.
}

void Rend_RenderShadowProjections(uint listIdx, rendershadowprojectionparams_t &p)
{
    // Configure the render list primitive writer's texture unit state now.
    RL_LoadDefaultRtus();
    RL_Rtu_SetTextureUnmanaged(RTU_PRIMARY, GL_PrepareLSTexture(LST_DYNAMIC),
                               GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    // Write shadows to the render lists.
    Rend_IterateProjectionList(listIdx, drawShadowWorker, (void *)&p);
}
