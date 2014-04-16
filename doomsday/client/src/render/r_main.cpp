/** @file r_main.cpp
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
            Vector3f tmp = map.lightGrid().evaluate(spr->origin);
            V3f_Set(params->ambientColor, tmp.x, tmp.y, tmp.z);
        }
        else
        {
            SectorCluster &cluster = spr->data.sprite.bspLeaf->cluster();
            Vector3f const &secColor = Rend_SectorLightColor(cluster);

            // No need for distance attentuation.
            float lightLevel = cluster.sector().lightLevel();

            // Add extra light plus bonus.
            lightLevel += Rend_ExtraLightDelta();
            lightLevel *= pspLightLevelMultiplier;

            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final ambientColor in affect.
            for(int i = 0; i < 3; ++i)
            {
                params->ambientColor[i] = lightLevel * secColor[i];
            }
        }

        Rend_ApplyTorchLight(params->ambientColor, 0);

        collectaffectinglights_params_t lparams; zap(lparams);
        lparams.origin       = Vector3d(spr->origin);
        lparams.bspLeaf      = spr->data.sprite.bspLeaf;
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

static void setupModelParamsForVisPSprite(drawmodelparams_t *params, vispsprite_t *spr)
{
    params->mf = spr->data.model.mf;
    params->nextMF = spr->data.model.nextMF;
    params->inter = spr->data.model.inter;
    params->alwaysInterpolate = false;
    params->id = spr->data.model.id;
    params->selector = spr->data.model.selector;
    params->flags = spr->data.model.flags;
    params->origin[VX] = spr->origin[VX];
    params->origin[VY] = spr->origin[VY];
    params->origin[VZ] = spr->origin[VZ];
    params->srvo[VX] = spr->data.model.visOff[VX];
    params->srvo[VY] = spr->data.model.visOff[VY];
    params->srvo[VZ] = spr->data.model.visOff[VZ] - spr->data.model.floorClip;
    params->gzt = spr->data.model.gzt;
    params->distance = -10;
    params->yaw = spr->data.model.yaw;
    params->extraYawAngle = 0;
    params->yawAngleOffset = spr->data.model.yawAngleOffset;
    params->pitch = spr->data.model.pitch;
    params->extraPitchAngle = 0;
    params->pitchAngleOffset = spr->data.model.pitchAngleOffset;
    params->extraScale = 0;
    params->viewAlign = spr->data.model.viewAligned;
    params->mirror = (mirrorHudModels? true : false);
    params->shineYawOffset = -vang;
    params->shinePitchOffset = vpitch + 90;
    params->shineTranslateWithViewerPos = false;
    params->shinepspriteCoordSpace = true;
    params->ambientColor[CA] = spr->data.model.alpha;

    if((levelFullBright || spr->data.model.stateFullBright) &&
       !spr->data.model.mf->testSubFlag(0, MFF_DIM))
    {
        params->ambientColor[CR] = params->ambientColor[CG] = params->ambientColor[CB] = 1;
        params->vLightListIdx = 0;
    }
    else
    {
        Map &map = ClientApp::worldSystem().map();

        if(useBias && map.hasLightGrid())
        {
            Vector3f tmp = map.lightGrid().evaluate(params->origin);
            V3f_Set(params->ambientColor, tmp.x, tmp.y, tmp.z);
        }
        else
        {
            SectorCluster &cluster = spr->data.model.bspLeaf->cluster();
            Vector3f const &secColor = Rend_SectorLightColor(cluster);

            // Diminished light (with compression).
            float lightLevel = cluster.sector().lightLevel();

            // No need for distance attentuation.

            // Add extra light.
            lightLevel += Rend_ExtraLightDelta();

            // The last step is to compress the resultant light value by
            // the global lighting function.
            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final ambientColor in effect.
            for(int i = 0; i < 3; ++i)
            {
                params->ambientColor[i] = lightLevel * secColor[i];
            }
        }

        Rend_ApplyTorchLight(params->ambientColor, params->distance);

        collectaffectinglights_params_t lparams; zap(lparams);
        lparams.origin       = Vector3d(spr->origin);
        lparams.bspLeaf      = spr->data.model.bspLeaf;
        lparams.ambientColor = Vector3f(params->ambientColor);
        lparams.starkLight   = true;

        params->vLightListIdx = R_CollectAffectingLights(&lparams);
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

        drawmodelparams_t parms; zap(parms);
        setupModelParamsForVisPSprite(&parms, spr);
        Rend_DrawModel(parms);
    }
}
