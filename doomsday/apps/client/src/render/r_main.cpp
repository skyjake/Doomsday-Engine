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

dint levelFullBright;
dint weaponOffsetScaleY = 1000;
dint psp3d;

dfloat pspLightLevelMultiplier = 1;
dfloat pspOffset[2];

/*
 * Console variables:
 */
dfloat weaponFOVShift    = 45;
dfloat weaponOffsetScale = 0.3183f;  // 1/Pi
dbyte weaponScaleMode    = SCALEMODE_SMART_STRETCH;

static const MaterialVariantSpec &pspriteMaterialSpec()
{
    return ClientApp::resources().materialSpec(PSpriteContext, 0, 1, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                 0, -2, 0, false, true, true, false);
}

static void setupPSpriteParams(rendpspriteparams_t &parm, const vispsprite_t &vs)
{
    static const dint WEAPONTOP = 32;  /// @todo Currently hardcoded here and in the plugins.

    const dfloat offScaleY = ::weaponOffsetScaleY / 1000.0f;

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
            for (dint i = 0; i < 3; ++i)
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
            dfloat lightLevel = color.w;

            // Add extra light plus bonus.
            lightLevel += Rend_ExtraLightDelta();
            lightLevel *= pspLightLevelMultiplier;

            // The last step is to compress the resultant light value by
            // the global lighting function.
            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final ambientColor.
            for (dint i = 0; i < 3; ++i)
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
