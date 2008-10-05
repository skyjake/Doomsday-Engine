/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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

/**
 * rend_sprite.c: Rendering Map Objects as 2D Sprites
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_dgl.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define DOTPROD(a, b)       (a[0]*b[0] + a[1]*b[1] + a[2]*b[2])

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void setupPSpriteParams(rendpspriteparams_t *params,
                               vispsprite_t *spr);
static void setupModelParamsForVisPSprite(rendmodelparams_t* params,
                                          vispsprite_t* spr);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean willRenderSprites;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int spriteLight = 4;
float maxSpriteAngle = 60;

// If true - use the "no translucency" blending mode for sprites/masked walls
byte noSpriteTrans = false;
int useSpriteAlpha = 1;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Rend_SpriteRegister(void)
{
    // Cvars
    C_VAR_INT("rend-sprite-align", &alwaysAlign, 0, 0, 3);
    C_VAR_FLOAT("rend-sprite-align-angle", &maxSpriteAngle, 0, 0, 90);
    C_VAR_INT("rend-sprite-alpha", &useSpriteAlpha, 0, 0, 1);
    C_VAR_INT("rend-sprite-blend", &missileBlend, 0, 0, 1);
    C_VAR_INT("rend-sprite-lights", &spriteLight, 0, 0, 10);
    C_VAR_BYTE("rend-sprite-mode", &noSpriteTrans, 0, 0, 1);
    C_VAR_INT("rend-sprite-noz", &noSpriteZWrite, 0, 0, 1);
    C_VAR_BYTE("rend-sprite-precache", &precacheSprites, 0, 0, 1);
}

static __inline void renderQuad(gl_vertex_t *v, gl_color_t *c,
                                gl_texcoord_t *tc)
{
    DGL_Begin(DGL_QUADS);
        DGL_Color4ubv(c[0].rgba);
        DGL_TexCoord2fv(tc[0].st);
        DGL_Vertex3fv(v[0].xyz);

        DGL_Color4ubv(c[1].rgba);
        DGL_TexCoord2fv(tc[1].st);
        DGL_Vertex3fv(v[1].xyz);

        DGL_Color4ubv(c[2].rgba);
        DGL_TexCoord2fv(tc[2].st);
        DGL_Vertex3fv(v[2].xyz);

        DGL_Color4ubv(c[3].rgba);
        DGL_TexCoord2fv(tc[3].st);
        DGL_Vertex3fv(v[3].xyz);
    DGL_End();
}

/**
 * Fog is turned off while rendering. It's not feasible to think that the
 * fog would obstruct the player's view of his own weapon.
 */
void Rend_Draw3DPlayerSprites(void)
{
    int                 i;

    // Setup the modelview matrix.
    Rend_ModelViewMatrix(false);

    // Clear Z buffer. This will prevent the psprites from being clipped
    // by nearby polygons.
    glClear(GL_DEPTH_BUFFER_BIT);

    // Turn off fog.
    if(usingFog)
        DGL_Disable(DGL_FOG);

    for(i = 0; i < DDMAXPSPRITES; ++i)
    {
        vispsprite_t*       spr = &visPSprites[i];

        if(spr->type != VPSPR_MODEL)
            continue; // Not used.

        {
        rendmodelparams_t   params;
        setupModelParamsForVisPSprite(&params, spr);
        Rend_RenderModel(&params);
        }
    }

    // Should we turn the fog back on?
    if(usingFog)
        DGL_Enable(DGL_FOG);
}

/**
 * Set all the colors in the array to that specified.
 */
void Spr_UniformVertexColors(int count, gl_color_t *colors,
                             const float *rgba)
{
    for(; count-- > 0; colors++)
    {
        colors->rgba[CR] = (byte) (255 * rgba[CR]);
        colors->rgba[CG] = (byte) (255 * rgba[CG]);
        colors->rgba[CB] = (byte) (255 * rgba[CB]);
        colors->rgba[CA] = (byte) (255 * rgba[CA]);
    }
}

typedef struct {
    float               color[3], extra[3];
    gl_vertex_t*        normal;
} lightspriteparams_t;

static boolean lightSprite(const vlight_t* vlight, void* context)
{
    float*              dest;
    float               dot;
    lightspriteparams_t* params = (lightspriteparams_t*) context;

    dot = DOTPROD(vlight->vector, params->normal->xyz);
    dot += vlight->offset; // Shift a bit towards the light.

    if(!vlight->affectedByAmbient)
    {   // Won't be affected by ambient.
        dest = params->extra;
    }
    else
    {
        dest = params->color;
    }

    // Ability to both light and shade.
    if(dot > 0)
    {
        dot *= vlight->lightSide;
    }
    else
    {
        dot *= vlight->darkSide;
    }

    dot = MINMAX_OF(-1, dot, 1);

    dest[CR] += dot * vlight->color[CR];
    dest[CG] += dot * vlight->color[CG];
    dest[CB] += dot * vlight->color[CB];

    return true; // Continue iteration.
}

/**
 * Calculate vertex lighting.
 */
void Spr_VertexColors(int count, gl_color_t *out, gl_vertex_t *normal,
                      uint vLightListIdx, const float* ambient)
{
    int                 i, k;
    lightspriteparams_t params;

    for(i = 0; i < count; ++i, out++, normal++)
    {
        // Begin with total darkness.
        params.color[CR] = params.color[CG] = params.color[CB] = 0;
        params.extra[CR] = params.extra[CG] = params.extra[CB] = 0;
        params.normal = normal;

        VL_ListIterator(vLightListIdx, &params, lightSprite);

        // Check for ambient and convert to ubyte.
        for(k = 0; k < 3; ++k)
        {
            if(params.color[k] < ambient[k])
                params.color[k] = ambient[k];
            params.color[k] += params.extra[k];
            params.color[k] = MINMAX_OF(0, params.color[k], 1);

            // This is the final color.
            out->rgba[k] = (byte) (255 * params.color[k]);
        }

        out->rgba[CA] = (byte) (255 * ambient[CA]);
    }
}

static void setupPSpriteParams(rendpspriteparams_t* params,
                               vispsprite_t* spr)
{
    float               offScaleY = weaponOffsetScaleY / 1000.0f;
    spritetex_t*        sprTex;
    spritedef_t*        sprDef;
    ddpsprite_t*        psp = spr->psp;
    int                 sprite = psp->statePtr->sprite;
    int                 frame = psp->statePtr->frame;
    int                 idx;
    boolean             flip;
    short               width, height;
    spriteframe_t*      sprFrame;
    material_t*         mat;

#ifdef RANGECHECK
    if((unsigned) sprite >= (unsigned) numSprites)
        Con_Error("R_GetSpriteInfo: invalid sprite number %i.\n", sprite);
#endif

    sprDef = &sprites[sprite];
#ifdef RANGECHECK
    if(frame >= sprDef->numFrames)
        Con_Error("setupPSpriteParams: Invalid frame number %i for sprite %i",
                  frame, sprite);
#endif

    sprFrame = &sprDef->spriteFrames[frame];
    mat = sprFrame->mats[0];
    idx = mat->ofTypeID;
    flip = sprFrame->flip[0];
    width = mat->width;
    height = mat->height;

    sprTex = spriteTextures[idx];

    params->pos[VX] = psp->pos[VX] - sprTex->offX + pspOffset[VX];
    params->pos[VY] = offScaleY * psp->pos[VY] +
        (1 - offScaleY) * 32 - sprTex->offY + pspOffset[VY];
    params->width = width;
    params->height = height;

    // Let's calculate texture coordinates.
    // To remove a possible edge artifact, move the corner a bit up/left.
    params->texOffset[0] =
        sprTex->texCoord[VX] - 0.4f / M_CeilPow2(params->width);
    params->texOffset[1] =
        sprTex->texCoord[VY] - 0.4f / M_CeilPow2(params->height);

    params->texFlip[0] = flip;
    params->texFlip[1] = false;

    params->mat = mat;
    params->ambientColor[CA] = spr->data.sprite.alpha;

    if(spr->data.sprite.isFullBright)
    {
        params->ambientColor[CR] = params->ambientColor[CG] =
            params->ambientColor[CB] = 1;
        params->vLightListIdx = 0;
    }
    else
    {
        collectaffectinglights_params_t lparams;

        if(useBias)
        {
            // Evaluate the position in the light grid.
            LG_Evaluate(spr->center, params->ambientColor);
        }
        else
        {
            float               lightLevel;
            const float*        secColor =
                R_GetSectorLightColor(spr->data.sprite.subsector->sector);

            if(spr->psp->light < 1)
                lightLevel = (spr->psp->light - .1f);
            else
                lightLevel = 1;

            // No need for distance attentuation.

            // Add extra light.
            lightLevel += R_ExtraLightDelta();

            Rend_ApplyLightAdaptation(&lightLevel);

            // Determine the final ambientColor in affect.
            params->ambientColor[CR] = lightLevel * secColor[CR];
            params->ambientColor[CG] = lightLevel * secColor[CG];
            params->ambientColor[CB] = lightLevel * secColor[CB];
        }

        Rend_ApplyTorchLight(params->ambientColor, 0);

        lparams.starkLight = false;
        lparams.center[VX] = spr->center[VX];
        lparams.center[VY] = spr->center[VY];
        lparams.center[VZ] = spr->center[VZ];
        lparams.maxLights = spriteLight;
        lparams.subsector = spr->data.sprite.subsector;
        lparams.ambientColor = params->ambientColor;

        params->vLightListIdx = R_CollectAffectingLights(&lparams);
    }
}

void Rend_DrawPSprite(const rendpspriteparams_t *params)
{
    int                 i;
    float               v1[2], v2[2], v3[2], v4[2];
    gl_color_t          quadColors[4];
    gl_vertex_t         quadNormals[4];

    if(renderTextures == 1)
        GL_SetPSprite(R_GetMaterialNum(params->mat));
    else if(renderTextures == 2)
        // For lighting debug, render all solid surfaces using the gray texture.
        GL_BindTexture(GL_PrepareMaterial(R_GetMaterial(DDT_GRAY, MAT_DDTEX)),
                       filterSprites? DGL_LINEAR : DGL_NEAREST);
    else
        DGL_Bind(0);

    //  0---1
    //  |   |  Vertex layout.
    //  3---2

    v1[VX] = params->pos[VX];
    v1[VY] = params->pos[VY];

    v2[VX] = params->pos[VX] + params->width;
    v2[VY] = params->pos[VY];

    v3[VX] = params->pos[VX] + params->width;
    v3[VY] = params->pos[VY] + params->height;

    v4[VX] = params->pos[VX];
    v4[VY] = params->pos[VY] + params->height;

    // All psprite vertices are co-plannar, so just copy the view front vector.
    // \fixme: Can we do something better here?
    for(i = 0; i < 4; ++i)
    {
        quadNormals[i].xyz[VX] = viewFrontVec[VX];
        quadNormals[i].xyz[VY] = viewFrontVec[VZ];
        quadNormals[i].xyz[VZ] = viewFrontVec[VY];
    }

    if(!params->vLightListIdx)
    {   // Lit uniformly.
        Spr_UniformVertexColors(4, quadColors, params->ambientColor);
    }
    else
    {   // Lit normally.
        Spr_VertexColors(4, quadColors, quadNormals, params->vLightListIdx,
                         params->ambientColor);
    }

    {
    gl_texcoord_t   tcs[4], *tc = tcs;
    gl_color_t     *c = quadColors;

    tc[0].st[0] = params->texOffset[0] *  (params->texFlip[0]? 1:0);
    tc[0].st[1] = params->texOffset[1] *  (params->texFlip[1]? 1:0);
    tc[1].st[0] = params->texOffset[0] * (!params->texFlip[0]? 1:0);
    tc[1].st[1] = params->texOffset[1] *  (params->texFlip[1]? 1:0);
    tc[2].st[0] = params->texOffset[0] * (!params->texFlip[0]? 1:0);
    tc[2].st[1] = params->texOffset[1] * (!params->texFlip[1]? 1:0);
    tc[3].st[0] = params->texOffset[0] *  (params->texFlip[0]? 1:0);
    tc[3].st[1] = params->texOffset[1] * (!params->texFlip[1]? 1:0);

    DGL_Begin(DGL_QUADS);
        DGL_Color4ubv(c[0].rgba);
        DGL_TexCoord2fv(tc[0].st);
        DGL_Vertex2fv(v1);

        DGL_Color4ubv(c[1].rgba);
        DGL_TexCoord2fv(tc[1].st);
        DGL_Vertex2fv(v2);

        DGL_Color4ubv(c[2].rgba);
        DGL_TexCoord2fv(tc[2].st);
        DGL_Vertex2fv(v3);

        DGL_Color4ubv(c[3].rgba);
        DGL_TexCoord2fv(tc[3].st);
        DGL_Vertex2fv(v4);
    DGL_End();
    }
}

/**
 * Draws 2D HUD sprites.
 *
 * \note If they were already drawn 3D, this won't do anything.
 */
void Rend_Draw2DPlayerSprites(void)
{
    int                 i;
    ddplayer_t         *ddpl = &viewPlayer->shared;
    ddpsprite_t        *psp;

    // Cameramen have no HUD sprites.
    if((ddpl->flags & DDPF_CAMERA) || (ddpl->flags & DDPF_CHASECAM))
        return;

    // Check for fullbright.
    for(i = 0, psp = ddpl->pSprites; i < DDMAXPSPRITES; ++i, psp++)
    {
        vispsprite_t        *spr = &visPSprites[i];

        // Should this psprite be drawn?
        if(spr->type != VPSPR_SPRITE)
            continue; // No...

        // Draw as separate sprites.
        if(spr->psp && spr->psp->statePtr)
        {
            rendpspriteparams_t params;

            setupPSpriteParams(&params, spr);
            Rend_DrawPSprite(&params);
        }
    }
}

/**
 * A sort of a sprite, I guess... Masked walls must be rendered sorted
 * with sprites, so no artifacts appear when sprites are seen behind
 * masked walls.
 */
void Rend_RenderMaskedWall(rendmaskedwallparams_t *params)
{
    boolean     withDyn = false;
    int         normal = 0, dyn = 1;

    // Do we have a dynamic light to blend with?
    // This only happens when multitexturing is enabled.
    if(params->modTex)
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

        RL_SelectTexUnits(2);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, IS_MUL ? 4 : 5);

        // The dynamic light.
        RL_BindTo(IS_MUL ? 0 : 1, params->modTex, DGL_LINEAR);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, params->modColor);

        // The actual texture.
        RL_BindTo(IS_MUL ? 1 : 0, params->texture, glmode[texMagMode]);

        withDyn = true;
    }
    else
    {
        RL_SelectTexUnits(1);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);

        RL_Bind(params->texture, glmode[texMagMode]);
        normal = 0;
    }

    // Masked walls are sometimes used for special effects like arcs,
    // cobwebs and bottoms of sails. In order for them to look right,
    // we need to disable texture wrapping on the horizontal axis (S).
    // Most masked walls need wrapping, though. What we need to do is
    // look at the texture coordinates and see if they require texture
    // wrapping.
    if(params->masked)
    {
        if(withDyn)
        {
            DGL_SetInteger(DGL_ACTIVE_TEXTURE, IS_MUL ? 1 : 0);
        }

        if(params->texCoord[0][VX] < 0 || params->texCoord[0][VX] > 1 ||
           params->texCoord[1][VX] < 0 || params->texCoord[1][VX] > 1)
        {
            // The texcoords are out of the normal [0,1] range.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            GL_REPEAT);
        }
        else
        {
            // Visible portion is within the actual [0,1] range.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);
        }
    }
    GL_BlendMode(params->blendMode);

    // Draw one quad. This is obviously not a very efficient way to render
    // lots of masked walls, but since 3D models and sprites must be
    // rendered interleaved with masked walls, there's not much that can be
    // done about this.
    if(withDyn)
    {
        DGL_Begin(DGL_QUADS);
        DGL_Color4fv(params->vertices[0].color);
        DGL_MultiTexCoord2f(normal, params->texCoord[0][0],
                            params->texCoord[1][1]);

        DGL_MultiTexCoord2f(dyn, params->modTexCoord[0][0],
                            params->modTexCoord[1][1]);

        DGL_Vertex3f(params->vertices[0].pos[VX],
                     params->vertices[0].pos[VZ],
                     params->vertices[0].pos[VY]);

        DGL_Color4fv(params->vertices[1].color);
        DGL_MultiTexCoord2fv(normal, params->texCoord[0]);

        DGL_MultiTexCoord2f(dyn, params->modTexCoord[0][0],
                            params->modTexCoord[1][0]);

        DGL_Vertex3f(params->vertices[1].pos[VX],
                     params->vertices[1].pos[VZ],
                     params->vertices[1].pos[VY]);

        DGL_Color4fv(params->vertices[3].color);
        DGL_MultiTexCoord2f(normal, params->texCoord[1][0],
                            params->texCoord[0][1]);

        DGL_MultiTexCoord2f(dyn, params->modTexCoord[0][1],
                            params->modTexCoord[1][0]);

        DGL_Vertex3f(params->vertices[3].pos[VX],
                     params->vertices[3].pos[VZ],
                     params->vertices[3].pos[VY]);

        DGL_Color4fv(params->vertices[2].color);
        DGL_MultiTexCoord2fv(normal, params->texCoord[1]);

        DGL_MultiTexCoord2f(dyn, params->modTexCoord[0][1],
                            params->modTexCoord[1][1]);

        DGL_Vertex3f(params->vertices[2].pos[VX],
                     params->vertices[2].pos[VZ],
                     params->vertices[2].pos[VY]);
        DGL_End();

        // Restore normal GL state.
        RL_SelectTexUnits(1);
        DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);
        DGL_DisableArrays(true, true, 0x1);
    }
    else
    {
        DGL_Begin(DGL_QUADS);
        DGL_Color4fv(params->vertices[0].color);
        DGL_MultiTexCoord2f(normal, params->texCoord[0][0],
                           params->texCoord[1][1]);

        DGL_Vertex3f(params->vertices[0].pos[VX],
                    params->vertices[0].pos[VZ],
                    params->vertices[0].pos[VY]);

        DGL_Color4fv(params->vertices[1].color);
        DGL_MultiTexCoord2fv(normal, params->texCoord[0]);

        DGL_Vertex3f(params->vertices[1].pos[VX],
                    params->vertices[1].pos[VZ],
                    params->vertices[1].pos[VY]);

        DGL_Color4fv(params->vertices[3].color);
        DGL_MultiTexCoord2f(normal, params->texCoord[1][0],
                           params->texCoord[0][1]);

        DGL_Vertex3f(params->vertices[3].pos[VX],
                    params->vertices[3].pos[VZ],
                    params->vertices[3].pos[VY]);

        DGL_Color4fv(params->vertices[2].color);
        DGL_MultiTexCoord2fv(normal, params->texCoord[1]);

        DGL_Vertex3f(params->vertices[2].pos[VX],
                    params->vertices[2].pos[VZ],
                    params->vertices[2].pos[VY]);
        DGL_End();
    }

    GL_BlendMode(BM_NORMAL);
}

static void setupModelParamsForVisPSprite(rendmodelparams_t* params,
                                          vispsprite_t* spr)
{
    params->mf = spr->data.model.mf;
    params->nextMF = spr->data.model.nextMF;
    params->inter = spr->data.model.inter;
    params->alwaysInterpolate = false;
    params->id = spr->data.model.id;
    params->selector = spr->data.model.selector;
    params->flags = spr->data.model.flags;
    params->center[VX] = spr->center[VX];
    params->center[VY] = spr->center[VY];
    params->center[VZ] = spr->center[VZ];
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
       !(spr->data.model.mf->sub[0].flags & MFF_DIM))
    {
        params->ambientColor[CR] = params->ambientColor[CG] =
            params->ambientColor[CB] = 1;
        params->vLightListIdx = 0;
    }
    else
    {
        collectaffectinglights_params_t lparams;

        if(useBias)
        {
            LG_Evaluate(params->center, params->ambientColor);
        }
        else
        {
            float               lightLevel;
            const float*        secColor =
                R_GetSectorLightColor(spr->data.model.subsector->sector);

            // Diminished light (with compression).
            lightLevel = spr->data.model.subsector->sector->lightLevel;

            // No need for distance attentuation.

            // Add extra light.
            lightLevel += R_ExtraLightDelta();

            // The last step is to compress the resultant light value by
            // the global lighting function.
            Rend_ApplyLightAdaptation(&lightLevel);

            // Determine the final ambientColor in effect.
            params->ambientColor[CR] = lightLevel * secColor[CR];
            params->ambientColor[CG] = lightLevel * secColor[CG];
            params->ambientColor[CB] = lightLevel * secColor[CB];
        }

        Rend_ApplyTorchLight(params->ambientColor, params->distance);

        lparams.starkLight = true;
        lparams.center[VX] = spr->center[VX];
        lparams.center[VY] = spr->center[VY];
        lparams.center[VZ] = spr->center[VZ];
        lparams.maxLights = modelLight;
        lparams.subsector = spr->data.model.subsector;
        lparams.ambientColor = params->ambientColor;

        params->vLightListIdx = R_CollectAffectingLights(&lparams);
    }
}

static boolean generateHaloForVisSprite(vissprite_t* spr, boolean primary)
{
    lumobj_t*           lum = LO_GetLuminous(spr->lumIdx);
    vec3_t              center;
    float               occlusionFactor, flareMul;

    if(LUM_OMNI(lum)->flags & LUMOF_NOHALO)
        return false;

    V3_Copy(center, spr->center);
    center[VZ] += LUM_OMNI(lum)->zOff;

    if(spr->type == VSPR_SPRITE)
    {
        V3_Sum(center, center, spr->data.sprite.srvo);
    }
    else if(spr->type == VSPR_MODEL)
    {
        V3_Sum(center, center, spr->data.model.srvo);
    }

    flareMul = LUM_OMNI(lum)->flareMul;

    /**
     * \kludge surface decorations do not yet persist over frames,
     * thus we do not smoothly occlude their halos. Instead, we will
     * have to put up with them instantly appearing/disappearing.
     */
    if(spr->isDecoration)
    {
        if(LO_IsClipped(spr->lumIdx, viewPlayer - ddPlayers))
            occlusionFactor = 0;
        else
            occlusionFactor = 1;

        flareMul *= Rend_DecorSurfaceAngleHaloMul(lum->decorSource);
    }
    else
    {
        byte                haloFactor = (LUM_OMNI(lum)->haloFactors?
            LUM_OMNI(lum)->haloFactors[viewPlayer - ddPlayers] : 0xff);

        occlusionFactor = (haloFactor & 0x7f) / 127.0f;
    }

    return H_RenderHalo(center[VX], center[VY], center[VZ],
                        LUM_OMNI(lum)->flareSize,
                        LUM_OMNI(lum)->flareTex, LUM_OMNI(lum)->flareCustom,
                        LUM_OMNI(lum)->color,
                        LO_DistanceToViewer(spr->lumIdx, viewPlayer - ddPlayers),
                        occlusionFactor, flareMul,
                        LUM_OMNI(lum)->xOff, primary,
                        (LUM_OMNI(lum)->flags & LUMOF_DONTTURNHALO));
}

/**
 * Render sprites, 3D models, masked wall segments and halos, ordered
 * back to front. Halos are rendered with Z-buffer tests and writes
 * disabled, so they don't go into walls or interfere with real objects.
 * It means that halos can be partly occluded by objects that are closer
 * to the viewpoint, but that's the price to pay for not having access to
 * the actual Z-buffer per-pixel depth information. The other option would
 * be for halos to shine through masked walls, sprites and models, which
 * looks even worse. (Plus, they are *halos*, not real lens flares...)
 */
void Rend_DrawMasked(void)
{
    boolean             haloDrawn = false;
    vissprite_t*        spr;

    if(!willRenderSprites)
        return;

    R_SortVisSprites();

    if(visSpriteP > visSprites)
    {
        // Draw all vissprites back to front.
        // Sprites look better with Z buffer writes turned off.
        for(spr = visSprSortedHead.next; spr != &visSprSortedHead; spr = spr->next)
        {
            switch(spr->type)
            {
            case VSPR_HIDDEN: // Fall through.
            default:
                break;

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
            }

            // How about a halo?
            if(spr->lumIdx)
            {
                if(generateHaloForVisSprite(spr, true) && !haloDrawn)
                    haloDrawn = true;
            }
        }

        // Draw secondary halos.
        if(haloDrawn && haloMode > 1)
        {
            // Now we can setup the state only once.
            H_SetupState(true);

            for(spr = visSprSortedHead.next; spr != &visSprSortedHead;
                spr = spr->next)
            {
                if(spr->lumIdx)
                {
                    generateHaloForVisSprite(spr, false);
                }
            }

            // And we're done...
            H_SetupState(false);
        }
    }
}

void Rend_RenderSprite(const rendspriteparams_t* params)
{
    int                 i;
    gl_color_t          quadColors[4];
    gl_vertex_t         quadNormals[4];
    boolean             restoreMatrix = false;
    boolean             restoreZ = false;
    float               spriteCenter[3];
    float               surfaceNormal[3];
    float               v1[3], v2[3], v3[3], v4[3];

    if(renderTextures == 1)
    {
        // Do we need to translate any of the colors?
        if(params->tMap)
        {   // We need to prepare a translated version of the sprite.
            GL_SetTranslatedSprite(R_GetMaterialNum(params->mat), params->tMap,
                                   params->tClass);
        }
        else
        {   // Set the texture. No translation required.
            GL_BindTexture(GL_PrepareMaterial(params->mat),
                           filterSprites? DGL_LINEAR : DGL_NEAREST);
        }
    }
    else if(renderTextures == 2)
        // For lighting debug, render all solid surfaces using the gray texture.
        GL_BindTexture(GL_PrepareMaterial(R_GetMaterial(DDT_GRAY, MAT_DDTEX)),
                       filterSprites? DGL_LINEAR : DGL_NEAREST);
    else
        DGL_Bind(0);

    // Coordinates to the center of the sprite (game coords).
    spriteCenter[VX] = params->center[VX] + params->srvo[VX];
    spriteCenter[VY] = params->center[VY] + params->srvo[VY];
    spriteCenter[VZ] = params->center[VZ] + params->srvo[VZ];

    M_ProjectViewRelativeLine2D(spriteCenter, params->viewAligned,
                                params->width, params->viewOffX, v1, v4);

    v2[VX] = v1[VX];
    v2[VY] = v1[VY];
    v3[VX] = v4[VX];
    v3[VY] = v4[VY];

    v1[VZ] = v4[VZ] = spriteCenter[VZ] - params->height / 2;
    v2[VZ] = v3[VZ] = spriteCenter[VZ] + params->height / 2;

    // Calculate the surface normal.
    M_PointCrossProduct(v2, v1, v3, surfaceNormal);
    M_Normalize(surfaceNormal);

/*
// Draw the surface normal.
DGL_Disable(DGL_TEXTURING);
DGL_Begin(DGL_LINES);
DGL_Color4f(1, 0, 0, 1);
DGL_Vertex3f(spriteCenter[VX], spriteCenter[VZ], spriteCenter[VY]);
DGL_Color4f(1, 0, 0, 0);
DGL_Vertex3f(spriteCenter[VX] + surfaceNormal[VX] * 10,
             spriteCenter[VZ] + surfaceNormal[VZ] * 10,
             spriteCenter[VY] + surfaceNormal[VY] * 10);
DGL_End();
DGL_Enable(DGL_TEXTURING);
*/

    // All sprite vertices are co-plannar, so just copy the surface normal.
    // \fixme: Can we do something better here?
    for(i = 0; i < 4; ++i)
        memcpy(quadNormals[i].xyz, surfaceNormal, sizeof(surfaceNormal));

    if(!params->vLightListIdx)
    {   // Lit uniformly.
        Spr_UniformVertexColors(4, quadColors, params->ambientColor);
    }
    else
    {   // Lit normally.
        Spr_VertexColors(4, quadColors, quadNormals, params->vLightListIdx,
                         params->ambientColor);
    }

    // Do we need to do some aligning?
    if(params->viewAligned || alwaysAlign >= 2)
    {
        // We must set up a modelview transformation matrix.
        restoreMatrix = true;
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();

        // Rotate around the center of the sprite.
        DGL_Translatef(spriteCenter[VX], spriteCenter[VZ], spriteCenter[VY]);
        if(!params->viewAligned)
        {
            float   s_dx = v1[VX] - v2[VX];
            float   s_dy = v1[VY] - v2[VY];

            if(alwaysAlign == 2)
            {   // Restricted camera alignment.
                float   dx = spriteCenter[VX] - vx;
                float   dy = spriteCenter[VY] - vz;
                float   spriteAngle =
                    BANG2DEG(bamsAtan2(spriteCenter[VZ] - vy,
                                       sqrt(dx * dx + dy * dy)));

                if(spriteAngle > 180)
                    spriteAngle -= 360;

                if(fabs(spriteAngle) > maxSpriteAngle)
                {
                    float   turnAngle =
                        (spriteAngle >
                        0 ? spriteAngle - maxSpriteAngle : spriteAngle +
                        maxSpriteAngle);

                    // Rotate along the sprite edge.
                    DGL_Rotatef(turnAngle, s_dx, 0, s_dy);
                }
            }
            else
            {   // Restricted view plane alignment.
                // This'll do, for now... Really it should notice both the
                // sprite angle and vpitch.
                DGL_Rotatef(vpitch * .5f, s_dx, 0, s_dy);
            }
        }
        else
        {
            // Normal rotation perpendicular to the view plane.
            DGL_Rotatef(vpitch, viewsidex, 0, viewsidey);
        }
        DGL_Translatef(-spriteCenter[VX], -spriteCenter[VZ], -spriteCenter[VY]);
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

    {
    gl_vertex_t     vs[4], *v = vs;
    gl_texcoord_t   tcs[4], *tc = tcs;

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

    tc[0].st[0] = params->matOffset[0] *  (params->matFlip[0]? 1:0);
    tc[0].st[1] = params->matOffset[1] * (!params->matFlip[1]? 1:0);
    tc[1].st[0] = params->matOffset[0] *  (params->matFlip[0]? 1:0);
    tc[1].st[1] = params->matOffset[1] *  (params->matFlip[1]? 1:0);
    tc[2].st[0] = params->matOffset[0] * (!params->matFlip[0]? 1:0);
    tc[2].st[1] = params->matOffset[1] *  (params->matFlip[1]? 1:0);
    tc[3].st[0] = params->matOffset[0] * (!params->matFlip[0]? 1:0);
    tc[3].st[1] = params->matOffset[1] * (!params->matFlip[1]? 1:0);

    renderQuad(v, quadColors, tc);
    }

    // Need to restore the original modelview matrix?
    if(restoreMatrix)
        DGL_PopMatrix();

    // Change back to normal blending?
    if(params->blendMode != BM_NORMAL)
        GL_BlendMode(BM_NORMAL);

    // Enable Z-writing again?
    if(restoreZ)
        glDepthMask(GL_TRUE);
}
