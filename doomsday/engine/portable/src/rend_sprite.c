/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
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

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct spritelighterdata_s {
    vissprite_t *spr;
    byte        *rgb1, *rgb2;
    fvertex_t    viewvec;
} spritelighterdata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void Rend_ScaledAmbientLight(byte *out, byte *ambient, float mul);
static boolean Rend_SpriteLighter(lumobj_t * lum, fixed_t dist, void *data);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean willRenderSprites;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

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
    C_VAR_INT("rend-sprite-lit", &litSprites, 0, 0, 1);
    C_VAR_BYTE("rend-sprite-mode", &noSpriteTrans, 0, 0, 1);
    C_VAR_INT("rend-sprite-noz", &noSpriteZWrite, 0, 0, 1);
    C_VAR_BYTE("rend-sprite-precache", &precacheSprites, 0, 0, 1);
}

/**
 * Fog is turned off while rendering. It's not feasible to think that the
 * fog would obstruct the player's view of his own weapon.
 */
void Rend_Draw3DPlayerSprites(void)
{
    int     i;

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
        Rend_RenderModel(vispsprites + i);
    }

    // Should we turn the fog back on?
    if(usingFog)
        gl.Enable(DGL_FOG);
}

static void Rend_DoLightSprite(vissprite_t *spr, rendpoly_t *quad)
{
    spritelighterdata_t data;
    float   len;

    memcpy(&quad->vertices[1].color, &quad->vertices[0].color, 3);

    data.spr = spr;
    data.rgb1 = quad->vertices[0].color.rgba;
    data.rgb2 = quad->vertices[1].color.rgba;

    data.viewvec.pos[VX] = FIX2FLT(spr->data.mo.gx - viewx);
    data.viewvec.pos[VY] = FIX2FLT(spr->data.mo.gy - viewy);

    len = sqrt(data.viewvec.pos[VX] * data.viewvec.pos[VX] +
               data.viewvec.pos[VY] * data.viewvec.pos[VY]);
    if(len)
    {
        data.viewvec.pos[VX] /= len;
        data.viewvec.pos[VY] /= len;
        DL_RadiusIterator(spr->data.mo.subsector, spr->data.mo.gx,
                          spr->data.mo.gy, dlMaxRad << FRACBITS,
                          &data, Rend_SpriteLighter);
    }

    // Check floor and ceiling for glow. They count as ambient light.
    if(spr->data.mo.hasglow)
    {
        int     v;
        float   glowHeight;

        // Floor glow
        glowHeight = (MAX_GLOWHEIGHT * spr->data.mo.floorglowamount)
                      * glowHeightFactor;
        // Don't make too small or too large glows.
        if(glowHeight > 2)
        {
            if(glowHeight > glowHeightMax)
                glowHeight = glowHeightMax;

            len = 1 - (FIX2FLT(spr->data.mo.gz) -
                       spr->data.mo.secfloor) / glowHeight;

            for(v = 0; v < 2; ++v)
                Rend_ScaledAmbientLight(quad->vertices[v].color.rgba,
                                        spr->data.mo.floorglow, len);
        }

        // Ceiling glow
        glowHeight = (MAX_GLOWHEIGHT * spr->data.mo.ceilglowamount)
                      * glowHeightFactor;
        // Don't make too small or too large glows.
        if(glowHeight > 2)
        {
            if(glowHeight > glowHeightMax)
                glowHeight = glowHeightMax;

            len = 1 - (spr->data.mo.secceil -
                       FIX2FLT(spr->data.mo.gzt)) / glowHeight;

            for(v = 0; v < 2; ++v)
                Rend_ScaledAmbientLight(quad->vertices[v].color.rgba,
                                        spr->data.mo.ceilglow, len);
        }
    }
}

static void Rend_DrawPSprite(float x, float y, byte *color1, byte *color2,
                             float scale, int flip, int lump)
{
    int     w, h;
    int     w2, h2;
    float   s, t;
    int     pSprMode = 1;
    spritelump_t *slump = spritelumps[lump];

    if(flip)
        flip = 1;               // Make sure it's zero or one.

    if(renderTextures)
        GL_SetSprite(lump, pSprMode);
    else
        gl.Bind(0);

    w = slump->width;
    h = slump->height;
    w2 = CeilPow2(w);
    h2 = CeilPow2(h);
    w *= scale;
    h *= scale;

    // Let's calculate texture coordinates.
    // To remove a possible edge artifact, move the corner a bit up/left.
    s = slump->tc[pSprMode][VX] - 0.4f / w2;
    t = slump->tc[pSprMode][VY] - 0.4f / h2;

    if(litSprites)
    {
        gl.Begin(DGL_QUADS);

        gl.Color4ubv(color1);

        gl.TexCoord2f(flip * s, 0);
        gl.Vertex2f(x, y);

        gl.Color4ubv(color2);

        gl.TexCoord2f(!flip * s, 0);
        gl.Vertex2f(x + w, y);

        gl.TexCoord2f(!flip * s, t);
        gl.Vertex2f(x + w, y + h);

        gl.Color4ubv(color1);

        gl.TexCoord2f(flip * s, t);
        gl.Vertex2f(x, y + h);
        gl.End();
    }
    else
    {
        gl.Begin(DGL_QUADS);

        gl.Color4ubv(color1);

        gl.TexCoord2f(flip * s, 0);
        gl.Vertex2f(x, y);

        gl.TexCoord2f(!flip * s, 0);
        gl.Vertex2f(x + w, y);

        gl.TexCoord2f(!flip * s, t);
        gl.Vertex2f(x + w, y + h);

        gl.TexCoord2f(flip * s, t);
        gl.Vertex2f(x, y + h);
        gl.End();
    }
}

/**
 * Draws 2D player sprites. If they were already drawn 3D, this
 * won't do anything.
 */
void Rend_DrawPlayerSprites(void)
{
    spriteinfo_t info[DDMAXPSPRITES];
    ddpsprite_t *psp;
    int     i, c;
    sector_t *sec = viewplayer->mo->subsector->sector;
    float   offx = pspOffX / 16.0f;
    float   offy = pspOffY / 16.0f;
    float   offScaleY = weaponOffsetScaleY / 1000.0f;
    byte    somethingVisible = false;
    byte    isFullBright = (levelFullBright != 0);
    rendpoly_t *tempquad = NULL;

    // Cameramen have no psprites.
    if((viewplayer->flags & DDPF_CAMERA) || (viewplayer->flags & DDPF_CHASECAM))
        return;

    // Clear sprite info.
    memset(info, 0, sizeof(info));

    // Check for fullbright.
    for(i = 0, psp = viewplayer->psprites; i < DDMAXPSPRITES; ++i, psp++)
    {
        // Should this psprite be drawn?
        if((psp->flags & DDPSPF_RENDERED) || !psp->stateptr)
            continue;           // No...

        // If one of the psprites is fullbright, both are.
        if(psp->stateptr->flags & STF_FULLBRIGHT)
            isFullBright = true;

        // Something will be drawn.
        somethingVisible = true;
        psp->flags |= DDPSPF_RENDERED;

        R_GetSpriteInfo(psp->stateptr->sprite, psp->stateptr->frame,
                        &info[i]);
    }

    if(!somethingVisible)
    {
        // No psprite was found, we can get out of here.
        return;
    }

    psp = viewplayer->psprites;

    // If both psprites have identical attributes, they can be combined
    // into one sprite (cvar allowing).
    /*  if(rend_hud_combined_sprite
       && info[0].realLump > 0 && info[1].realLump > 0
       && psp[0].alpha == psp[1].alpha
       && psp[0].light == psp[1].light)
       {
       // Use the sprite info to prepare a single GL texture for
       // both the psprites.

       Con_Printf("0:x(%x,%x) off(%x,%x)\n",
       psp[0].x, psp[0].y, info[0].offset, info[0].topOffset);
       Con_Printf("1:x(%x,%x) off(%x,%x)\n",
       psp[1].x, psp[1].y, info[1].offset, info[1].topOffset);
       Con_Printf("--\n");
       }
       else */
    {
        const byte *secRGB = R_GetSectorLightColor(sec);
        byte  secbRGB[3];
        byte  rgba[4];

        if(useBias)
        {
            // Evaluate the position of this player in the light grid.
            // TODO: Could be affected by BIAS (dynamic lights?).
            float point[3] = { FIX2FLT(viewplayer->mo->pos[VX]), FIX2FLT(viewplayer->mo->pos[VY]),
                               FIX2FLT(viewplayer->mo->pos[VZ] + viewplayer->mo->height/2)};
            LG_Evaluate(point, secbRGB);
        }

        tempquad = R_AllocRendPoly(RP_NONE, false, 2);
        tempquad->vertices[0].dist = tempquad->vertices[1].dist = 1;

        // Draw as separate sprites.
        for(i = 0; i < DDMAXPSPRITES; ++i)
        {
            int light = (int)(psp[i].light * 255.0f);

            if(!info[i].realLump)
                continue;       // This doesn't exist.

            Rend_ApplyLightAdaptation(&light);

            if(isFullBright)
            {
                memset(&rgba, 255, 3);
            }
            else if(useBias)
            {
                memcpy(&rgba, &secbRGB, 3);
            }
            else
            {
                float lval;

                for(c = 0; c < 3; ++c)
                {
                    lval = light * (secRGB[c] / 255.0f);

                    if(lval < 0)
                        lval = 0;
                    if(lval > 255)
                        lval = 255;

                    rgba[c] = (byte) lval;
                }
            }
            rgba[CA] = psp[i].alpha * 255.0f;

            RL_VertexColors(tempquad, light, rgba);

            // Add extra light using dynamic lights.
            if(litSprites)
                Rend_DoLightSprite(vispsprites +i, tempquad);

            tempquad->vertices[0].color.rgba[CA] =
                tempquad->vertices[1].color.rgba[CA] = rgba[CA];

            Rend_DrawPSprite(psp[i].x - info[i].offset + offx,
                           offScaleY * psp[i].y + (1 - offScaleY) * 32 -
                           info[i].topOffset + offy,
                           &tempquad->vertices[0].color.rgba[0],
                           &tempquad->vertices[1].color.rgba[0],
                           1, info[i].flip, info[i].lump);
        }
        R_FreeRendPoly(tempquad);
    }
}

/**
 * A sort of a sprite, I guess... Masked walls must be rendered sorted
 * with sprites, so no artifacts appear when sprites are seen behind
 * masked walls.
 */
void Rend_RenderMaskedWall(vissprite_t * vis)
{
    float   color[4];
    boolean withDyn = false;
    int     normal, dyn = DGL_TEXTURE1;

    // Do we have a dynamic light to blend with?
    // This only happens when multitexturing is enabled.
    if(vis->data.wall.light)
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
        RL_BindTo(IS_MUL ? 0 : 1, vis->data.wall.light->texture);
        RL_FloatRGB(vis->data.wall.light->color, color);
        gl.SetFloatv(DGL_ENV_COLOR, color);

        // The actual texture.
        RL_BindTo(IS_MUL ? 1 : 0, vis->data.wall.texture);

        withDyn = true;
    }
    else
    {
        RL_SelectTexUnits(1);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 1);

        RL_Bind(vis->data.wall.texture);
        normal = DGL_TEXTURE0;
    }

    // Masked walls are sometimes used for special effects like arcs,
    // cobwebs and bottoms of sails. In order for them to look right,
    // we need to disable texture wrapping on the horizontal axis (S).
    // Most masked walls need wrapping, though. What we need to do is
    // look at the texture coordinates and see if they require texture
    // wrapping.
    if(vis->data.wall.masked)
    {
        if(withDyn)
        {
            gl.SetInteger(DGL_ACTIVE_TEXTURE, IS_MUL ? 1 : 0);
        }

        if(vis->data.wall.texc[0][VX] < 0 || vis->data.wall.texc[0][VX] > 1 ||
           vis->data.wall.texc[1][VX] < 0 || vis->data.wall.texc[1][VX] > 1)
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
    GL_BlendMode(vis->data.wall.blendmode);

    // Draw one quad. This is obviously not a very efficient way to render
    // lots of masked walls, but since 3D models and sprites must be
    // rendered interleaved with masked walls, there's not much that can be
    // done about this.
    if(withDyn)
    {
        gl.Begin(DGL_QUADS);

        gl.Color4ubv(vis->data.wall.vertices[0].color);
        gl.MultiTexCoord2f(normal, vis->data.wall.texc[0][VX],
                           vis->data.wall.texc[1][VY]);

        gl.MultiTexCoord2f(dyn, vis->data.wall.light->s[0],
                           vis->data.wall.light->t[1]);

        gl.Vertex3f(vis->data.wall.vertices[0].pos[VX],
                    vis->data.wall.vertices[0].pos[VZ],
                    vis->data.wall.vertices[0].pos[VY]);

        gl.MultiTexCoord2fv(normal, vis->data.wall.texc[0]);

        gl.MultiTexCoord2f(dyn, vis->data.wall.light->s[0],
                           vis->data.wall.light->t[0]);

        gl.Vertex3f(vis->data.wall.vertices[2].pos[VX],
                    vis->data.wall.vertices[2].pos[VZ],
                    vis->data.wall.vertices[2].pos[VY]);

        gl.Color4ubv(vis->data.wall.vertices[1].color);
        gl.MultiTexCoord2f(normal, vis->data.wall.texc[1][VX],
                           vis->data.wall.texc[0][VY]);

        gl.MultiTexCoord2f(dyn, vis->data.wall.light->s[1],
                           vis->data.wall.light->t[0]);

        gl.Vertex3f(vis->data.wall.vertices[3].pos[VX],
                    vis->data.wall.vertices[3].pos[VZ],
                    vis->data.wall.vertices[3].pos[VY]);

        gl.MultiTexCoord2fv(normal, vis->data.wall.texc[1]);

        gl.MultiTexCoord2f(dyn, vis->data.wall.light->s[1],
                           vis->data.wall.light->t[1]);

        gl.Vertex3f(vis->data.wall.vertices[1].pos[VX],
                    vis->data.wall.vertices[1].pos[VZ],
                    vis->data.wall.vertices[1].pos[VY]);

        gl.End();

        // Restore normal GL state.
        RL_SelectTexUnits(1);
        gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
        gl.DisableArrays(true, true, 0x1);
    }
    else
    {
        gl.Begin(DGL_QUADS);

        gl.Color4ubv(vis->data.wall.vertices[0].color);
        gl.MultiTexCoord2f(normal, vis->data.wall.texc[0][VX],
                           vis->data.wall.texc[1][VY]);

        gl.Vertex3f(vis->data.wall.vertices[0].pos[VX],
                    vis->data.wall.vertices[0].pos[VZ],
                    vis->data.wall.vertices[0].pos[VY]);

        gl.MultiTexCoord2fv(normal, vis->data.wall.texc[0]);

        gl.Vertex3f(vis->data.wall.vertices[2].pos[VX],
                    vis->data.wall.vertices[2].pos[VZ],
                    vis->data.wall.vertices[2].pos[VY]);

        gl.Color4ubv(vis->data.wall.vertices[1].color);
        gl.MultiTexCoord2f(normal, vis->data.wall.texc[1][VX],
                           vis->data.wall.texc[0][VY]);

        gl.Vertex3f(vis->data.wall.vertices[3].pos[VX],
                    vis->data.wall.vertices[3].pos[VZ],
                    vis->data.wall.vertices[3].pos[VY]);

        gl.MultiTexCoord2fv(normal, vis->data.wall.texc[1]);

        gl.Vertex3f(vis->data.wall.vertices[1].pos[VX],
                    vis->data.wall.vertices[1].pos[VZ],
                    vis->data.wall.vertices[1].pos[VY]);

        gl.End();
    }

    GL_BlendMode(BM_NORMAL);
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
    boolean haloDrawn = false;
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
                // Depth writing is once again needed.
                Rend_RenderMaskedWall(spr);
            }
            else // This is a mobj.
            {
                // There might be a model for this sprite, let's see.
                if(!spr->data.mo.mf)
                {
                    if(spr->data.mo.patch >= 0)
                    {
                        // Render an old fashioned sprite.
                        // Ah, the nostalgia...
                        if(noSpriteZWrite)
                            gl.Disable(DGL_DEPTH_WRITE);
                        Rend_RenderSprite(spr);
                        if(noSpriteZWrite)
                            gl.Enable(DGL_DEPTH_WRITE);
                    }
                }
                else // It's a sprite and it has a modelframe (it's a 3D model).
                {
                    Rend_RenderModel(spr);
                }

                // How about a halo?
                if(spr->data.mo.light)
                {
                    haloDrawn = H_RenderHalo(spr, true);
                }
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
                if(spr->type && spr->data.mo.light)
                    H_RenderHalo(spr, false);
            }

            // And we're done...
            H_SetupState(false);
        }
    }
}

// Mode zero (tc[0]) is used with regular sprites
#define Rend_SpriteTexCoord(pnum, x, y) \
( \
    gl.TexCoord2f(spritelumps[pnum]->tc[0][VX] * x, \
                  spritelumps[pnum]->tc[0][VY] * y) \
)

static boolean Rend_SpriteLighter(lumobj_t * lum, fixed_t dist, void *data)
{
    int     i, temp;
    float   fdist = FIX2FLT(dist) * 1.2f; // Pretend the light is a bit further away.
    spritelighterdata_t *slData = (spritelighterdata_t*) data;
    fvertex_t lightVec;
    float   directness, side, inleft, inright, zfactor;

    if(!fdist)
        return true;
    if(slData->rgb1[0] == 0xff && slData->rgb1[1] == 0xff && slData->rgb1[2] == 0xff &&
       slData->rgb2[0] == 0xff && slData->rgb2[1] == 0xff && slData->rgb2[2] == 0xff)
        return false;           // No point in continuing, light is already white.

    zfactor =
        (FIX2FLT(slData->spr->data.mo.gz + slData->spr->data.mo.gzt) / 2 -
         (FIX2FLT(lum->thing->pos[VZ]) + lum->center)) / dlMaxRad;

    // Round out the "shape" of light to be more spherical.
    zfactor *= 8;

    if(zfactor < 0)
        zfactor = -zfactor;
    if(zfactor > 1)
        return true;            // Too high or low.

    zfactor = 1 - zfactor;
    // Enlarge the full-lit area.
    zfactor *= 4;
    if(zfactor > 1)
        zfactor = 1;

    lightVec.pos[VX]  = FIX2FLT(slData->spr->data.mo.gx - lum->thing->pos[VX]);
    lightVec.pos[VY]  = FIX2FLT(slData->spr->data.mo.gy - lum->thing->pos[VY]);
    lightVec.pos[VX] /= fdist;
    lightVec.pos[VY] /= fdist;

    // Also include the effect of the distance to zfactor.
    fdist /= (lum->radius * 2) /*dlMaxRad */ ;
    fdist = 1 - fdist;
    //fdist *= 4;
    if(fdist > 1)
        fdist = 1;
    zfactor *= fdist;

    // Now the view vector and light vector are normalized.
    directness = slData->viewvec.pos[VX] * lightVec.pos[VX] +
                 slData->viewvec.pos[VY] * lightVec.pos[VY];   // Dot product.
    side = -slData->viewvec.pos[VY] * lightVec.pos[VX] +
            slData->viewvec.pos[VX] * lightVec.pos[VY];
    // If side > 0, the light comes from the right.
    if(directness > 0)
    {
        // The light comes from the front.
        if(side > 0)
        {
            inright = 1;
            inleft = directness;
        }
        else
        {
            inleft = 1;
            inright = directness;
        }
    }
    else
    {
        // The light comes from the back.
        if(side > 0)
        {
            inright = side;
            inleft = 0;
        }
        else
        {
            inleft = -side;
            inright = 0;
        }
    }
    inright *= zfactor;
    inleft *= zfactor;
    if(inleft > 0)
    {
        for(i = 0; i < 3; ++i)
        {
            temp = slData->rgb1[i] + inleft * lum->rgb[i];
            if(temp > 0xff)
                temp = 0xff;
            slData->rgb1[i] = temp;
        }
    }
    if(inright > 0)
    {
        for(i = 0; i < 3; ++i)
        {
            temp = slData->rgb2[i] + inright * lum->rgb[i];
            if(temp > 0xff)
                temp = 0xff;
            slData->rgb2[i] = temp;
        }
    }
    return true;
}

static void Rend_ScaledAmbientLight(byte *out, byte *ambient, float mul)
{
    int     i;
    int     val;

    if(mul < 0)
        mul = 0;
    else if(mul > 1)
        mul = 1;

    for(i = 0; i < 3; ++i)
    {
        val = ambient[i] * mul;
        if(out[i] < val)
            out[i] = val;
    }
}

void Rend_RenderSprite(vissprite_t *spr)
{
    int     patch = spr->data.mo.patch;
    float   bot, top;
    int     sprh;
    float   v1[2];
    DGLubyte alpha;
    boolean additiveBlending = false, flip, restoreMatrix = false;
    boolean usingSRVO = false, restoreZ = false;
    rendpoly_t *tempquad = NULL;

    if(!renderTextures)
    {
        gl.Bind(0);
    }
    else
    {
        // Do we need to translate any of the colors?
        if(spr->data.mo.flags & DDMF_TRANSLATION)
        {
            // We need to prepare a translated version of the sprite.
            GL_SetTranslatedSprite(patch,
                                   (spr->data.mo.
                                    flags & DDMF_TRANSLATION) >> DDMF_TRANSSHIFT,
                                   spr->data.mo.class);
        }
        else
        {
            // Set the texture. No translation required.
            GL_SetSprite(patch, 0);
        }
    }
    sprh = spritelumps[patch]->height;

    // Set the lighting and alpha.
    if(useSpriteAlpha)
    {
        if(missileBlend && (spr->data.mo.flags & DDMF_BRIGHTSHADOW))
        {
            alpha = 204;            // 80 %.
        }
        else if(spr->data.mo.flags & DDMF_SHADOW)
            alpha = 51;             // One third.
        else if(spr->data.mo.flags & DDMF_ALTSHADOW)
            alpha = 160;            // Two thirds.
        else
            alpha = 255;

        // Sprite has a custom alpha multiplier?
        if(spr->data.mo.alpha >= 0)
            alpha *= spr->data.mo.alpha;
    }
    else
        alpha = 255;

    if(missileBlend && (spr->data.mo.flags & DDMF_BRIGHTSHADOW))
        additiveBlending = true;

    if(spr->data.mo.lightlevel < 0)
    {
        gl.Color4ub(0xff, 0xff, 0xff, alpha);
    }
    else
    {
        int lightLevel = spr->data.mo.lightlevel;
        Rend_ApplyLightAdaptation(&lightLevel);

        v1[VX] = Q_FIX2FLT(spr->data.mo.gx);
        v1[VY] = Q_FIX2FLT(spr->data.mo.gy);

        tempquad = R_AllocRendPoly(RP_NONE, false, 2);
        tempquad->vertices[0].dist =
            tempquad->vertices[1].dist = Rend_PointDist2D(v1);

        RL_VertexColors(tempquad, lightLevel, spr->data.mo.rgb);

        // Add extra light using dynamic lights.
        if(litSprites)
            Rend_DoLightSprite(spr, tempquad);

        tempquad->vertices[0].color.rgba[CA] =
            tempquad->vertices[1].color.rgba[CA] = alpha;
        gl.Color4ubv(tempquad->vertices[0].color.rgba);
    }

    // We must find the correct positioning using the sector floor and ceiling
    // heights as an aid.
    top = FIX2FLT(spr->data.mo.gzt);
    // Sprite fits in, adjustment possible?
    if(sprh < spr->data.mo.secceil - spr->data.mo.secfloor)
    {
        // Check top.
        if((spr->data.mo.flags & DDMF_FITTOP) && top > spr->data.mo.secceil)
            top = spr->data.mo.secceil;
        // Check bottom.
        if(spr->data.mo.flooradjust &&
           !(spr->data.mo.flags & DDMF_NOFITBOTTOM) &&
           top - sprh < spr->data.mo.secfloor)
            top = spr->data.mo.secfloor + sprh;
    }
    // Adjust by the floor clip.
    top -= FIX2FLT(spr->data.mo.floorclip);
    bot = top - sprh;

    // Should the texture be flipped?
    flip = spr->data.mo.flip;

    // Should we apply a SRVO?
    if(spr->data.mo.visoff[VX] || spr->data.mo.visoff[VY] ||
       spr->data.mo.visoff[VZ])
    {
        usingSRVO = true;
        gl.MatrixMode(DGL_MODELVIEW);
        gl.PushMatrix();
        gl.Translatef(spr->data.mo.visoff[VX], spr->data.mo.visoff[VZ],
                      spr->data.mo.visoff[VY]);
    }

    // Do we need to do some aligning?
    if(spr->data.mo.viewaligned || alwaysAlign >= 2)
    {
        float   centerx = FIX2FLT(spr->data.mo.gx);
        float   centery = FIX2FLT(spr->data.mo.gy);
        float   centerz = (top + bot) * .5f;

        // We must set up a modelview transformation matrix.
        restoreMatrix = true;
        gl.MatrixMode(DGL_MODELVIEW);
        gl.PushMatrix();

        // Rotate around the center of the sprite.
        gl.Translatef(centerx, centerz, centery);
        if(!spr->data.mo.viewaligned)
        {
            float   s_dx = spr->data.mo.v1[VX] - spr->data.mo.v2[VX];
            float   s_dy = spr->data.mo.v1[VY] - spr->data.mo.v2[VY];

            if(alwaysAlign == 2)    // restricted camera alignment
            {
                float   dx = centerx - vx, dy = centery - vz;
                float   spriteAngle =
                    BANG2DEG(bamsAtan2(centerz - vy, sqrt(dx * dx + dy * dy)));

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
            else                // restricted view plane alignment
            {
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
        gl.Translatef(-centerx, -centerz, -centery);
    }

    if(additiveBlending)
    {
        // Change the blending mode.
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
    }
    else if(noSpriteTrans && alpha >= 250)
    {
        // Use the "no translucency" blending mode.
        GL_BlendMode(BM_ZEROALPHA);
    }

    // Transparent sprites shouldn't be written to the Z buffer.
    if(alpha < 250 || additiveBlending)
    {
        restoreZ = true;
        gl.Disable(DGL_DEPTH_WRITE);
    }

    // Render the sprite.
    if(litSprites && spr->data.mo.lightlevel >= 0)
    {
        // Its a lit sprite
        gl.Begin(DGL_QUADS);
        Rend_SpriteTexCoord (patch, flip, 1);
        gl.Vertex3f(spr->data.mo.v1[VX], bot, spr->data.mo.v1[VY]);
        Rend_SpriteTexCoord (patch, flip, 0);
        gl.Vertex3f(spr->data.mo.v1[VX], top, spr->data.mo.v1[VY]);

        gl.Color4ubv(tempquad->vertices[1].color.rgba);

        Rend_SpriteTexCoord (patch, !flip, 0);
        gl.Vertex3f(spr->data.mo.v2[VX], top, spr->data.mo.v2[VY]);
        Rend_SpriteTexCoord (patch, !flip, 1);
        gl.Vertex3f(spr->data.mo.v2[VX], bot, spr->data.mo.v2[VY]);
        gl.End();
    }
    else
    {
        gl.Begin(DGL_QUADS);
        Rend_SpriteTexCoord (patch, flip, 1);
        gl.Vertex3f(spr->data.mo.v1[VX], bot, spr->data.mo.v1[VY]);
        Rend_SpriteTexCoord (patch, flip, 0);
        gl.Vertex3f(spr->data.mo.v1[VX], top, spr->data.mo.v1[VY]);

        Rend_SpriteTexCoord (patch, !flip, 0);
        gl.Vertex3f(spr->data.mo.v2[VX], top, spr->data.mo.v2[VY]);
        Rend_SpriteTexCoord (patch, !flip, 1);
        gl.Vertex3f(spr->data.mo.v2[VX], bot, spr->data.mo.v2[VY]);
        gl.End();
    }

    // This mirroring would be excellent if it were properly clipped.
     /*{
       gl.Disable(DGL_DEPTH_TEST);

       // Draw a "test mirroring".
       gl.Begin(DGL_QUADS);
       gl.Color4ub(tempquad->vertices[0].color.rgba[0],
       tempquad->vertices[0].color.rgba[1],
       tempquad->vertices[0].color.rgba[2],
       alpha/3);
       gl.TexCoord2f(flip, 0);
       gl.Vertex3f(spr->data.mo.v1[VX], 2*spr->data.mo.secfloor - top, spr->data.mo.v1[VY]);
       gl.TexCoord2f(flip, 1);
       gl.Vertex3f(spr->data.mo.v1[VX], 2*spr->data.mo.secfloor - bot, spr->data.mo.v1[VY]);
       gl.TexCoord2f(!flip, 1);
       gl.Vertex3f(spr->data.mo.v2[VX], 2*spr->data.mo.secfloor - bot, spr->data.mo.v2[VY]);
       gl.TexCoord2f(!flip, 0);
       gl.Vertex3f(spr->data.mo.v2[VX], 2*spr->data.mo.secfloor - top, spr->data.mo.v2[VY]);
       gl.End();

       gl.Enable(DGL_DEPTH_TEST);
       }*/

    if(restoreMatrix)
    {
        // Restore the original modelview matrix.
        gl.PopMatrix();
    }

    if(usingSRVO)
        gl.PopMatrix();

    if(noSpriteTrans || additiveBlending)
    {
        // Change to normal blending.
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
    }

    if(restoreZ)
    {
        // Enable Z-writing again.
        gl.Enable(DGL_DEPTH_WRITE);
    }
    if(tempquad)
        R_FreeRendPoly(tempquad);
}
