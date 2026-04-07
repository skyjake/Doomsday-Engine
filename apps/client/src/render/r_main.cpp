/** @file r_main.cpp
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_platform.h"

#include "dd_def.h"  // finesine
#include "clientapp.h"
#include "gl/gl_main.h"
#include "render/billboard.h"
#include "render/modelrenderer.h"
#include "render/r_main.h"
#include "render/rend_main.h"
#include "render/rend_model.h"
#include "render/rendersystem.h"
#include "render/vissprite.h"
#include "resource/clientresources.h"
#include "world/map.h"
#include "world/subsector.h"
#include "world/convexsubspace.h"
#include "world/p_players.h"

#include <doomsday/defs/sprite.h>
#include <doomsday/world/bspleaf.h>
#include <doomsday/world/materials.h>
#include <doomsday/res/sprites.h>
#include <de/legacy/vector1.h>
#include <de/glinfo.h>
#include <de/glstate.h>
#include <de/glframebuffer.h>

using namespace de;

int levelFullBright;
int weaponOffsetScaleY = 1000;
int psp3d;

float pspLightLevelMultiplier = 1;
float pspOffset[2];

/*
 * Console variables:
 */
float weaponFOVShift    = 45;
float weaponOffsetScale = 0.3183f;  // 1/Pi
dbyte weaponScaleMode    = SCALEMODE_SMART_STRETCH;

static const MaterialVariantSpec &pspriteMaterialSpec()
{
    return ClientApp::resources().materialSpec(PSpriteContext, 0, 1, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                 0, -2, 0, false, true, true, false);
}

static void setupPSpriteParams(rendpspriteparams_t &parm, const vispsprite_t &vs)
{
    static const int WEAPONTOP = 32;  /// @todo Currently hardcoded here and in the plugins.

    const float offScaleY = ::weaponOffsetScaleY / 1000.0f;

    DE_ASSERT(vs.psp);
    const ddpsprite_t &psp = *vs.psp;
    DE_ASSERT(psp.statePtr);
    const state_t &state = *psp.statePtr;

    const defn::Sprite::View spriteView = defn::Sprite(res::Sprites::get().sprite(state.sprite, state.frame)).view(0);

    // Lookup the Material for this Sprite and prepare the animator.
    MaterialAnimator &matAnimator = ClientMaterial::find(*spriteView.material)
            .getAnimator(pspriteMaterialSpec());
    matAnimator.prepare();

    const TextureVariant &tex             = *matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;
    const Vec2i &texOrigin             = tex.base().origin();
    const variantspecification_t &texSpec = tex.spec().variant;

    parm.pos[0] = psp.pos[0] + texOrigin.x + pspOffset[0] - texSpec.border;
    parm.pos[1] = WEAPONTOP + offScaleY * (psp.pos[1] - WEAPONTOP) + texOrigin.y
                + pspOffset[1] - texSpec.border;

    const Vec2ui dimensions = matAnimator.dimensions() + Vec2ui(texSpec.border, texSpec.border) * 2;
    parm.width  = dimensions.x;
    parm.height = dimensions.y;

    tex.glCoords(&parm.texOffset[0], &parm.texOffset[1]);

    parm.texFlip[0] = spriteView.mirrorX;
    parm.texFlip[1] = false;
    parm.mat        = &matAnimator.material();

    parm.ambientColor[3] = vs.alpha;

    if (vs.light.isFullBright)
    {
        parm.ambientColor[0] = parm.ambientColor[1] = parm.ambientColor[2] = 1;
        parm.vLightListIdx = 0;
    }
    else
    {
        DE_ASSERT(vs.bspLeaf);
#if 0
        const world::Map &map = ClientApp::world().map();
        if (useBias && map.hasLightGrid())
        {
            // Evaluate the position in the light grid.
            Vec4f color = map.lightGrid().evaluate(vs.origin);

            // Apply light range compression.
            for (int i = 0; i < 3; ++i)
            {
                color[i] += Rend_LightAdaptationDelta(color[i]);
            }

            V3f_Set(parm.ambientColor, color.x, color.y, color.z);
        }
        else
#endif
        {
            const auto &subsec   = vs.bspLeaf->subspace().subsector().as<Subsector>();
            const Vec4f color = subsec.lightSourceColorfIntensity();

            // No need for distance attentuation.
            float lightLevel = color.w;

            // Add extra light plus bonus.
            lightLevel += Rend_ExtraLightDelta();
            lightLevel *= pspLightLevelMultiplier;

            // The last step is to compress the resultant light value by
            // the global lighting function.
            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final ambientColor.
            for (int i = 0; i < 3; ++i)
            {
                parm.ambientColor[i] = lightLevel * color[i];
            }
        }
        Rend_ApplyTorchLight(parm.ambientColor, 0);

        parm.vLightListIdx =
                Rend_CollectAffectingLights(vs.origin, Vec3f(parm.ambientColor),
                                            vs.bspLeaf->subspacePtr());
    }
}

void Rend_Draw2DPlayerSprites()
{
    if (!viewPlayer) return;

    const ddplayer_t &ddpl = viewPlayer->publicData();

    // Cameramen have no HUD sprites.
    if (ddpl.flags & DDPF_CAMERA  ) return;
    if (ddpl.flags & DDPF_CHASECAM) return;

    if (fogParams.usingFog)
    {
        DGL_Enable(DGL_FOG);
    }

    // Draw HUD vissprites.
    for (const vispsprite_t &vs : visPSprites)
    {
        // We are only interested in sprites (models are handled elsewhere).
        if (vs.type != VPSPR_SPRITE) continue;  // No...

        // We require PSprite and State info.
        if (!vs.psp || !vs.psp->statePtr) continue;

        try
        {
            rendpspriteparams_t parm; setupPSpriteParams(parm, vs);
            Rend_DrawPSprite(parm);
        }
        catch (const Resources::MissingResourceManifestError &er)
        {
            // Log but otherwise ignore this error.
            const state_t &state = *vs.psp->statePtr;
            LOG_GL_WARNING("Drawing psprite '%i' frame '%i': %s")
                    << state.sprite << state.frame << er.asText();
        }
    }

    if (fogParams.usingFog)
    {
        DGL_Disable(DGL_FOG);
    }
}

static void setupModelParamsForVisPSprite(vissprite_t &vis, const vispsprite_t &spr)
{
    drawmodelparams_t *params = VS_MODEL(&vis);

    params->mf = spr.data.model.mf;
    params->nextMF = spr.data.model.nextMF;
    params->inter = spr.data.model.inter;
    params->alwaysInterpolate = false;
    params->id = spr.data.model.id;
    params->selector = spr.data.model.selector;
    params->flags = spr.data.model.flags;
    vis.pose.origin = spr.origin;
    vis.pose.srvo[0] = spr.data.model.visOff[0];
    vis.pose.srvo[1] = spr.data.model.visOff[1];
    vis.pose.srvo[2] = spr.data.model.visOff[2] - spr.data.model.floorClip;
    vis.pose.topZ = spr.data.model.topZ;
    vis.pose.distance = -10;
    vis.pose.yaw = spr.data.model.yaw;
    vis.pose.extraYawAngle = 0;
    vis.pose.yawAngleOffset = spr.data.model.yawAngleOffset;
    vis.pose.pitch = spr.data.model.pitch;
    vis.pose.extraPitchAngle = 0;
    vis.pose.pitchAngleOffset = spr.data.model.pitchAngleOffset;
    vis.pose.extraScale = 0;
    vis.pose.viewAligned = spr.data.model.viewAligned;
    vis.pose.mirrored = (mirrorHudModels? true : false);
    params->shineYawOffset = -vang;
    params->shinePitchOffset = vpitch + 90;
    params->shineTranslateWithViewerPos = false;
    params->shinepspriteCoordSpace = true;
    vis.light.ambientColor[3] = spr.alpha;

    if ((levelFullBright || spr.light.isFullBright) &&
       !spr.data.model.mf->testSubFlag(0, MFF_DIM))
    {
        vis.light.ambientColor[0] = vis.light.ambientColor[1] = vis.light.ambientColor[2] = 1;
        vis.light.vLightListIdx = 0;
    }
    else
    {
        vis.light.setupLighting(vis.pose.origin, vis.pose.distance, *spr.bspLeaf);
    }
}

void Rend_Draw3DPlayerSprites()
{
    GL_ProjectionMatrix(true /* fixed FOV for psprites */);
    Rend_ModelViewMatrix(false /* don't apply view angle rotation */);

    bool first = true;

    // Draw HUD vissprites.
    for (const vispsprite_t &spr : visPSprites)
    {
        // We are only interested in models (sprites are handled elsewhere).
        if (spr.type != VPSPR_MODEL &&
            spr.type != VPSPR_MODEL2) continue;

        if (first)
        {
            first = false;

            // Clear the depth buffer so models don't clip into walls.
            GLState::current().target().clear(GLFramebuffer::Depth);
        }

        if (spr.type == VPSPR_MODEL)
        {
            vissprite_t vs;
            setupModelParamsForVisPSprite(vs, spr);
            Rend_DrawModel(vs);
        }
        else
        {
            vispsprite_t lit = spr;
            /// @todo Apply the origin offset here and when rendering.
            lit.light.setupLighting(spr.origin + Vec3d(0, 0, -10), -10, *spr.bspLeaf);
            ClientApp::render().modelRenderer()
                    .render(lit, viewPlayer->publicData().mo);
        }
    }

    // Restore normal projection matrix.
    GL_ProjectionMatrix(false);
}

angle_t R_ViewPointToAngle(Vec2d point)
{
    const viewdata_t *viewData = &viewPlayer->viewport();
    point -= Vec2d(viewData->current.origin);
    return M_PointXYToAngle(point.x, point.y);
}

coord_t R_ViewPointDistance(coord_t x, coord_t y)
{
    const Vec3d &viewOrigin = viewPlayer->viewport().current.origin;
    coord_t viewOriginv1[2] = { viewOrigin.x, viewOrigin.y };
    coord_t pointv1[2] = { x, y };
    return M_PointDistance(viewOriginv1, pointv1);
}

void R_ProjectViewRelativeLine2D(coord_t const center[2], dd_bool alignToViewPlane,
                                 coord_t width, coord_t offset, coord_t start[2], coord_t end[2])
{
    const viewdata_t *viewData = &viewPlayer->viewport();
    float sinrv, cosrv;

    if(alignToViewPlane)
    {
        // Should be fully aligned to view plane.
        sinrv = -viewData->viewCos;
        cosrv =  viewData->viewSin;
    }
    else
    {
        // Transform the origin point.
        coord_t trX   = center[VX] - viewData->current.origin.x;
        coord_t trY   = center[VY] - viewData->current.origin.y;
        float thangle = BANG2RAD(bamsAtan2(trY * 10, trX * 10)) - float(de::PI) / 2;
        sinrv = sin(thangle);
        cosrv = cos(thangle);
    }

    start[VX] = center[VX];
    start[VY] = center[VY];

    start[VX] -= cosrv * ((width / 2) + offset);
    start[VY] -= sinrv * ((width / 2) + offset);
    end[VX] = start[VX] + cosrv * width;
    end[VY] = start[VY] + sinrv * width;
}

void R_ProjectViewRelativeLine2D(Vec2d const center, bool alignToViewPlane,
                                 coord_t width, coord_t offset, Vec2d &start, Vec2d &end)
{
    const viewdata_t *viewData = &viewPlayer->viewport();
    float sinrv, cosrv;

    if(alignToViewPlane)
    {
        // Should be fully aligned to view plane.
        sinrv = -viewData->viewCos;
        cosrv =  viewData->viewSin;
    }
    else
    {
        // Transform the origin point.
        coord_t trX   = center[VX] - viewData->current.origin.x;
        coord_t trY   = center[VY] - viewData->current.origin.y;
        float thangle = BANG2RAD(bamsAtan2(trY * 10, trX * 10)) - float(de::PI) / 2;
        sinrv = sin(thangle);
        cosrv = cos(thangle);
    }

    start = center - Vec2d(cosrv * ((width / 2) + offset),
                           sinrv * ((width / 2) + offset));
    end = start + Vec2d(cosrv * width, sinrv * width);
}

bool R_GenerateTexCoords(Vec2f &s, Vec2f &t, const Vec3d &point,
                         float xScale, float yScale, const Vec3d &v1, const Vec3d &v2,
                         const Mat3f &tangentMatrix)
{
    const Vec3d v1ToPoint = v1 - point;
    s[0] = v1ToPoint.dot(tangentMatrix.column(0)/*tangent*/) * xScale + .5f;
    t[0] = v1ToPoint.dot(tangentMatrix.column(1)/*bitangent*/) * yScale + .5f;

    // Is the origin point visible?
    if(s[0] >= 1 || t[0] >= 1)
        return false; // Right on the X axis or below on the Y axis.

    const Vec3d v2ToPoint = v2 - point;
    s[1] = v2ToPoint.dot(tangentMatrix.column(0)) * xScale + .5f;
    t[1] = v2ToPoint.dot(tangentMatrix.column(1)) * yScale + .5f;

    // Is the end point visible?
    if(s[1] <= 0 || t[1] <= 0)
        return false; // Left on the X axis or above on the Y axis.

    return true;
}

#undef R_ChooseAlignModeAndScaleFactor
DE_EXTERN_C dd_bool R_ChooseAlignModeAndScaleFactor(float *scale, int width, int height,
                                                    int availWidth, int availHeight, scalemode_t scaleMode)
{
    if(scaleMode == SCALEMODE_STRETCH)
    {
        if(scale) *scale = 1;
        return true;
    }
    else
    {
        float heightAspectCorrected = height * 1.2f;

        // First try scaling horizontally to fit the available width.
        float factor = float(availWidth) / float(width);
        if(factor * heightAspectCorrected <= availHeight)
        {
            // Fits, use letterbox.
            if(scale) *scale = factor;
            return false;
        }

        // Fit vertically instead.
        if(scale) *scale = float(availHeight) / heightAspectCorrected;
        return true; // Pillarbox.
    }
}

#undef R_ChooseScaleMode2
DE_EXTERN_C scalemode_t R_ChooseScaleMode2(int width, int height, int availWidth, int availHeight,
                                           scalemode_t overrideMode, float stretchEpsilon)
{
    const float availRatio = float(availWidth) / availHeight;
    const float origRatio  = float(width) / (height * 1.2f);

    // Considered identical?
    if(INRANGE_OF(availRatio, origRatio, .001f))
        return SCALEMODE_STRETCH;

    if(SCALEMODE_STRETCH == overrideMode || SCALEMODE_NO_STRETCH  == overrideMode)
        return overrideMode;

    // Within tolerable stretch range?
    return INRANGE_OF(availRatio, origRatio, stretchEpsilon)? SCALEMODE_STRETCH : SCALEMODE_NO_STRETCH;
}

#undef R_ChooseScaleMode
DE_EXTERN_C scalemode_t R_ChooseScaleMode(int width, int height, int availWidth, int availHeight,
                                          scalemode_t overrideMode)
{
    return R_ChooseScaleMode2(availWidth, availHeight, width, height, overrideMode,
                              DEFAULT_SCALEMODE_STRETCH_EPSILON);
}
