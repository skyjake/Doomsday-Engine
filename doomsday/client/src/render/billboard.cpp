/** @file billboard.cpp  Rendering billboard "sprites".
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"

#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "Texture"

#include "world/map.h"
#include "world/thinkers.h"
#include "BspLeaf"

#include "resource/sprite.h"

#include "render/vissprite.h"

#include "render/billboard.h"

using namespace de;

void Rend_RenderSprite(rendspriteparams_t const *params);

static void setupPSpriteParams(rendpspriteparams_t *params, vispsprite_t *spr);
static void setupModelParamsForVisPSprite(rendmodelparams_t *params, vispsprite_t *spr);

int spriteLight = 4;
float maxSpriteAngle = 60;

// If true - use the "no translucency" blending mode for sprites/masked walls
byte noSpriteTrans;
int useSpriteAlpha = 1;
int useSpriteBlend = 1;

int alwaysAlign;
int noSpriteZWrite;

byte devNoSprites;

void Rend_SpriteRegister()
{
    C_VAR_INT   ("rend-sprite-align",       &alwaysAlign,       0, 0, 3);
    C_VAR_FLOAT ("rend-sprite-align-angle", &maxSpriteAngle,    0, 0, 90);
    C_VAR_INT   ("rend-sprite-alpha",       &useSpriteAlpha,    0, 0, 1);
    C_VAR_INT   ("rend-sprite-blend",       &useSpriteBlend,    0, 0, 1);
    C_VAR_INT   ("rend-sprite-lights",      &spriteLight,       0, 0, 10);
    C_VAR_BYTE  ("rend-sprite-mode",        &noSpriteTrans,     0, 0, 1);
    C_VAR_INT   ("rend-sprite-noz",         &noSpriteZWrite,    0, 0, 1);
    C_VAR_BYTE  ("rend-sprite-precache",    &precacheSprites,   0, 0, 1);
    C_VAR_BYTE  ("rend-dev-nosprite",       &devNoSprites,      CVF_NO_ARCHIVE, 0, 1);
}

static inline void renderQuad(dgl_vertex_t *v, dgl_color_t *c, dgl_texcoord_t *tc)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBegin(GL_QUADS);
        glColor4ubv(c[0].rgba);
        glTexCoord2fv(tc[0].st);
        glVertex3fv(v[0].xyz);

        glColor4ubv(c[1].rgba);
        glTexCoord2fv(tc[1].st);
        glVertex3fv(v[1].xyz);

        glColor4ubv(c[2].rgba);
        glTexCoord2fv(tc[2].st);
        glVertex3fv(v[2].xyz);

        glColor4ubv(c[3].rgba);
        glTexCoord2fv(tc[3].st);
        glVertex3fv(v[3].xyz);
    glEnd();
}

void Rend_Draw3DPlayerSprites()
{
    // Setup the modelview matrix.
    Rend_ModelViewMatrix(false /* don't apply view angle rotation */);

    // Clear Z buffer. This will prevent the psprites from being clipped
    // by nearby polygons.
    glClear(GL_DEPTH_BUFFER_BIT);

    rendmodelparams_t parm;
    for(int i = 0; i < DDMAXPSPRITES; ++i)
    {
        vispsprite_t *spr = &visPSprites[i];

        if(spr->type != VPSPR_MODEL) continue; // Not used.

        setupModelParamsForVisPSprite(&parm, spr);
        Rend_RenderModel(&parm);
    }
}

/**
 * Set all the colors in the array to that specified.
 */
void Spr_UniformVertexColors(int count, dgl_color_t *colors, float const *rgba)
{
    for(; count-- > 0; colors++)
    {
        colors->rgba[CR] = (byte) (255 * rgba[CR]);
        colors->rgba[CG] = (byte) (255 * rgba[CG]);
        colors->rgba[CB] = (byte) (255 * rgba[CB]);
        colors->rgba[CA] = (byte) (255 * rgba[CA]);
    }
}

struct lightspriteworker_params_t
{
    Vector3f color, extra;
    Vector3f normal;
    uint numProcessed, max;
};

static void lightSprite(VectorLight const &vlight, lightspriteworker_params_t &parms)
{
    float strength = vlight.direction.dot(parms.normal)
                   + vlight.offset; // Shift toward the light a little.

    // Ability to both light and shade.
    if(strength > 0)
    {
        strength *= vlight.lightSide;
    }
    else
    {
        strength *= vlight.darkSide;
    }

    Vector3f &dest = vlight.affectedByAmbient? parms.color : parms.extra;
    dest += vlight.color * de::clamp(-1.f, strength, 1.f);
}

static int lightSpriteWorker(VectorLight const *vlight, void *context)
{
    lightspriteworker_params_t &parms = *static_cast<lightspriteworker_params_t *>(context);

    lightSprite(*vlight, parms);
    parms.numProcessed += 1;

    // Time to stop?
    return parms.max && parms.numProcessed == parms.max;
}

/**
 * Calculate vertex lighting.
 */
void Spr_VertexColors(int count, dgl_color_t *out, dgl_vertex_t *normal,
    uint vLightListIdx, uint maxLights, float const *ambient)
{
    Vector3f const saturated(1, 1, 1);
    lightspriteworker_params_t parms;

    for(int i = 0; i < count; ++i, out++, normal++)
    {
        // Begin with total darkness.
        parms.color        = Vector3f();
        parms.extra        = Vector3f();
        parms.normal       = Vector3f(normal->xyz);
        parms.max          = maxLights;
        parms.numProcessed = 0;

        VL_ListIterator(vLightListIdx, lightSpriteWorker, &parms);

        // Check for ambient and convert to ubyte.
        Vector3f color = (parms.color.max(ambient) + parms.extra).min(saturated);

        out->rgba[CR] = byte( 255 * color.x );
        out->rgba[CG] = byte( 255 * color.y );
        out->rgba[CB] = byte( 255 * color.z );
        out->rgba[CA] = byte( 255 * ambient[CA] );
    }
}

static void setupPSpriteParams(rendpspriteparams_t *params, vispsprite_t *spr)
{
    ddpsprite_t *psp      = spr->psp;
    int const spriteIdx   = psp->statePtr->sprite;
    int const frameIdx    = psp->statePtr->frame;
    float const offScaleY = weaponOffsetScaleY / 1000.0f;

    Sprite const *sprite  = &App_ResourceSystem().sprite(spriteIdx, frameIdx);

    Material *material = sprite->viewAngle(0).material;
    bool flip          = sprite->viewAngle(0).mirrorX;

    MaterialVariantSpec const &spec =
        App_Materials().variantSpec(PSpriteContext, 0, 1, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                    0, -2, 0, false, true, true, false);
    MaterialSnapshot const &ms = material->prepare(spec);

    Texture const &tex = ms.texture(MTU_PRIMARY).generalCase();
    variantspecification_t const &texSpec = TS_GENERAL(ms.texture(MTU_PRIMARY).spec());

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
    params->ambientColor[CA] = spr->data.sprite.alpha;

    if(spr->data.sprite.isFullBright)
    {
        params->ambientColor[CR] = params->ambientColor[CG] =
            params->ambientColor[CB] = 1;
        params->vLightListIdx = 0;
    }
    else
    {
        Map &map = App_World().map();

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
}

MaterialVariantSpec const &PSprite_MaterialSpec()
{
    return App_Materials().variantSpec(SpriteContext, 0, 0, 0, 0,
                                       GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                       1, -2, 0, false, true, true, false);
}

void Rend_DrawPSprite(rendpspriteparams_t const *params)
{
    if(renderTextures == 1)
    {
        GL_SetPSprite(params->mat, 0, 0);
        glEnable(GL_TEXTURE_2D);
    }
    else if(renderTextures == 2)
    {
        // For lighting debug, render all solid surfaces using the gray texture.
        MaterialSnapshot const &ms =
            App_Materials().find(de::Uri("System", Path("gray"))).material().prepare(PSprite_MaterialSpec());

        GL_BindTexture(&ms.texture(MTU_PRIMARY));
        glEnable(GL_TEXTURE_2D);
    }

    //  0---1
    //  |   |  Vertex layout.
    //  3---2

    float v1[2], v2[2], v3[2], v4[2];
    v1[VX] = params->pos[VX];
    v1[VY] = params->pos[VY];

    v2[VX] = params->pos[VX] + params->width;
    v2[VY] = params->pos[VY];

    v3[VX] = params->pos[VX] + params->width;
    v3[VY] = params->pos[VY] + params->height;

    v4[VX] = params->pos[VX];
    v4[VY] = params->pos[VY] + params->height;

    // All psprite vertices are co-plannar, so just copy the view front vector.
    // @todo: Can we do something better here?
    float const *frontVec = R_ViewData(viewPlayer - ddPlayers)->frontVec;
    dgl_vertex_t quadNormals[4];
    for(int i = 0; i < 4; ++i)
    {
        quadNormals[i].xyz[VX] = frontVec[VX];
        quadNormals[i].xyz[VY] = frontVec[VZ];
        quadNormals[i].xyz[VZ] = frontVec[VY];
    }

    dgl_color_t quadColors[4];
    if(!params->vLightListIdx)
    {
        // Lit uniformly.
        Spr_UniformVertexColors(4, quadColors, params->ambientColor);
    }
    else
    {
        // Lit normally.
        Spr_VertexColors(4, quadColors, quadNormals, params->vLightListIdx, spriteLight + 1, params->ambientColor);
    }

    dgl_texcoord_t tcs[4], *tc = tcs;
    dgl_color_t *c = quadColors;

    tc[0].st[0] = params->texOffset[0] *  (params->texFlip[0]? 1:0);
    tc[0].st[1] = params->texOffset[1] *  (params->texFlip[1]? 1:0);
    tc[1].st[0] = params->texOffset[0] * (!params->texFlip[0]? 1:0);
    tc[1].st[1] = params->texOffset[1] *  (params->texFlip[1]? 1:0);
    tc[2].st[0] = params->texOffset[0] * (!params->texFlip[0]? 1:0);
    tc[2].st[1] = params->texOffset[1] * (!params->texFlip[1]? 1:0);
    tc[3].st[0] = params->texOffset[0] *  (params->texFlip[0]? 1:0);
    tc[3].st[1] = params->texOffset[1] * (!params->texFlip[1]? 1:0);

    glBegin(GL_QUADS);
        glColor4ubv(c[0].rgba);
        glTexCoord2fv(tc[0].st);
        glVertex2fv(v1);

        glColor4ubv(c[1].rgba);
        glTexCoord2fv(tc[1].st);
        glVertex2fv(v2);

        glColor4ubv(c[2].rgba);
        glTexCoord2fv(tc[2].st);
        glVertex2fv(v3);

        glColor4ubv(c[3].rgba);
        glTexCoord2fv(tc[3].st);
        glVertex2fv(v4);
    glEnd();

    if(renderTextures)
    {
        glDisable(GL_TEXTURE_2D);
    }
}

void Rend_Draw2DPlayerSprites(void)
{
    int i;
    ddplayer_t *ddpl = &viewPlayer->shared;
    ddpsprite_t *psp;

    // Cameramen have no HUD sprites.
    if((ddpl->flags & DDPF_CAMERA) || (ddpl->flags & DDPF_CHASECAM)) return;

    if(usingFog)
    {
        glEnable(GL_FOG);
    }

    // Check for fullbright.
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
            Rend_DrawPSprite(&params);
        }
    }

    if(usingFog)
    {
        glDisable(GL_FOG);
    }
}

/**
 * A sort of a sprite, I guess... Masked walls must be rendered sorted
 * with sprites, so no artifacts appear when sprites are seen behind
 * masked walls.
 */
void Rend_RenderMaskedWall(rendmaskedwallparams_t const *p)
{
    GLenum normalTarget, dynTarget;

    Texture::Variant *tex = 0;
    if(renderTextures)
    {
        MaterialSnapshot const &ms =
            reinterpret_cast<MaterialVariant *>(p->material)->prepare();
        tex = &ms.texture(MTU_PRIMARY);
    }

    // Do we have a dynamic light to blend with?
    // This only happens when multitexturing is enabled.
    bool withDyn = false;
    int normal = 0, dyn = 1;
    if(p->modTex && numTexUnits > 1)
    {
        if(IS_MUL)
        {
            normal = 1;
            dyn = 0;
        }
        else
        {
            normal = 0;
            dyn = 1;
        }

        GL_SelectTexUnits(2);
        GL_ModulateTexture(IS_MUL ? 4 : 5);

        // The dynamic light.
        glActiveTexture(IS_MUL ? GL_TEXTURE0 : GL_TEXTURE1);
        /// @todo modTex may be the name of a "managed" texture.
        GL_BindTextureUnmanaged(renderTextures ? p->modTex : 0,
                                gl::ClampToEdge, gl::ClampToEdge);

        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, p->modColor);

        // The actual texture.
        glActiveTexture(IS_MUL ? GL_TEXTURE1 : GL_TEXTURE0);
        GL_BindTexture(tex);

        withDyn = true;
    }
    else
    {
        GL_ModulateTexture(1);
        glEnable(GL_TEXTURE_2D);
        GL_BindTexture(tex);
        normal = 0;
    }

    GL_BlendMode(p->blendMode);

    normalTarget = normal? GL_TEXTURE1 : GL_TEXTURE0;
    dynTarget    =    dyn? GL_TEXTURE1 : GL_TEXTURE0;

    // Draw one quad. This is obviously not a very efficient way to render
    // lots of masked walls, but since 3D models and sprites must be
    // rendered interleaved with masked walls, there's not much that can be
    // done about this.
    if(withDyn)
    {
        glBegin(GL_QUADS);
            glColor4fv(p->vertices[0].color);
            glMultiTexCoord2f(normalTarget, p->texCoord[0][0], p->texCoord[1][1]);

            glMultiTexCoord2f(dynTarget, p->modTexCoord[0][0], p->modTexCoord[1][1]);

            glVertex3f(p->vertices[0].pos[VX],
                       p->vertices[0].pos[VZ],
                       p->vertices[0].pos[VY]);

            glColor4fv(p->vertices[1].color);
            glMultiTexCoord2f(normalTarget, p->texCoord[0][0], p->texCoord[0][1]);

            glMultiTexCoord2f(dynTarget, p->modTexCoord[0][0], p->modTexCoord[0][1]);

            glVertex3f(p->vertices[1].pos[VX],
                       p->vertices[1].pos[VZ],
                       p->vertices[1].pos[VY]);

            glColor4fv(p->vertices[3].color);
            glMultiTexCoord2f(normalTarget, p->texCoord[1][0], p->texCoord[0][1]);

            glMultiTexCoord2f(dynTarget, p->modTexCoord[1][0], p->modTexCoord[0][1]);

            glVertex3f(p->vertices[3].pos[VX],
                       p->vertices[3].pos[VZ],
                       p->vertices[3].pos[VY]);

            glColor4fv(p->vertices[2].color);
            glMultiTexCoord2f(normalTarget, p->texCoord[1][0], p->texCoord[1][1]);

            glMultiTexCoord2f(dynTarget, p->modTexCoord[1][0], p->modTexCoord[1][1]);

            glVertex3f(p->vertices[2].pos[VX],
                       p->vertices[2].pos[VZ],
                       p->vertices[2].pos[VY]);
        glEnd();

        // Restore normal GL state.
        GL_SelectTexUnits(1);
        GL_ModulateTexture(1);
    }
    else
    {
        glBegin(GL_QUADS);
            glColor4fv(p->vertices[0].color);
            glTexCoord2f(p->texCoord[0][0], p->texCoord[1][1]);

            glVertex3f(p->vertices[0].pos[VX],
                       p->vertices[0].pos[VZ],
                       p->vertices[0].pos[VY]);

            glColor4fv(p->vertices[1].color);
            glTexCoord2f(p->texCoord[0][0], p->texCoord[0][1]);

            glVertex3f(p->vertices[1].pos[VX],
                       p->vertices[1].pos[VZ],
                       p->vertices[1].pos[VY]);

            glColor4fv(p->vertices[3].color);
            glTexCoord2f(p->texCoord[1][0], p->texCoord[0][1]);

            glVertex3f(p->vertices[3].pos[VX],
                       p->vertices[3].pos[VZ],
                       p->vertices[3].pos[VY]);

            glColor4fv(p->vertices[2].color);
            glTexCoord2f(p->texCoord[1][0], p->texCoord[1][1]);

            glVertex3f(p->vertices[2].pos[VX],
                       p->vertices[2].pos[VZ],
                       p->vertices[2].pos[VY]);
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    GL_BlendMode(BM_NORMAL);
}

static void setupModelParamsForVisPSprite(rendmodelparams_t *params, vispsprite_t *spr)
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
        Map &map = App_World().map();

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

static bool generateHaloForVisSprite(vissprite_t const *spr, bool primary = false)
{
    float occlusionFactor;

    if(primary && (spr->data.flare.flags & RFF_NO_PRIMARY))
        return false;

    if(spr->data.flare.isDecoration)
    {
        /*
         * Surface decorations do not yet persist over frames, so we do
         * not smoothly occlude their flares. Instead, we will have to
         * put up with them instantly appearing/disappearing.
         */
        occlusionFactor = R_ViewerLumobjIsClipped(spr->data.flare.lumIdx)? 0 : 1;
    }
    else
    {
        occlusionFactor = (spr->data.flare.factor & 0x7f) / 127.0f;
    }

    return H_RenderHalo(spr->origin,
                        spr->data.flare.size,
                        spr->data.flare.tex,
                        spr->data.flare.color,
                        spr->distance,
                        occlusionFactor, spr->data.flare.mul,
                        spr->data.flare.xOff, primary,
                        (spr->data.flare.flags & RFF_NO_TURN) == 0);
}

void Rend_DrawMasked()
{
    if(devNoSprites) return;

    R_SortVisSprites();

    if(visSpriteP > visSprites)
    {
        bool primaryHaloDrawn = false;

        // Draw all vissprites back to front.
        // Sprites look better with Z buffer writes turned off.
        for(vissprite_t *spr = visSprSortedHead.next; spr != &visSprSortedHead; spr = spr->next)
        {
            switch(spr->type)
            {
            default: break;

            case VSPR_MASKED_WALL:
                // A masked wall is a specialized sprite.
                Rend_RenderMaskedWall(&spr->data.wall);
                break;

            case VSPR_SPRITE:
                // Render an old fashioned sprite, ah the nostalgia...
                Rend_RenderSprite(&spr->data.sprite);
                break;

            case VSPR_MODEL:
                Rend_RenderModel(&spr->data.model);
                break;

            case VSPR_FLARE:
                if(generateHaloForVisSprite(spr, true))
                {
                    primaryHaloDrawn = true;
                }
                break;
            }
        }

        // Draw secondary halos?
        if(primaryHaloDrawn && haloMode > 1)
        {
            // Now we can setup the state only once.
            H_SetupState(true);

            for(vissprite_t *spr = visSprSortedHead.next; spr != &visSprSortedHead; spr = spr->next)
            {
                if(spr->type == VSPR_FLARE)
                {
                    generateHaloForVisSprite(spr);
                }
            }

            // And we're done...
            H_SetupState(false);
        }
    }
}

MaterialVariantSpec const &Rend_SpriteMaterialSpec(int tclass, int tmap)
{
    return App_Materials().variantSpec(SpriteContext, 0, 1, tclass, tmap,
                                       GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                       1, -2, -1, true, true, true, false);
}

static MaterialVariant *chooseSpriteMaterial(rendspriteparams_t const &p)
{
    if(!renderTextures) return 0;
    if(renderTextures == 2)
    {
        // For lighting debug, render all solid surfaces using the gray texture.
        return App_Materials().find(de::Uri("System", Path("gray"))).material().chooseVariant(Rend_SpriteMaterialSpec(), true);
    }

    // Use the pre-chosen sprite.
    return reinterpret_cast<MaterialVariant *>(p.material);
}

static int drawVectorLightWorker(VectorLight const *vlight, void *context)
{
    coord_t distFromViewer = *static_cast<coord_t *>(context);
    if(distFromViewer < 1600 - 8)
    {
        Rend_DrawVectorLight(vlight, 1 - distFromViewer / 1600);
    }
    return false; // Continue iteration.
}

void Rend_RenderSprite(rendspriteparams_t const *params)
{
    coord_t v1[3], v2[3], v3[3], v4[3];
    Point2Rawf viewOffset = Point2Rawf(0, 0); ///< View-aligned offset to center point.
    Size2Rawf size = Size2Rawf(0, 0);
    dgl_color_t quadColors[4];
    dgl_vertex_t quadNormals[4];
    boolean restoreMatrix = false;
    boolean restoreZ = false;
    coord_t spriteCenter[3];
    coord_t surfaceNormal[3];
    MaterialVariant *mat = 0;
    MaterialSnapshot const *ms = 0;
    float s = 1, t = 1; ///< Bottom right coords.
    int i;

    // Many sprite properties are inherited from the material.
    if(params->material)
    {
        // Ensure this variant has been prepared.
        ms = &reinterpret_cast<MaterialVariant *>(params->material)->prepare();

        variantspecification_t const &texSpec = TS_GENERAL(ms->texture(MTU_PRIMARY).spec());
        size.width  = ms->width() + texSpec.border * 2;
        size.height = ms->height() + texSpec.border * 2;
        viewOffset.x = -size.width / 2;

        ms->texture(MTU_PRIMARY).glCoords(&s, &t);

        Texture &tex = ms->texture(MTU_PRIMARY).generalCase();
        viewOffset.x += float(-tex.origin().x);
    }

    // We may want to draw using another material instead.
    mat = chooseSpriteMaterial(*params);
    if(mat != reinterpret_cast<MaterialVariant *>(params->material))
    {
        ms = mat? &mat->prepare() : 0;
    }

    if(ms)
    {
        GL_BindTexture(&ms->texture(MTU_PRIMARY));
        glEnable(GL_TEXTURE_2D);
    }
    else
    {
        GL_SetNoTexture();
    }

    // Coordinates to the center of the sprite (game coords).
    spriteCenter[VX] = params->center[VX] + params->srvo[VX];
    spriteCenter[VY] = params->center[VY] + params->srvo[VY];
    spriteCenter[VZ] = params->center[VZ] + params->srvo[VZ];

    R_ProjectViewRelativeLine2D(spriteCenter, params->viewAligned,
                                size.width, viewOffset.x, v1, v4);

    v2[VX] = v1[VX];
    v2[VY] = v1[VY];
    v3[VX] = v4[VX];
    v3[VY] = v4[VY];

    v1[VZ] = v4[VZ] = spriteCenter[VZ] - size.height / 2 + viewOffset.y;
    v2[VZ] = v3[VZ] = spriteCenter[VZ] + size.height / 2 + viewOffset.y;

    // Calculate the surface normal.
    V3d_PointCrossProduct(surfaceNormal, v2, v1, v3);
    V3d_Normalize(surfaceNormal);

/*#if _DEBUG
    // Draw the surface normal.
    glBegin(GL_LINES);
    glColor4f(1, 0, 0, 1);
    glVertex3f(spriteCenter[VX], spriteCenter[VZ], spriteCenter[VY]);
    glColor4f(1, 0, 0, 0);
    glVertex3f(spriteCenter[VX] + surfaceNormal[VX] * 10,
               spriteCenter[VZ] + surfaceNormal[VZ] * 10,
               spriteCenter[VY] + surfaceNormal[VY] * 10);
    glEnd();
#endif*/

    // All sprite vertices are co-plannar, so just copy the surface normal.
    // @todo: Can we do something better here?
    for(i = 0; i < 4; ++i)
    {
        V3f_Copyd(quadNormals[i].xyz, surfaceNormal);
    }

    if(!params->vLightListIdx)
    {
        // Lit uniformly.
        Spr_UniformVertexColors(4, quadColors, params->ambientColor);
    }
    else
    {
        // Lit normally.
        Spr_VertexColors(4, quadColors, quadNormals, params->vLightListIdx,
                         spriteLight + 1, params->ambientColor);
    }

    // Do we need to do some aligning?
    if(params->viewAligned || alwaysAlign >= 2)
    {
        // We must set up a modelview transformation matrix.
        restoreMatrix = true;
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        // Rotate around the center of the sprite.
        glTranslatef(spriteCenter[VX], spriteCenter[VZ], spriteCenter[VY]);
        if(!params->viewAligned)
        {
            float s_dx = v1[VX] - v2[VX];
            float s_dy = v1[VY] - v2[VY];

            if(alwaysAlign == 2)
            {   // Restricted camera alignment.
                float dx = spriteCenter[VX] - vOrigin[VX];
                float dy = spriteCenter[VY] - vOrigin[VZ];
                float spriteAngle = BANG2DEG(
                    bamsAtan2(spriteCenter[VZ] - vOrigin[VY], sqrt(dx * dx + dy * dy)));

                if(spriteAngle > 180)
                    spriteAngle -= 360;

                if(fabs(spriteAngle) > maxSpriteAngle)
                {
                    float turnAngle = (spriteAngle > 0? spriteAngle - maxSpriteAngle :
                                                        spriteAngle + maxSpriteAngle);

                    // Rotate along the sprite edge.
                    glRotatef(turnAngle, s_dx, 0, s_dy);
                }
            }
            else
            {   // Restricted view plane alignment.
                // This'll do, for now... Really it should notice both the
                // sprite angle and vpitch.
                glRotatef(vpitch * .5f, s_dx, 0, s_dy);
            }
        }
        else
        {
            // Normal rotation perpendicular to the view plane.
            glRotatef(vpitch, viewsidex, 0, viewsidey);
        }
        glTranslatef(-spriteCenter[VX], -spriteCenter[VZ], -spriteCenter[VY]);
    }

    // Need to change blending modes?
    if(params->blendMode != BM_NORMAL)
        GL_BlendMode(params->blendMode);

    // Transparent sprites shouldn't be written to the Z buffer.
    if(params->noZWrite || params->ambientColor[CA] < .98f ||
       !(params->blendMode == BM_NORMAL || params->blendMode == BM_ZEROALPHA))
    {
        restoreZ = true;
        glDepthMask(GL_FALSE);
    }

    dgl_vertex_t vs[4], *v = vs;
    dgl_texcoord_t tcs[4], *tc = tcs;

    //  1---2
    //  |   |  Vertex layout.
    //  0---3

    v[0].xyz[0] = v1[VX];
    v[0].xyz[1] = v1[VZ];
    v[0].xyz[2] = v1[VY];

    v[1].xyz[0] = v2[VX];
    v[1].xyz[1] = v2[VZ];
    v[1].xyz[2] = v2[VY];

    v[2].xyz[0] = v3[VX];
    v[2].xyz[1] = v3[VZ];
    v[2].xyz[2] = v3[VY];

    v[3].xyz[0] = v4[VX];
    v[3].xyz[1] = v4[VZ];
    v[3].xyz[2] = v4[VY];

    tc[0].st[0] = s *  (params->matFlip[0]? 1:0);
    tc[0].st[1] = t * (!params->matFlip[1]? 1:0);
    tc[1].st[0] = s *  (params->matFlip[0]? 1:0);
    tc[1].st[1] = t *  (params->matFlip[1]? 1:0);
    tc[2].st[0] = s * (!params->matFlip[0]? 1:0);
    tc[2].st[1] = t *  (params->matFlip[1]? 1:0);
    tc[3].st[0] = s * (!params->matFlip[0]? 1:0);
    tc[3].st[1] = t * (!params->matFlip[1]? 1:0);

    renderQuad(v, quadColors, tc);

    if(ms) glDisable(GL_TEXTURE_2D);

    if(devMobjVLights && params->vLightListIdx)
    {
        // Draw the vlight vectors, for debug.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(params->center[VX], params->center[VZ], params->center[VY]);

        coord_t distFromViewer = de::abs(params->distance);
        VL_ListIterator(params->vLightListIdx, drawVectorLightWorker, &distFromViewer);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }

    // Need to restore the original modelview matrix?
    if(restoreMatrix)
        glPopMatrix();

    // Change back to normal blending?
    if(params->blendMode != BM_NORMAL)
        GL_BlendMode(BM_NORMAL);

    // Enable Z-writing again?
    if(restoreZ)
        glDepthMask(GL_TRUE);
}
