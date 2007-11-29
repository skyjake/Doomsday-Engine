/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

static void setupSpriteParamsForVisSprite(rendspriteparams_t *params,
                                          vissprite_t *spr);
static void setupPSpriteParamsForVisSprite(rendpspriteparams_t *params,
                                           vissprite_t *spr);
static void setupMaskedWallParamsForVisSprite(rendmaskedwallparams_t *params,
                                              vissprite_t *spr);
static void setupModelParamsForVisSprite(modelparams_t *params,
                                              vissprite_t *spr);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean willRenderSprites;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     spriteLight = 4;
float   maxSpriteAngle = 60;

// If true - use the "no translucency" blending mode for sprites/masked walls
byte    noSpriteTrans = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int useSpriteAlpha = 1;

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
    gl.Begin(DGL_QUADS);
        gl.Color4ubv(c[0].rgba);
        gl.TexCoord2fv(tc[0].st);
        gl.Vertex3fv(v[0].xyz);

        gl.Color4ubv(c[1].rgba);
        gl.TexCoord2fv(tc[1].st);
        gl.Vertex3fv(v[1].xyz);

        gl.Color4ubv(c[2].rgba);
        gl.TexCoord2fv(tc[2].st);
        gl.Vertex3fv(v[2].xyz);

        gl.Color4ubv(c[3].rgba);
        gl.TexCoord2fv(tc[3].st);
        gl.Vertex3fv(v[3].xyz);
    gl.End();
}

/**
 * Fog is turned off while rendering. It's not feasible to think that the
 * fog would obstruct the player's view of his own weapon.
 */
void Rend_Draw3DPlayerSprites(void)
{
    int         i;
    modelparams_t params;

    // Setup the modelview matrix.
    Rend_ModelViewMatrix(false);

    // Clear Z buffer. This will prevent the psprites from being clipped
    // by nearby polygons.
    gl.Clear(DGL_DEPTH_BUFFER_BIT);

    // Turn off fog.
    if(usingFog)
        gl.Disable(DGL_FOG);

    for(i = 0; i < DDMAXPSPRITES; ++i)
    {
        if(!vispsprites[i].type)
            continue;           // Not used.

        setupModelParamsForVisSprite(&params, vispsprites + i);
        Rend_RenderModel(&params);
    }

    // Should we turn the fog back on?
    if(usingFog)
        gl.Enable(DGL_FOG);
}

/**
 * Set all the colors in the array to bright white.
 */
void Spr_FullBrightVertexColors(int count, gl_color_t *colors, float alpha)
{
    for(; count-- > 0; colors++)
    {
        memset(colors->rgba, 255, 3);
        colors->rgba[3] = (byte) (255 * alpha);
    }
}

/**
 * Calculate vertex lighting.
 */
void Spr_VertexColors(int count, gl_color_t *out, gl_vertex_t *normal,
                      int numLights, vlight_t *lights,
                      float ambient[4])
{
    int         i, k;
    float       color[3], extra[3], *dest, dot;
    vlight_t   *light;

    for(i = 0; i < count; ++i, out++, normal++)
    {
        // Begin with total darkness.
        memset(color, 0, sizeof(color));
        memset(extra, 0, sizeof(extra));

        // Add light from each source.
        for(k = 0, light = lights; k < numLights; ++k, light++)
        {
            if(!light->used)
                continue;

            dot = DOTPROD(light->vector, normal->xyz);
            if(light->lum)
            {
                dest = color;
            }
            else
            {
                // This is world light (won't be affected by ambient).
                // Ability to both light and shade.
                dest = extra;
                dot += light->offset;   // Shift a bit towards the light.
                if(dot > 0)
                    dot *= light->lightSide;
                else
                    dot *= light->darkSide;
            }

            // No light from the wrong side.
            if(dot <= 0)
            {
                // Lights with a source won't shade anything.
                if(light->lum)
                    continue;
                if(dot < -1)
                    dot = -1;
            }
            else
            {
                if(dot > 1)
                    dot = 1;
            }

            dest[0] += dot * light->color[0];
            dest[1] += dot * light->color[1];
            dest[2] += dot * light->color[2];
        }

        // Check for ambient and convert to ubyte.
        for(k = 0; k < 3; ++k)
        {
            if(color[k] < ambient[k])
                color[k] = ambient[k];
            color[k] += extra[k];
            if(color[k] < 0)
                color[k] = 0;
            if(color[k] > 1)
                color[k] = 1;

            // This is the final color.
            out->rgba[k] = (byte) (255 * color[k]);
        }
        out->rgba[CA] = (byte) (255 * ambient[CA]);
    }
}

static void setupPSpriteParamsForVisSprite(rendpspriteparams_t *params,
                                           vissprite_t *spr)
{
    float       offScaleY = weaponOffsetScaleY / 1000.0f;
    spritelump_t *slump;
    spriteinfo_t info;
    ddpsprite_t *psp = spr->data.psprite.psp;
    visspritelightparams_t lparams;

    // Clear sprite info.
    memset(&info, 0, sizeof(info));
    R_GetSpriteInfo(psp->stateptr->sprite, psp->stateptr->frame,
                    &info);
    slump = spritelumps[info.lump];

    params->pos[VX] = psp->pos[VX] - info.offset + pspOffset[VX];
    params->pos[VY] =
        offScaleY * psp->pos[VY] + (1 - offScaleY) * 32 - info.topOffset + pspOffset[VY];
    params->width = slump->width;
    params->height = slump->height;
    params->subsector = spr->data.psprite.subsector;

    // Let's calculate texture coordinates.
    // To remove a possible edge artifact, move the corner a bit up/left.
    params->texOffset[0] =
        slump->tc[1][VX] - 0.4f / M_CeilPow2(params->width);
    params->texOffset[1] =
        slump->tc[1][VY] - 0.4f / M_CeilPow2(params->height);

    params->texFlip[0] = (boolean) info.flip;
    params->texFlip[1] = false;

    params->lump = info.lump;

    memcpy(params->rgba, spr->data.psprite.rgb, sizeof(float) * 3);
    params->rgba[CA] = spr->data.psprite.alpha;
    params->lightLevel = spr->data.psprite.lightlevel;

    lparams.starkLight = true;
    memcpy(lparams.center, spr->center, sizeof(lparams.center));
    lparams.maxLights = spriteLight;
    lparams.subsector = params->subsector;

    R_SetAmbientColor(params->rgba, params->lightLevel, spr->distance);
    R_DetermineLightsAffectingVisSprite(&lparams, &params->lights, &params->numLights);
}

void Rend_DrawPSprite(const rendpspriteparams_t *params)
{
    int         i, pSprMode = 1;
    float       v1[2], v2[2], v3[2], v4[2];
    gl_color_t  quadColors[4];
    gl_vertex_t quadNormals[4];
    spritelump_t *slump = spritelumps[params->lump];

    if(renderTextures == 1)
        GL_SetSprite(params->lump, pSprMode);
    else if(renderTextures == 2)
        // For lighting debug, render all solid surfaces using the gray texture.
        GL_BindTexture(GL_PrepareMaterial(DDT_GRAY, MAT_DDTEX, NULL));
    else
        gl.Bind(0);

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

    if(params->lightLevel < 0)
    {   // Fullbright white.
        Spr_FullBrightVertexColors(4, quadColors, params->rgba[CA]);
    }
    else
    {   // Lit normally.
        extern float ambientColor[3];

        float       ambient[4];

        memcpy(ambient, ambientColor, sizeof(float) * 3);
        ambient[CA] = params->rgba[CA];

        Spr_VertexColors(4, quadColors, quadNormals,
                         params->numLights, params->lights, ambient);
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

    gl.Begin(DGL_QUADS);
        gl.Color4ubv(c[0].rgba);
        gl.TexCoord2fv(tc[0].st);
        gl.Vertex2fv(v1);

        gl.Color4ubv(c[1].rgba);
        gl.TexCoord2fv(tc[1].st);
        gl.Vertex2fv(v2);

        gl.Color4ubv(c[2].rgba);
        gl.TexCoord2fv(tc[2].st);
        gl.Vertex2fv(v3);

        gl.Color4ubv(c[3].rgba);
        gl.TexCoord2fv(tc[3].st);
        gl.Vertex2fv(v4);
    gl.End();
    }
}

/**
 * Draws 2D player sprites. If they were already drawn 3D, this
 * won't do anything.
 */
void Rend_DrawPlayerSprites(void)
{
    int         i;
    ddpsprite_t *psp;
    vissprite_t *vis;

    // Cameramen have no psprites.
    if((viewPlayer->flags & DDPF_CAMERA) || (viewPlayer->flags & DDPF_CHASECAM))
        return;

    // Check for fullbright.
    for(i = 0, psp = viewPlayer->psprites; i < DDMAXPSPRITES; ++i, psp++)
    {
        vis = vispsprites + i;

        // Should this psprite be drawn?
        if(vis->type != VSPR_HUD_SPRITE)
            continue; // No...

        // Draw as separate sprites.
        {
        rendpspriteparams_t params;

        setupPSpriteParamsForVisSprite(&params, vis);
        Rend_DrawPSprite(&params);
        }
    }
}

static void setupMaskedWallParamsForVisSprite(rendmaskedwallparams_t *params,
                                              vissprite_t *spr)
{
    memcpy(params, &spr->data.wall, sizeof(params));
}

/**
 * A sort of a sprite, I guess... Masked walls must be rendered sorted
 * with sprites, so no artifacts appear when sprites are seen behind
 * masked walls.
 */
void Rend_RenderMaskedWall(rendmaskedwallparams_t *params)
{
    boolean     withDyn = false;
    int         normal, dyn = DGL_TEXTURE1;

    // Do we have a dynamic light to blend with?
    // This only happens when multitexturing is enabled.
    if(params->modTex)
    {
        if(IS_MUL)
        {
            normal = DGL_TEXTURE1;
            dyn = DGL_TEXTURE0;
        }
        else
        {
            normal = DGL_TEXTURE0;
            dyn = DGL_TEXTURE1;
        }

        RL_SelectTexUnits(2);
        gl.SetInteger(DGL_MODULATE_TEXTURE, IS_MUL ? 4 : 5);

        // The dynamic light.
        RL_BindTo(IS_MUL ? 0 : 1, params->modTex);
        gl.SetFloatv(DGL_ENV_COLOR, params->modColor);

        // The actual texture.
        RL_BindTo(IS_MUL ? 1 : 0, params->texture);

        withDyn = true;
    }
    else
    {
        RL_SelectTexUnits(1);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 1);

        RL_Bind(params->texture);
        normal = DGL_TEXTURE0;
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
            gl.SetInteger(DGL_ACTIVE_TEXTURE, IS_MUL ? 1 : 0);
        }

        if(params->texc[0][VX] < 0 || params->texc[0][VX] > 1 ||
           params->texc[1][VX] < 0 || params->texc[1][VX] > 1)
        {
            // The texcoords are out of the normal [0,1] range.
            gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
        }
        else
        {
            // Visible portion is within the actual [0,1] range.
            gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
        }
    }
    GL_BlendMode(params->blendmode);

    // Draw one quad. This is obviously not a very efficient way to render
    // lots of masked walls, but since 3D models and sprites must be
    // rendered interleaved with masked walls, there's not much that can be
    // done about this.
    if(withDyn)
    {
        gl.Begin(DGL_QUADS);
        gl.Color4ubv(params->vertices[0].color);
        gl.MultiTexCoord2f(normal, params->texc[0][0],
                           params->texc[1][1]);

        gl.MultiTexCoord2f(dyn, params->modTexC[0][0],
                           params->modTexC[1][1]);

        gl.Vertex3f(params->vertices[0].pos[VX],
                    params->vertices[0].pos[VZ],
                    params->vertices[0].pos[VY]);

        gl.Color4ubv(params->vertices[1].color);
        gl.MultiTexCoord2fv(normal, params->texc[0]);

        gl.MultiTexCoord2f(dyn, params->modTexC[0][0],
                           params->modTexC[1][0]);

        gl.Vertex3f(params->vertices[1].pos[VX],
                    params->vertices[1].pos[VZ],
                    params->vertices[1].pos[VY]);

        gl.Color4ubv(params->vertices[3].color);
        gl.MultiTexCoord2f(normal, params->texc[1][0],
                           params->texc[0][1]);

        gl.MultiTexCoord2f(dyn, params->modTexC[0][1],
                           params->modTexC[1][0]);

        gl.Vertex3f(params->vertices[3].pos[VX],
                    params->vertices[3].pos[VZ],
                    params->vertices[3].pos[VY]);

        gl.Color4ubv(params->vertices[2].color);
        gl.MultiTexCoord2fv(normal, params->texc[1]);

        gl.MultiTexCoord2f(dyn, params->modTexC[0][1],
                           params->modTexC[1][1]);

        gl.Vertex3f(params->vertices[2].pos[VX],
                    params->vertices[2].pos[VZ],
                    params->vertices[2].pos[VY]);
        gl.End();

        // Restore normal GL state.
        RL_SelectTexUnits(1);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
        gl.DisableArrays(true, true, 0x1);
    }
    else
    {
        gl.Begin(DGL_QUADS);
        gl.Color4ubv(params->vertices[0].color);
        gl.MultiTexCoord2f(normal, params->texc[0][0],
                           params->texc[1][1]);

        gl.Vertex3f(params->vertices[0].pos[VX],
                    params->vertices[0].pos[VZ],
                    params->vertices[0].pos[VY]);

        gl.Color4ubv(params->vertices[1].color);
        gl.MultiTexCoord2fv(normal, params->texc[0]);

        gl.Vertex3f(params->vertices[1].pos[VX],
                    params->vertices[1].pos[VZ],
                    params->vertices[1].pos[VY]);

        gl.Color4ubv(params->vertices[3].color);
        gl.MultiTexCoord2f(normal, params->texc[1][0],
                           params->texc[0][1]);

        gl.Vertex3f(params->vertices[3].pos[VX],
                    params->vertices[3].pos[VZ],
                    params->vertices[3].pos[VY]);

        gl.Color4ubv(params->vertices[2].color);
        gl.MultiTexCoord2fv(normal, params->texc[1]);

        gl.Vertex3f(params->vertices[2].pos[VX],
                    params->vertices[2].pos[VZ],
                    params->vertices[2].pos[VY]);
        gl.End();
    }

    GL_BlendMode(BM_NORMAL);
}

static void setupModelParamsForVisSprite(modelparams_t *params,
                                         vissprite_t *spr)
{
    if(!params || !spr)
        return; // Hmm...

    if(spr->type == VSPR_MAP_OBJECT ||
       spr->type == VSPR_HUD_MODEL)
    {
        visspritelightparams_t lparams;

        lparams.starkLight = (spr->type == VSPR_HUD_MODEL);
        memcpy(lparams.center, spr->center, sizeof(lparams.center));
        lparams.subsector = spr->data.mo.subsector;
        lparams.maxLights = modelLight;

        {
        float   rgba[4];
        memcpy(rgba, spr->data.mo.rgb, sizeof(float) * 3);
        rgba[CA] = 1;
        R_SetAmbientColor(rgba, spr->data.mo.lightlevel, spr->distance);
        }
        R_DetermineLightsAffectingVisSprite(&lparams, &params->lights, &params->numLights);

        params->mf = spr->data.mo.mf;
        params->nextmf = spr->data.mo.nextmf;
        params->inter = spr->data.mo.inter;
        params->alwaysInterpolate = false;
        params->id = spr->data.mo.id;
        params->selector = spr->data.mo.selector;
        params->flags = spr->data.mo.flags;
        params->center[VX] = spr->center[VX];
        params->center[VY] = spr->center[VY];
        params->center[VZ] = spr->center[VZ];
        params->srvo[VX] = spr->data.mo.visoff[VX];
        params->srvo[VY] = spr->data.mo.visoff[VY];
        params->srvo[VZ] = spr->data.mo.visoff[VZ] - spr->data.mo.floorclip;
        params->gzt = spr->data.mo.gzt;
        params->distance = spr->distance;
        params->yaw = spr->data.mo.yaw;
        params->extraYawAngle = 0;
        params->yawAngleOffset = spr->data.mo.yawAngleOffset;
        params->pitch = spr->data.mo.pitch;
        params->extraPitchAngle = 0;
        params->pitchAngleOffset = spr->data.mo.pitchAngleOffset;
        params->extraScale = 0;
        params->subsector = spr->data.mo.subsector;

        params->lightLevel = spr->data.mo.lightlevel;

        memcpy(params->rgb, spr->data.mo.rgb, sizeof(float) * 3);
        params->uniformColor = false;
        params->alpha = spr->data.mo.alpha;

        params->viewAligned = spr->data.mo.viewaligned;
        params->mirror =
            (spr->type == VSPR_HUD_MODEL && mirrorHudModels != 0);

        params->shineYawOffset = (spr->type == VSPR_HUD_MODEL? -vang : 0);
        params->shinePitchOffset = (spr->type == VSPR_HUD_MODEL? vpitch + 90 : 0);
        params->shineTranslateWithViewerPos = false;
        params->shinepspriteCoordSpace = (spr->type == VSPR_HUD_MODEL);
    }
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
    float       center[3];
    boolean     haloDrawn = false;
    vissprite_t *spr;

    if(!willRenderSprites)
        return;

    R_SortVisSprites();
    if(vissprite_p > vissprites)
    {
        // Draw all vissprites back to front.
        // Sprites look better with Z buffer writes turned off.
        for(spr = vsprsortedhead.next; spr != &vsprsortedhead; spr = spr->next)
        {
            if(spr->type == VSPR_MASKED_WALL)
            {
                rendmaskedwallparams_t params;

                setupMaskedWallParamsForVisSprite(&params, spr);
                Rend_RenderMaskedWall(&spr->data.wall);
            }
            else if(spr->type == VSPR_MAP_OBJECT)
            {
                // There might be a model for this sprite, let's see.
                if(!spr->data.mo.mf)
                {   // Render an old fashioned sprite, ah the nostalgia...
                    if(spr->data.mo.patch >= 0)
                    {
                        rendspriteparams_t params;

                        setupSpriteParamsForVisSprite(&params, spr);
                        Rend_RenderSprite(&params);
                    }
                }
                else
                {   // It's a sprite and it has a modelframe (it's a 3D model).
                    modelparams_t params;

                    setupModelParamsForVisSprite(&params, spr);
                    Rend_RenderModel(&params);
                }
            }

            // How about a halo?
            if(spr->light)
            {
                center[VX] = spr->center[VX];
                center[VY] = spr->center[VY];
                center[VZ] = spr->center[VZ];

                if(spr->type == VSPR_MAP_OBJECT)
                {
                    center[VX] += spr->data.mo.visoff[VX];
                    center[VY] += spr->data.mo.visoff[VY];
                    center[VZ] += spr->data.mo.visoff[VZ];
                }

                if(H_RenderHalo(center[VX], center[VY], center[VZ],
                                spr->light, true) &&
                   !haloDrawn)
                    haloDrawn = true;
            }
        }

        // Draw secondary halos.
        if(haloDrawn && haloMode > 1)
        {
            // Now we can setup the state only once.
            H_SetupState(true);

            for(spr = vsprsortedhead.next; spr != &vsprsortedhead;
                spr = spr->next)
            {
                if(spr->light)
                {
                    center[VX] = spr->center[VX];
                    center[VY] = spr->center[VY];
                    center[VZ] = spr->center[VZ];

                    if(spr->type == VSPR_MAP_OBJECT ||
                       spr->type == VSPR_HUD_MODEL)
                    {
                        center[VX] += spr->data.mo.visoff[VX];
                        center[VY] += spr->data.mo.visoff[VY];
                        center[VZ] += spr->data.mo.visoff[VZ];
                    }

                    H_RenderHalo(center[VX], center[VY], center[VZ],
                                 spr->light, false);
                }
            }

            // And we're done...
            H_SetupState(false);
        }
    }
}

void Rend_RenderSprite(const rendspriteparams_t *params)
{
    int         i;
    gl_color_t  quadColors[4];
    gl_vertex_t quadNormals[4];
    boolean     restoreMatrix = false;
    boolean     restoreZ = false;
    float       spriteCenter[3];
    float       surfaceNormal[3];
    float       v1[3], v2[3], v3[3], v4[3];

    if(renderTextures == 1)
    {
        // Do we need to translate any of the colors?
        if(params->tmap)
        {   // We need to prepare a translated version of the sprite.
            GL_SetTranslatedSprite(params->patchNum, params->tmap,
                                   params->tclass);
        }
        else
        {   // Set the texture. No translation required.
            GL_SetSprite(params->patchNum, 0);
        }
    }
    else if(renderTextures == 2)
        // For lighting debug, render all solid surfaces using the gray texture.
        GL_BindTexture(GL_PrepareMaterial(DDT_GRAY, MAT_DDTEX, NULL));
    else
        gl.Bind(0);

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
    M_PointCrossProduct(v2, v3, v1, surfaceNormal);
    M_Normalize(surfaceNormal);

    // All sprite vertices are co-plannar, so just copy the surface normal.
    // \fixme: Can we do something better here?
    for(i = 0; i < 4; ++i)
        memcpy(quadNormals[i].xyz, surfaceNormal, sizeof(surfaceNormal));

    // Light the sprite.
    if(params->lightLevel < 0)
    {   // Fullbright white.
        Spr_FullBrightVertexColors(4, quadColors, params->rgba[CA]);
    }
    else
    {   // Lit normally.
        extern float ambientColor[3];

        float       ambient[4];

        memcpy(ambient, ambientColor, sizeof(float) * 3);
        ambient[CA] = params->rgba[CA];

        Spr_VertexColors(4, quadColors, quadNormals,
                         params->numLights, params->lights, ambient);
    }

    // Do we need to do some aligning?
    if(params->viewAligned || alwaysAlign >= 2)
    {
        // We must set up a modelview transformation matrix.
        restoreMatrix = true;
        gl.MatrixMode(DGL_MODELVIEW);
        gl.PushMatrix();

        // Rotate around the center of the sprite.
        gl.Translatef(spriteCenter[VX], spriteCenter[VZ], spriteCenter[VY]);
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
                    gl.Rotatef(turnAngle, s_dx, 0, s_dy);
                }
            }
            else
            {   // Restricted view plane alignment.
                // This'll do, for now... Really it should notice both the
                // sprite angle and vpitch.
                gl.Rotatef(vpitch * .5f, s_dx, 0, s_dy);
            }
        }
        else
        {
            // Normal rotation perpendicular to the view plane.
            gl.Rotatef(vpitch, viewsidex, 0, viewsidey);
        }
        gl.Translatef(-spriteCenter[VX], -spriteCenter[VZ], -spriteCenter[VY]);
    }

    // Need to change blending modes?
    if(params->blendMode != BM_NORMAL)
        GL_BlendMode(params->blendMode);

    // Transparent sprites shouldn't be written to the Z buffer.
    if(params->noZWrite || params->rgba[CA] < .98f ||
       !(params->blendMode == BM_NORMAL || params->blendMode == BM_ZEROALPHA))
    {
        restoreZ = true;
        gl.Disable(DGL_DEPTH_WRITE);
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

    tc[0].st[0] = params->texOffset[0] *  (params->texFlip[0]? 1:0);
    tc[0].st[1] = params->texOffset[1] * (!params->texFlip[1]? 1:0);
    tc[1].st[0] = params->texOffset[0] *  (params->texFlip[0]? 1:0);
    tc[1].st[1] = params->texOffset[1] *  (params->texFlip[1]? 1:0);
    tc[2].st[0] = params->texOffset[0] * (!params->texFlip[0]? 1:0);
    tc[2].st[1] = params->texOffset[1] *  (params->texFlip[1]? 1:0);
    tc[3].st[0] = params->texOffset[0] * (!params->texFlip[0]? 1:0);
    tc[3].st[1] = params->texOffset[1] * (!params->texFlip[1]? 1:0);

    renderQuad(v, quadColors, tc);
    }

    // Need to restore the original modelview matrix?
    if(restoreMatrix)
        gl.PopMatrix();

    // Change back to normal blending?
    if(params->blendMode != BM_NORMAL)
        GL_BlendMode(BM_NORMAL);

    // Enable Z-writing again?
    if(restoreZ)
        gl.Enable(DGL_DEPTH_WRITE);
}

static void setupSpriteParamsForVisSprite(rendspriteparams_t *params,
                                          vissprite_t *spr)
{
    float       top;
    visspritelightparams_t lparams;
    spritelump_t *sprite = spritelumps[spr->data.mo.patch];

    // Setup params:
    params->width = (float) sprite->width;
    params->height = sprite->height;

    // We must find the correct positioning using the sector floor and ceiling
    // heights as an aid.
    top = spr->data.mo.gzt;
    // Sprite fits in, adjustment possible?
    if(params->height < spr->data.mo.secceil - spr->data.mo.secfloor)
    {
        // Check top.
        if((spr->data.mo.flags & DDMF_FITTOP) && top > spr->data.mo.secceil)
            top = spr->data.mo.secceil;
        // Check bottom.
        if(spr->data.mo.flooradjust &&
           !(spr->data.mo.flags & DDMF_NOFITBOTTOM) &&
           top - params->height < spr->data.mo.secfloor)
            top = spr->data.mo.secfloor + params->height;
    }
    // Adjust by the floor clip.
    top -= spr->data.mo.floorclip;

    params->center[VX] = spr->center[VX];
    params->center[VY] = spr->center[VY];
    params->center[VZ] = top - params->height / 2;
    params->viewOffX = (float) sprite->offset - params->width / 2;
    params->subsector = spr->data.mo.subsector;
    params->distance = spr->distance;
    params->viewAligned = (spr->data.mo.viewaligned || alwaysAlign == 3);
    memcpy(params->srvo, spr->data.mo.visoff, sizeof(params->srvo));
    params->noZWrite = noSpriteZWrite;

    params->patchNum = spr->data.mo.patch;
    if(spr->data.mo.flags & DDMF_TRANSLATION)
        params->tmap = (spr->data.mo.flags & DDMF_TRANSLATION) >> DDMF_TRANSSHIFT;
    else
        params->tmap = 0;
    params->tclass = spr->data.mo.pclass;
    params->texOffset[0] = sprite->tc[0][0];
    params->texOffset[1] = sprite->tc[0][1];
    params->texFlip[0] = spr->data.mo.texFlip[0];
    params->texFlip[1] = spr->data.mo.texFlip[1];

    // Set the lighting and alpha.
    memcpy(params->rgba, spr->data.mo.rgb, sizeof(params->rgba));
    if(useSpriteAlpha)
    {
        if(missileBlend && (spr->data.mo.flags & DDMF_BRIGHTSHADOW))
            params->rgba[CA] = .8f;      // 80 %.
        else if(spr->data.mo.flags & DDMF_SHADOW)
            params->rgba[CA] = .333f;    // One third.
        else if(spr->data.mo.flags & DDMF_ALTSHADOW)
            params->rgba[CA] = .666f;    // Two thirds.
        else
            params->rgba[CA] = 1;

        // Sprite has a custom alpha multiplier?
        if(spr->data.mo.alpha >= 0)
            params->rgba[CA] *= spr->data.mo.alpha;
    }
    else
        params->rgba[CA] = 1;

    if(missileBlend && (spr->data.mo.flags & DDMF_BRIGHTSHADOW))
    {   // Additive blending.
        params->blendMode = BM_ADD;
    }
    else if(noSpriteTrans && params->rgba[CA] >= .98f)
    {   // Use the "no translucency" blending mode.
        params->blendMode = BM_ZEROALPHA;
    }
    else
    {
        params->blendMode = BM_NORMAL;
    }

    params->lightLevel = spr->data.mo.lightlevel;

    lparams.starkLight = false;
    memcpy(lparams.center, params->center, sizeof(lparams.center));
    lparams.maxLights = spriteLight;
    lparams.subsector = params->subsector;

    R_SetAmbientColor(params->rgba, params->lightLevel, params->distance);
    R_DetermineLightsAffectingVisSprite(&lparams, &params->lights, &params->numLights);
}
