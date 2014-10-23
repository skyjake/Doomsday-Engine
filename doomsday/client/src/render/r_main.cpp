/** @file r_main.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
#include "render/r_main.h"

#include "dd_def.h" // finesine
#include "clientapp.h"

#include "render/billboard.h"
#include "render/rend_main.h"
#include "render/vissprite.h"
#include "render/vlight.h"

#include "MaterialSnapshot"

#include "world/map.h"
#include "world/p_players.h"
#include "BspLeaf"
#include "ConvexSubspace"
#include "SectorCluster"

#include <de/GLState>
#include <de/vector1.h>

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
float weaponOffsetScale = 0.3183f; // 1/Pi
byte weaponScaleMode    = SCALEMODE_SMART_STRETCH;

static void setupPSpriteParams(rendpspriteparams_t *params, vispsprite_t *spr)
{
    ddpsprite_t *psp      = spr->psp;
    int const spriteIdx   = psp->statePtr->sprite;
    int const frameIdx    = psp->statePtr->frame;
    float const offScaleY = weaponOffsetScaleY / 1000.0f;

    SpriteViewAngle const &sprViewAngle =
        ClientApp::resourceSystem().sprite(spriteIdx, frameIdx).viewAngle(0);

    Material *material = sprViewAngle.material;
    bool flip          = sprViewAngle.mirrorX;

    MaterialVariantSpec const &spec =
        ClientApp::resourceSystem().materialSpec(PSpriteContext, 0, 1, 0, 0,
                                                 GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                                 0, -2, 0, false, true, true, false);
    MaterialSnapshot const &ms = material->prepare(spec);

    Texture const &tex = ms.texture(MTU_PRIMARY).generalCase();
    variantspecification_t const &texSpec = ms.texture(MTU_PRIMARY).spec().variant;

#define WEAPONTOP   32   /// @todo Currently hardcoded here and in the plugins.

    params->pos[VX] = psp->pos[VX] + tex.origin().x + pspOffset[VX] - texSpec.border;
    params->pos[VY] = WEAPONTOP + offScaleY * (psp->pos[VY] - WEAPONTOP) + tex.origin().y +
                      pspOffset[VY] - texSpec.border;
    params->width  = ms.width() + texSpec.border*2;
    params->height = ms.height() + texSpec.border*2;

    ms.texture(MTU_PRIMARY).glCoords(&params->texOffset[0], &params->texOffset[1]);

    params->texFlip[0] = flip;
    params->texFlip[1] = false;

    params->mat = material;
    params->ambientColor[3] = spr->data.sprite.alpha;

    if(spr->data.sprite.isFullBright)
    {
        params->ambientColor[0] =
            params->ambientColor[1] =
                params->ambientColor[2] = 1;
        params->vLightListIdx = 0;
    }
    else
    {
        Map &map = ClientApp::worldSystem().map();

        if(useBias && map.hasLightGrid())
        {
            // Evaluate the position in the light grid.
            Vector4f color = map.lightGrid().evaluate(spr->origin);

            // Apply light range compression.
            for(int i = 0; i < 3; ++i)
            {
                color[i] += Rend_LightAdaptationDelta(color[i]);
            }

            V3f_Set(params->ambientColor, color.x, color.y, color.z);
        }
        else
        {
            Vector4f const color = spr->data.sprite.bspLeaf->subspace().cluster().lightSourceColorfIntensity();

            // No need for distance attentuation.
            float lightLevel = color.w;

            // Add extra light plus bonus.
            lightLevel += Rend_ExtraLightDelta();
            lightLevel *= pspLightLevelMultiplier;

            // The last step is to compress the resultant light value by
            // the global lighting function.
            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final ambientColor.
            for(int i = 0; i < 3; ++i)
            {
                params->ambientColor[i] = lightLevel * color[i];
            }
        }

        Rend_ApplyTorchLight(params->ambientColor, 0);

        collectaffectinglights_params_t lparams; zap(lparams);
        lparams.origin       = Vector3d(spr->origin);
        lparams.subspace     = spr->data.sprite.bspLeaf->subspacePtr();
        lparams.ambientColor = Vector3f(params->ambientColor);

        params->vLightListIdx = R_CollectAffectingLights(&lparams);
    }

#undef WEAPONTOP
}

void Rend_Draw2DPlayerSprites()
{
    ddplayer_t *ddpl = &viewPlayer->shared;

    // Cameramen have no HUD sprites.
    if(ddpl->flags & DDPF_CAMERA) return;
    if(ddpl->flags & DDPF_CHASECAM) return;

    if(usingFog)
    {
        glEnable(GL_FOG);
    }

    // Check for fullbright.
    int i;
    ddpsprite_t *psp;
    for(i = 0, psp = ddpl->pSprites; i < DDMAXPSPRITES; ++i, psp++)
    {
        vispsprite_t *spr = &visPSprites[i];

        // Should this psprite be drawn?
        if(spr->type != VPSPR_SPRITE) continue; // No...

        // Draw as separate sprites.
        if(spr->psp && spr->psp->statePtr)
        {
            rendpspriteparams_t params;

            setupPSpriteParams(&params, spr);
            Rend_DrawPSprite(params);
        }
    }

    if(usingFog)
    {
        glDisable(GL_FOG);
    }
}

static void setupModelParamsForVisPSprite(vissprite_t &vis, vispsprite_t const *spr)
{
    drawmodelparams_t *params = VS_MODEL(&vis);

    params->mf = spr->data.model.mf;
    params->nextMF = spr->data.model.nextMF;
    params->inter = spr->data.model.inter;
    params->alwaysInterpolate = false;
    params->id = spr->data.model.id;
    params->selector = spr->data.model.selector;
    params->flags = spr->data.model.flags;
    vis.pose.origin[VX] = spr->origin[VX];
    vis.pose.origin[VY] = spr->origin[VY];
    vis.pose.origin[VZ] = spr->origin[VZ];
    vis.pose.srvo[VX] = spr->data.model.visOff[VX];
    vis.pose.srvo[VY] = spr->data.model.visOff[VY];
    vis.pose.srvo[VZ] = spr->data.model.visOff[VZ] - spr->data.model.floorClip;
    vis.pose.topZ = spr->data.model.topZ;
    vis.pose.distance = -10;
    vis.pose.yaw = spr->data.model.yaw;
    vis.pose.extraYawAngle = 0;
    vis.pose.yawAngleOffset = spr->data.model.yawAngleOffset;
    vis.pose.pitch = spr->data.model.pitch;
    vis.pose.extraPitchAngle = 0;
    vis.pose.pitchAngleOffset = spr->data.model.pitchAngleOffset;
    vis.pose.extraScale = 0;
    vis.pose.viewAligned = spr->data.model.viewAligned;
    vis.pose.mirrored = (mirrorHudModels? true : false);
    params->shineYawOffset = -vang;
    params->shinePitchOffset = vpitch + 90;
    params->shineTranslateWithViewerPos = false;
    params->shinepspriteCoordSpace = true;
    vis.light.ambientColor[CA] = spr->data.model.alpha;

    if((levelFullBright || spr->data.model.stateFullBright) &&
       !spr->data.model.mf->testSubFlag(0, MFF_DIM))
    {
        vis.light.ambientColor[CR] = vis.light.ambientColor[CG] = vis.light.ambientColor[CB] = 1;
        vis.light.vLightListIdx = 0;
    }
    else
    {
        Map &map = ClientApp::worldSystem().map();

        if(useBias && map.hasLightGrid())
        {
            Vector4f color = map.lightGrid().evaluate(vis.pose.origin);
            // Apply light range compression.
            for(int i = 0; i < 3; ++i)
            {
                color[i] += Rend_LightAdaptationDelta(color[i]);
            }
            vis.light.ambientColor.x = color.x;
            vis.light.ambientColor.y = color.y;
            vis.light.ambientColor.z = color.z;
        }
        else
        {
            Vector4f const color = spr->data.model.bspLeaf->subspace().cluster().lightSourceColorfIntensity();

            // No need for distance attentuation.
            float lightLevel = color.w;

            // Add extra light.
            lightLevel += Rend_ExtraLightDelta();

            // The last step is to compress the resultant light value by
            // the global lighting function.
            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final ambientColor.
            for(int i = 0; i < 3; ++i)
            {
                vis.light.ambientColor[i] = lightLevel * color[i];
            }
        }

        Rend_ApplyTorchLight(vis.light.ambientColor, vis.pose.distance);

        collectaffectinglights_params_t lparams; zap(lparams);
        lparams.origin       = Vector3d(spr->origin);
        lparams.subspace     = spr->data.model.bspLeaf->subspacePtr();
        lparams.ambientColor = Vector3f(vis.light.ambientColor);
        lparams.starkLight   = true;

        vis.light.vLightListIdx = R_CollectAffectingLights(&lparams);
    }
}

void Rend_Draw3DPlayerSprites()
{
    // Setup the modelview matrix.
    Rend_ModelViewMatrix(false /* don't apply view angle rotation */);

    static GLTexture localDepth; // note: static!
    GLTarget::AlternativeBuffer altDepth(GLState::current().target(), localDepth, GLTarget::DepthStencil);

    for(int i = 0; i < DDMAXPSPRITES; ++i)
    {
        vispsprite_t *spr = &visPSprites[i];

        if(spr->type != VPSPR_MODEL) continue; // Not used.

        if(altDepth.init())
        {
            // Clear the depth before first use.
            altDepth.target().clear(GLTarget::DepthStencil);
        }

        //drawmodelparams_t parms; zap(parms);
        vissprite_t temp; zap(temp);
        setupModelParamsForVisPSprite(temp, spr);
        Rend_DrawModel(temp);
    }
}
