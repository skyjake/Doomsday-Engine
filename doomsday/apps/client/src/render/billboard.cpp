/** @file billboard.cpp  Rendering billboard "sprites".
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_base.h"
#include "render/billboard.h"

#include <de/vector1.h>
#include <de/concurrency.h>
#include <doomsday/console/var.h>
#include "clientapp.h"

#include "gl/gl_main.h"

#include "r_util.h"
#include "render/rend_main.h"
#include "render/vissprite.h"

#include "MaterialVariantSpec"
#include "Texture"
#include "resource/sprite.h"

#include "world/p_players.h"  // viewPlayer, ddPlayers

using namespace de;

dint spriteLight = 4;
dfloat maxSpriteAngle = 60;

// If true - use the "no translucency" blending mode for sprites/masked walls
dbyte noSpriteTrans;
dint useSpriteAlpha = 1;
dint useSpriteBlend = 1;

dint alwaysAlign;
dint noSpriteZWrite;

dbyte devNoSprites;

static inline RenderSystem &rendSys()
{
    return ClientApp::renderSystem();
}

static inline ResourceSystem &resSys()
{
    return ClientApp::resourceSystem();
}

static inline void drawQuad(dgl_vertex_t *v, dgl_color_t *c, dgl_texcoord_t *tc)
{
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

void Rend_DrawMaskedWall(drawmaskedwallparams_t const &parms)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    TextureVariant *tex = nullptr;
    if(::renderTextures)
    {
        MaterialAnimator *matAnimator = parms.animator;
        DENG2_ASSERT(matAnimator);

        // Ensure we have up to date info about the material.
        matAnimator->prepare();

        tex = matAnimator->texUnit(MaterialAnimator::TU_LAYER0).texture;
    }

    // Do we have a dynamic light to blend with?
    // This only happens when multitexturing is enabled.
    bool withDyn = false;
    dint normal = 0, dyn = 1;
    if(parms.modTex && ::numTexUnits > 1)
    {
        if(IS_MUL)
        {
            normal = 1;
            dyn    = 0;
        }
        else
        {
            normal = 0;
            dyn    = 1;
        }

        GL_SelectTexUnits(2);
        GL_ModulateTexture(IS_MUL ? 4 : 5);

        // The dynamic light.
        glActiveTexture(IS_MUL ? GL_TEXTURE0 : GL_TEXTURE1);
        /// @todo modTex may be the name of a "managed" texture.
        GL_BindTextureUnmanaged(renderTextures ? parms.modTex : 0,
                                gl::ClampToEdge, gl::ClampToEdge);

        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, parms.modColor);

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

    GL_BlendMode(parms.blendMode);

    GLenum normalTarget = normal? GL_TEXTURE1 : GL_TEXTURE0;
    GLenum dynTarget    =    dyn? GL_TEXTURE1 : GL_TEXTURE0;

    // Draw one quad. This is obviously not a very efficient way to render
    // lots of masked walls, but since 3D models and sprites must be
    // rendered interleaved with masked walls, there's not much that can be
    // done about this.
    if(withDyn)
    {
        glBegin(GL_QUADS);
            glColor4fv(parms.vertices[0].color);
            glMultiTexCoord2f(normalTarget, parms.texCoord[0][0], parms.texCoord[1][1]);

            glMultiTexCoord2f(dynTarget, parms.modTexCoord[0][0], parms.modTexCoord[1][1]);

            glVertex3f(parms.vertices[0].pos[0],
                       parms.vertices[0].pos[2],
                       parms.vertices[0].pos[1]);

            glColor4fv(parms.vertices[1].color);
            glMultiTexCoord2f(normalTarget, parms.texCoord[0][0], parms.texCoord[0][1]);

            glMultiTexCoord2f(dynTarget, parms.modTexCoord[0][0], parms.modTexCoord[0][1]);

            glVertex3f(parms.vertices[1].pos[0],
                       parms.vertices[1].pos[2],
                       parms.vertices[1].pos[1]);

            glColor4fv(parms.vertices[3].color);
            glMultiTexCoord2f(normalTarget, parms.texCoord[1][0], parms.texCoord[0][1]);

            glMultiTexCoord2f(dynTarget, parms.modTexCoord[1][0], parms.modTexCoord[0][1]);

            glVertex3f(parms.vertices[3].pos[0],
                       parms.vertices[3].pos[2],
                       parms.vertices[3].pos[1]);

            glColor4fv(parms.vertices[2].color);
            glMultiTexCoord2f(normalTarget, parms.texCoord[1][0], parms.texCoord[1][1]);

            glMultiTexCoord2f(dynTarget, parms.modTexCoord[1][0], parms.modTexCoord[1][1]);

            glVertex3f(parms.vertices[2].pos[0],
                       parms.vertices[2].pos[2],
                       parms.vertices[2].pos[1]);
        glEnd();

        // Restore normal GL state.
        GL_SelectTexUnits(1);
        GL_ModulateTexture(1);
    }
    else
    {
        glBegin(GL_QUADS);
            glColor4fv(parms.vertices[0].color);
            glTexCoord2f(parms.texCoord[0][0], parms.texCoord[1][1]);

            glVertex3f(parms.vertices[0].pos[0],
                       parms.vertices[0].pos[2],
                       parms.vertices[0].pos[1]);

            glColor4fv(parms.vertices[1].color);
            glTexCoord2f(parms.texCoord[0][0], parms.texCoord[0][1]);

            glVertex3f(parms.vertices[1].pos[0],
                       parms.vertices[1].pos[2],
                       parms.vertices[1].pos[1]);

            glColor4fv(parms.vertices[3].color);
            glTexCoord2f(parms.texCoord[1][0], parms.texCoord[0][1]);

            glVertex3f(parms.vertices[3].pos[0],
                       parms.vertices[3].pos[2],
                       parms.vertices[3].pos[1]);

            glColor4fv(parms.vertices[2].color);
            glTexCoord2f(parms.texCoord[1][0], parms.texCoord[1][1]);

            glVertex3f(parms.vertices[2].pos[0],
                       parms.vertices[2].pos[2],
                       parms.vertices[2].pos[1]);
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    GL_BlendMode(BM_NORMAL);
}

/**
 * Set all the colors in the array to that specified.
 */
static void applyUniformColor(dint count, dgl_color_t *colors, dfloat const *rgba)
{
    for(; count-- > 0; colors++)
    {
        colors->rgba[0] = dbyte(255 * rgba[0]);
        colors->rgba[1] = dbyte(255 * rgba[1]);
        colors->rgba[2] = dbyte(255 * rgba[2]);
        colors->rgba[3] = dbyte(255 * rgba[3]);
    }
}

/**
 * Calculate vertex lighting.
 */
static void Spr_VertexColors(dint count, dgl_color_t *out, dgl_vertex_t *normalIt,
    duint lightListIdx, duint maxLights, dfloat const *ambient)
{
    DENG2_ASSERT(out && normalIt);

    Vector3f const saturated(1, 1, 1);

    for(dint i = 0; i < count; ++i, out++, normalIt++)
    {
        Vector3f const normal(normalIt->xyz);

        // Accumulate contributions from all affecting lights.
        Vector3f accum[2];  // Begin with total darkness [color, extra].
        dint numProcessed = 0;
        rendSys().forAllVectorLights(lightListIdx, [&maxLights, &normal
                                                      , &accum, &numProcessed] (VectorLightData const &vlight)
        {
            numProcessed += 1;

            dfloat strength = vlight.direction.dot(normal)
                            + vlight.offset;  // Shift toward the light a little.

            // Ability to both light and shade.
            if(strength > 0) strength *= vlight.lightSide;
            else             strength *= vlight.darkSide;

            accum[vlight.affectedByAmbient? 0 : 1]
                += vlight.color * de::clamp(-1.f, strength, 1.f);

            // Time to stop?
            return (maxLights && numProcessed == maxLights);
        });

        // Check for ambient and convert to ubyte.
        Vector3f color = (accum[0].max(ambient) + accum[1]).min(saturated);

        out->rgba[0] = dbyte( 255 * color.x );
        out->rgba[1] = dbyte( 255 * color.y );
        out->rgba[2] = dbyte( 255 * color.z );
        out->rgba[3] = dbyte( 255 * ambient[3] );
    }
}

MaterialVariantSpec const &PSprite_MaterialSpec()
{
    return resSys().materialSpec(SpriteContext, 0, 0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                 1, -2, 0, false, true, true, false);
}

void Rend_DrawPSprite(rendpspriteparams_t const &parms)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(::renderTextures == 1)
    {
        GL_SetPSprite(parms.mat, 0, 0);
        glEnable(GL_TEXTURE_2D);
    }
    else if(::renderTextures == 2)
    {
        // For lighting debug, render all solid surfaces using the gray texture.
        MaterialAnimator &matAnimator = resSys().material(de::Uri("System", Path("gray")))
                                                    .getAnimator(PSprite_MaterialSpec());

        // Ensure we have up to date info about the material.
        matAnimator.prepare();

        GL_BindTexture(matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture);
        glEnable(GL_TEXTURE_2D);
    }

    //  0---1
    //  |   |  Vertex layout.
    //  3---2

    dfloat v1[] = { parms.pos[0],               parms.pos[1] };
    dfloat v2[] = { parms.pos[0] + parms.width, parms.pos[1] };
    dfloat v3[] = { parms.pos[0] + parms.width, parms.pos[1] + parms.height };
    dfloat v4[] = { parms.pos[0],               parms.pos[1] + parms.height };

    // All psprite vertices are co-plannar, so just copy the view front vector.
    // @todo: Can we do something better here?
    Vector3f const &frontVec = R_ViewData(viewPlayer - ddPlayers)->frontVec;
    dgl_vertex_t quadNormals[4];
    for(dint i = 0; i < 4; ++i)
    {
        quadNormals[i].xyz[0] = frontVec.x;
        quadNormals[i].xyz[1] = frontVec.z;
        quadNormals[i].xyz[2] = frontVec.y;
    }

    dgl_color_t quadColors[4];
    if(!parms.vLightListIdx)
    {
        // Lit uniformly.
        applyUniformColor(4, quadColors, parms.ambientColor);
    }
    else
    {
        // Lit normally.
        Spr_VertexColors(4, quadColors, quadNormals, parms.vLightListIdx, spriteLight + 1, parms.ambientColor);
    }

    dgl_texcoord_t tcs[4], *tc = tcs;
    dgl_color_t *c = quadColors;

    tc[0].st[0] = parms.texOffset[0] *  (parms.texFlip[0]? 1:0);
    tc[0].st[1] = parms.texOffset[1] *  (parms.texFlip[1]? 1:0);
    tc[1].st[0] = parms.texOffset[0] * (!parms.texFlip[0]? 1:0);
    tc[1].st[1] = parms.texOffset[1] *  (parms.texFlip[1]? 1:0);
    tc[2].st[0] = parms.texOffset[0] * (!parms.texFlip[0]? 1:0);
    tc[2].st[1] = parms.texOffset[1] * (!parms.texFlip[1]? 1:0);
    tc[3].st[0] = parms.texOffset[0] *  (parms.texFlip[0]? 1:0);
    tc[3].st[1] = parms.texOffset[1] * (!parms.texFlip[1]? 1:0);

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

MaterialVariantSpec const &Rend_SpriteMaterialSpec(dint tclass, dint tmap)
{
    return resSys().materialSpec(SpriteContext, 0, 1, tclass, tmap, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                 1, -2, -1, true, true, true, false);
}

void Rend_DrawSprite(vissprite_t const &spr)
{
    drawspriteparams_t const &parm = *VS_SPRITE(&spr);

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    TextureVariant *tex = nullptr;
    Vector2f size;
    dfloat viewOffsetX = 0;  ///< View-aligned offset to center point.
    dfloat s = 1, t = 1;     ///< Bottom right coords.

    // Many sprite properties are inherited from the material.
    if(MaterialAnimator *matAnimator = parm.matAnimator)
    {
        // Ensure we have up to date info about the material.
        matAnimator->prepare();

        tex = matAnimator->texUnit(MaterialAnimator::TU_LAYER0).texture;
        dint const texBorder = tex->spec().variant.border;

        size        = matAnimator->dimensions() + Vector2i(texBorder * 2, texBorder * 2);
        viewOffsetX = -size.x / 2 + -tex->base().origin().x;

        tex->glCoords(&s, &t);
    }

    // We may want to draw using another material variant instead.
    if(renderTextures == 2)
    {
        // For lighting debug, render all solid surfaces using the gray texture.
        Material &debugMaterial       = resSys().material(de::Uri("System", Path("gray")));
        MaterialAnimator &matAnimator = debugMaterial.getAnimator(Rend_SpriteMaterialSpec());

        // Ensure we have up to date info about the material.
        matAnimator.prepare();

        tex = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;
    }

    if(renderTextures)
    {
        GL_BindTexture(tex);
        glEnable(GL_TEXTURE_2D);
    }
    else
    {
        GL_SetNoTexture();
    }

    // Coordinates to the center of the sprite (game coords).
    coord_t spriteCenter[3] = { spr.pose.origin[0] + spr.pose.srvo[0],
                                spr.pose.origin[1] + spr.pose.srvo[1],
                                spr.pose.origin[2] + spr.pose.srvo[2] };

    coord_t v1[3], v2[3], v3[3], v4[3];
    R_ProjectViewRelativeLine2D(spriteCenter, spr.pose.viewAligned,
                                size.x, viewOffsetX, v1, v4);

    v2[0] = v1[0];
    v2[1] = v1[1];
    v3[0] = v4[0];
    v3[1] = v4[1];

    v1[2] = v4[2] = spriteCenter[2] - size.y / 2;
    v2[2] = v3[2] = spriteCenter[2] + size.y / 2;

    // Calculate the surface normal.
    coord_t surfaceNormal[3];
    V3d_PointCrossProduct(surfaceNormal, v2, v1, v3);
    V3d_Normalize(surfaceNormal);

/*#if _DEBUG
    // Draw the surface normal.
    glBegin(GL_LINES);
    glColor4f(1, 0, 0, 1);
    glVertex3f(spriteCenter[0], spriteCenter[2], spriteCenter[1]);
    glColor4f(1, 0, 0, 0);
    glVertex3f(spriteCenter[0] + surfaceNormal[0] * 10,
               spriteCenter[2] + surfaceNormal[2] * 10,
               spriteCenter[1] + surfaceNormal[1] * 10);
    glEnd();
#endif*/

    // All sprite vertices are co-plannar, so just copy the surface normal.
    // @todo: Can we do something better here?
    dgl_color_t quadColors[4];
    dgl_vertex_t quadNormals[4];
    for(dint i = 0; i < 4; ++i)
    {
        V3f_Copyd(quadNormals[i].xyz, surfaceNormal);
    }

    if(!spr.light.vLightListIdx)
    {
        // Lit uniformly.
        applyUniformColor(4, quadColors, &spr.light.ambientColor[0]);
    }
    else
    {
        // Lit normally.
        Spr_VertexColors(4, quadColors, quadNormals, spr.light.vLightListIdx,
                         spriteLight + 1, &spr.light.ambientColor[0]);
    }

    // Do we need to do some aligning?
    bool restoreMatrix = false;
    bool restoreZ      = false;
    if(spr.pose.viewAligned || alwaysAlign >= 2)
    {
        // We must set up a modelview transformation matrix.
        restoreMatrix = true;
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        // Rotate around the center of the sprite.
        glTranslatef(spriteCenter[0], spriteCenter[2], spriteCenter[1]);
        if(!spr.pose.viewAligned)
        {
            dfloat s_dx = v1[0] - v2[0];
            dfloat s_dy = v1[1] - v2[1];

            if(alwaysAlign == 2)
            {
                // Restricted camera alignment.
                dfloat dx = spriteCenter[0] - vOrigin.x;
                dfloat dy = spriteCenter[1] - vOrigin.z;
                dfloat spriteAngle = BANG2DEG(
                    bamsAtan2(spriteCenter[2] - vOrigin.y, std::sqrt(dx * dx + dy * dy)));

                if(spriteAngle > 180)
                    spriteAngle -= 360;

                if(fabs(spriteAngle) > maxSpriteAngle)
                {
                    dfloat turnAngle = (spriteAngle > 0? spriteAngle - maxSpriteAngle :
                                                         spriteAngle + maxSpriteAngle);

                    // Rotate along the sprite edge.
                    glRotatef(turnAngle, s_dx, 0, s_dy);
                }
            }
            else
            {
                // Restricted view plane alignment.
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
        glTranslatef(-spriteCenter[0], -spriteCenter[2], -spriteCenter[1]);
    }

    // Need to change blending modes?
    if(parm.blendMode != BM_NORMAL)
    {
        GL_BlendMode(parm.blendMode);
    }

    // Transparent sprites shouldn't be written to the Z buffer.
    if(parm.noZWrite || spr.light.ambientColor[3] < .98f ||
       !(parm.blendMode == BM_NORMAL || parm.blendMode == BM_ZEROALPHA))
    {
        restoreZ = true;
        glDepthMask(GL_FALSE);
    }

    dgl_vertex_t vs[4], *v = vs;
    dgl_texcoord_t tcs[4], *tc = tcs;

    //  1---2
    //  |   |  Vertex layout.
    //  0---3

    v[0].xyz[0] = v1[0];
    v[0].xyz[1] = v1[2];
    v[0].xyz[2] = v1[1];

    v[1].xyz[0] = v2[0];
    v[1].xyz[1] = v2[2];
    v[1].xyz[2] = v2[1];

    v[2].xyz[0] = v3[0];
    v[2].xyz[1] = v3[2];
    v[2].xyz[2] = v3[1];

    v[3].xyz[0] = v4[0];
    v[3].xyz[1] = v4[2];
    v[3].xyz[2] = v4[1];

    tc[0].st[0] = s *  (parm.matFlip[0]? 1:0);
    tc[0].st[1] = t * (!parm.matFlip[1]? 1:0);
    tc[1].st[0] = s *  (parm.matFlip[0]? 1:0);
    tc[1].st[1] = t *  (parm.matFlip[1]? 1:0);
    tc[2].st[0] = s * (!parm.matFlip[0]? 1:0);
    tc[2].st[1] = t *  (parm.matFlip[1]? 1:0);
    tc[3].st[0] = s * (!parm.matFlip[0]? 1:0);
    tc[3].st[1] = t * (!parm.matFlip[1]? 1:0);

    drawQuad(v, quadColors, tc);

    if(renderTextures)
    {
        glDisable(GL_TEXTURE_2D);
    }

    if(devMobjVLights && spr.light.vLightListIdx)
    {
        // Draw the vlight vectors, for debug.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(spr.pose.origin[0], spr.pose.origin[2], spr.pose.origin[1]);

        coord_t const distFromViewer = de::abs(spr.pose.distance);
        rendSys().forAllVectorLights(spr.light.vLightListIdx, [&distFromViewer] (VectorLightData const &vlight)
        {
            if(distFromViewer < 1600 - 8)
            {
                Rend_DrawVectorLight(vlight, 1 - distFromViewer / 1600);
            }
            return LoopContinue;
        });

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }

    // Need to restore the original modelview matrix?
    if(restoreMatrix)
    {
        glPopMatrix();
    }

    // Change back to normal blending?
    if(parm.blendMode != BM_NORMAL)
    {
        GL_BlendMode(BM_NORMAL);
    }

    // Enable Z-writing again?
    if(restoreZ)
    {
        glDepthMask(GL_TRUE);
    }
}

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
