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

#include "clientapp.h"
#include "render/billboard.h"

#include "de_render.h"
#include "de_graphics.h"

#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "Texture"
#include "resource/sprite.h"

#include "world/p_players.h" // viewPlayer, ddPlayers
#include <de/vector1.h>
#include <de/concurrency.h>
#include <doomsday/console/var.h>

using namespace de;

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

    GLenum normalTarget, dynTarget;

    Texture::Variant *tex = 0;
    if(renderTextures)
    {
        MaterialSnapshot const &ms =
            reinterpret_cast<MaterialVariant *>(parms.material)->prepare();
        tex = &ms.texture(MTU_PRIMARY);
    }

    // Do we have a dynamic light to blend with?
    // This only happens when multitexturing is enabled.
    bool withDyn = false;
    int normal = 0, dyn = 1;
    if(parms.modTex && numTexUnits > 1)
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

    normalTarget = normal? GL_TEXTURE1 : GL_TEXTURE0;
    dynTarget    =    dyn? GL_TEXTURE1 : GL_TEXTURE0;

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

            glVertex3f(parms.vertices[0].pos[VX],
                       parms.vertices[0].pos[VZ],
                       parms.vertices[0].pos[VY]);

            glColor4fv(parms.vertices[1].color);
            glMultiTexCoord2f(normalTarget, parms.texCoord[0][0], parms.texCoord[0][1]);

            glMultiTexCoord2f(dynTarget, parms.modTexCoord[0][0], parms.modTexCoord[0][1]);

            glVertex3f(parms.vertices[1].pos[VX],
                       parms.vertices[1].pos[VZ],
                       parms.vertices[1].pos[VY]);

            glColor4fv(parms.vertices[3].color);
            glMultiTexCoord2f(normalTarget, parms.texCoord[1][0], parms.texCoord[0][1]);

            glMultiTexCoord2f(dynTarget, parms.modTexCoord[1][0], parms.modTexCoord[0][1]);

            glVertex3f(parms.vertices[3].pos[VX],
                       parms.vertices[3].pos[VZ],
                       parms.vertices[3].pos[VY]);

            glColor4fv(parms.vertices[2].color);
            glMultiTexCoord2f(normalTarget, parms.texCoord[1][0], parms.texCoord[1][1]);

            glMultiTexCoord2f(dynTarget, parms.modTexCoord[1][0], parms.modTexCoord[1][1]);

            glVertex3f(parms.vertices[2].pos[VX],
                       parms.vertices[2].pos[VZ],
                       parms.vertices[2].pos[VY]);
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

            glVertex3f(parms.vertices[0].pos[VX],
                       parms.vertices[0].pos[VZ],
                       parms.vertices[0].pos[VY]);

            glColor4fv(parms.vertices[1].color);
            glTexCoord2f(parms.texCoord[0][0], parms.texCoord[0][1]);

            glVertex3f(parms.vertices[1].pos[VX],
                       parms.vertices[1].pos[VZ],
                       parms.vertices[1].pos[VY]);

            glColor4fv(parms.vertices[3].color);
            glTexCoord2f(parms.texCoord[1][0], parms.texCoord[0][1]);

            glVertex3f(parms.vertices[3].pos[VX],
                       parms.vertices[3].pos[VZ],
                       parms.vertices[3].pos[VY]);

            glColor4fv(parms.vertices[2].color);
            glTexCoord2f(parms.texCoord[1][0], parms.texCoord[1][1]);

            glVertex3f(parms.vertices[2].pos[VX],
                       parms.vertices[2].pos[VZ],
                       parms.vertices[2].pos[VY]);
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    GL_BlendMode(BM_NORMAL);
}

/**
 * Set all the colors in the array to that specified.
 */
static void applyUniformColor(int count, dgl_color_t *colors, float const *rgba)
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
static void Spr_VertexColors(int count, dgl_color_t *out, dgl_vertex_t *normal,
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

MaterialVariantSpec const &PSprite_MaterialSpec()
{
    return ClientApp::resourceSystem().materialSpec(SpriteContext, 0, 0, 0, 0,
                                                    GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                                    1, -2, 0, false, true, true, false);
}

void Rend_DrawPSprite(rendpspriteparams_t const &parms)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(renderTextures == 1)
    {
        GL_SetPSprite(parms.mat, 0, 0);
        glEnable(GL_TEXTURE_2D);
    }
    else if(renderTextures == 2)
    {
        // For lighting debug, render all solid surfaces using the gray texture.
        MaterialSnapshot const &ms =
            ClientApp::resourceSystem().material(de::Uri("System", Path("gray")))
                      .prepare(PSprite_MaterialSpec());

        GL_BindTexture(&ms.texture(MTU_PRIMARY));
        glEnable(GL_TEXTURE_2D);
    }

    //  0---1
    //  |   |  Vertex layout.
    //  3---2

    float v1[2], v2[2], v3[2], v4[2];
    v1[VX] = parms.pos[VX];
    v1[VY] = parms.pos[VY];

    v2[VX] = parms.pos[VX] + parms.width;
    v2[VY] = parms.pos[VY];

    v3[VX] = parms.pos[VX] + parms.width;
    v3[VY] = parms.pos[VY] + parms.height;

    v4[VX] = parms.pos[VX];
    v4[VY] = parms.pos[VY] + parms.height;

    // All psprite vertices are co-plannar, so just copy the view front vector.
    // @todo: Can we do something better here?
    Vector3f const &frontVec = R_ViewData(viewPlayer - ddPlayers)->frontVec;
    dgl_vertex_t quadNormals[4];
    for(int i = 0; i < 4; ++i)
    {
        quadNormals[i].xyz[VX] = frontVec.x;
        quadNormals[i].xyz[VY] = frontVec.z;
        quadNormals[i].xyz[VZ] = frontVec.y;
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

MaterialVariantSpec const &Rend_SpriteMaterialSpec(int tclass, int tmap)
{
    return ClientApp::resourceSystem().materialSpec(SpriteContext, 0, 1, tclass, tmap,
                                                    GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                                    1, -2, -1, true, true, true, false);
}

static MaterialVariant *chooseSpriteMaterial(drawspriteparams_t const &p)
{
    if(!renderTextures) return 0;
    if(renderTextures == 2)
    {
        // For lighting debug, render all solid surfaces using the gray texture.
        return ClientApp::resourceSystem().material(de::Uri("System", Path("gray")))
                    .chooseVariant(Rend_SpriteMaterialSpec(), true);
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

void Rend_DrawSprite(drawspriteparams_t const &parms)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    bool restoreMatrix = false;
    bool restoreZ = false;

    Point2Rawf viewOffset; ///< View-aligned offset to center point.
    Size2Rawf size;

    MaterialSnapshot const *ms = 0;
    float s = 1, t = 1; ///< Bottom right coords.

    // Many sprite properties are inherited from the material.
    if(parms.material)
    {
        // Ensure this variant has been prepared.
        ms = &reinterpret_cast<MaterialVariant *>(parms.material)->prepare();

        variantspecification_t const &texSpec = ms->texture(MTU_PRIMARY).spec().variant;
        size.width  = ms->width() + texSpec.border * 2;
        size.height = ms->height() + texSpec.border * 2;
        viewOffset.x = -size.width / 2;

        ms->texture(MTU_PRIMARY).glCoords(&s, &t);

        Texture &tex = ms->texture(MTU_PRIMARY).generalCase();
        viewOffset.x += float(-tex.origin().x);
    }

    // We may want to draw using another material instead.
    MaterialVariant *mat = chooseSpriteMaterial(parms);
    if(mat != reinterpret_cast<MaterialVariant *>(parms.material))
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
    coord_t spriteCenter[3] = { parms.center[VX] + parms.srvo[VX],
                                parms.center[VY] + parms.srvo[VY],
                                parms.center[VZ] + parms.srvo[VZ] };

    coord_t v1[3], v2[3], v3[3], v4[3];
    R_ProjectViewRelativeLine2D(spriteCenter, parms.viewAligned,
                                size.width, viewOffset.x, v1, v4);

    v2[VX] = v1[VX];
    v2[VY] = v1[VY];
    v3[VX] = v4[VX];
    v3[VY] = v4[VY];

    v1[VZ] = v4[VZ] = spriteCenter[VZ] - size.height / 2 + viewOffset.y;
    v2[VZ] = v3[VZ] = spriteCenter[VZ] + size.height / 2 + viewOffset.y;

    // Calculate the surface normal.
    coord_t surfaceNormal[3];
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
    dgl_color_t quadColors[4];
    dgl_vertex_t quadNormals[4];
    for(int i = 0; i < 4; ++i)
    {
        V3f_Copyd(quadNormals[i].xyz, surfaceNormal);
    }

    if(!parms.vLightListIdx)
    {
        // Lit uniformly.
        applyUniformColor(4, quadColors, parms.ambientColor);
    }
    else
    {
        // Lit normally.
        Spr_VertexColors(4, quadColors, quadNormals, parms.vLightListIdx,
                         spriteLight + 1, parms.ambientColor);
    }

    // Do we need to do some aligning?
    if(parms.viewAligned || alwaysAlign >= 2)
    {
        // We must set up a modelview transformation matrix.
        restoreMatrix = true;
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        // Rotate around the center of the sprite.
        glTranslatef(spriteCenter[VX], spriteCenter[VZ], spriteCenter[VY]);
        if(!parms.viewAligned)
        {
            float s_dx = v1[VX] - v2[VX];
            float s_dy = v1[VY] - v2[VY];

            if(alwaysAlign == 2)
            {   // Restricted camera alignment.
                float dx = spriteCenter[VX] - vOrigin.x;
                float dy = spriteCenter[VY] - vOrigin.z;
                float spriteAngle = BANG2DEG(
                    bamsAtan2(spriteCenter[VZ] - vOrigin.y, sqrt(dx * dx + dy * dy)));

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
    if(parms.blendMode != BM_NORMAL)
    {
        GL_BlendMode(parms.blendMode);
    }

    // Transparent sprites shouldn't be written to the Z buffer.
    if(parms.noZWrite || parms.ambientColor[CA] < .98f ||
       !(parms.blendMode == BM_NORMAL || parms.blendMode == BM_ZEROALPHA))
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

    tc[0].st[0] = s *  (parms.matFlip[0]? 1:0);
    tc[0].st[1] = t * (!parms.matFlip[1]? 1:0);
    tc[1].st[0] = s *  (parms.matFlip[0]? 1:0);
    tc[1].st[1] = t *  (parms.matFlip[1]? 1:0);
    tc[2].st[0] = s * (!parms.matFlip[0]? 1:0);
    tc[2].st[1] = t *  (parms.matFlip[1]? 1:0);
    tc[3].st[0] = s * (!parms.matFlip[0]? 1:0);
    tc[3].st[1] = t * (!parms.matFlip[1]? 1:0);

    drawQuad(v, quadColors, tc);

    if(ms)
    {
        glDisable(GL_TEXTURE_2D);
    }

    if(devMobjVLights && parms.vLightListIdx)
    {
        // Draw the vlight vectors, for debug.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(parms.center[VX], parms.center[VZ], parms.center[VY]);

        coord_t distFromViewer = de::abs(parms.distance);
        VL_ListIterator(parms.vLightListIdx, drawVectorLightWorker, &distFromViewer);

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
    if(parms.blendMode != BM_NORMAL)
    {
        GL_BlendMode(BM_NORMAL);
    }

    // Enable Z-writing again?
    if(restoreZ)
    {
        glDepthMask(GL_TRUE);
    }
}
