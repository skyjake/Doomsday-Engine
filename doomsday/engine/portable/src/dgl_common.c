/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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
 * dgl_common.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void GL_SetGrayMipmap(int lev)
{
    GL_state_texture.grayMipmapFactor = lev / 255.0f;
}

/**
 * Set the currently active GL texture unit by ident.
 *
 * @param texture       GL texture unit ident to make active.
 */
void GL_ActiveTexture(const GLenum texture)
{
#ifdef WIN32
    if(!glActiveTextureARB)
        return;
    glActiveTextureARB(texture);
#else
# ifdef USE_MULTITEXTURE
    glActiveTextureARB(texture);
# endif
#endif
}

/**
 * Requires a texture environment mode that can add and multiply.
 * Nvidia's and ATI's appropriate extensions are supported, other cards will
 * not be able to utilize multitextured lights.
 */
void envAddColoredAlpha(int activate, GLenum addFactor)
{
    if(activate)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
                  GL_state_ext.nvTexEnvComb ? GL_COMBINE4_NV : GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);

        // Combine: texAlpha * constRGB + 1 * prevRGB.
        if(GL_state_ext.nvTexEnvComb)
        {
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, addFactor);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_ZERO);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_ONE_MINUS_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_COLOR);
        }
        else if(GL_state_ext.atiTexEnvComb)
        {   // MODULATE_ADD_ATI: Arg0 * Arg2 + Arg1.
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE_ADD_ATI);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, addFactor);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        }
        else
        {   // This doesn't look right.
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, addFactor);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        }
    }
    else
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
}

/**
 * Setup the texture environment for single-pass multiplicative lighting.
 * The last texture unit is always used for the texture modulation.
 * TUs 1...n-1 are used for dynamic lights.
 */
void envModMultiTex(int activate)
{
    // Setup TU 2: The modulated texture.
    GL_ActiveTexture(GL_TEXTURE1);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // Setup TU 1: The dynamic light.
    GL_ActiveTexture(GL_TEXTURE0);
    envAddColoredAlpha(activate, GL_SRC_ALPHA);

    // This is a single-pass mode. The alpha should remain unmodified
    // during the light stage.
    if(activate)
    {   // Replace: primAlpha.
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    }
}

/**
 * Configure the GL state for the specified texture modulation mode.
 *
 * @param mode          Modulation mode ident.
 */
void GL_ModulateTexture(int mode)
{
    switch(mode)
    {
    case 0:
        // No modulation: just replace with texture.
        GL_ActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        break;

    case 1:
        // Normal texture modulation with primary color.
        GL_ActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        break;

    case 12:
        // Normal texture modulation on both stages. TU 1 modulates with
        // primary color, TU 2 with TU 1.
        GL_ActiveTexture(GL_TEXTURE1);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        GL_ActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        break;

    case 2:
    case 3:
        // Texture modulation and interpolation.
        GL_ActiveTexture(GL_TEXTURE1);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
        if(mode == 2)
        {   // Used with surfaces that have a color.
            // TU 2: Modulate previous with primary color.
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

        }
        else
        {   // Mode 3: Used with surfaces with no primary color.
            // TU 2: Pass through.
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        }
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

        // TU 1: Interpolate between texture 1 and 2, using the constant
        // alpha as the factor.
        GL_ActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE1);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);

        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        break;

    case 4:
        // Apply sector light, dynamic light and texture.
        envModMultiTex(true);
        break;

    case 5:
    case 10:
        // Sector light * texture + dynamic light.
        GL_ActiveTexture(GL_TEXTURE1);
        envAddColoredAlpha(true, mode == 5 ? GL_SRC_ALPHA : GL_SRC_COLOR);

        // Alpha remains unchanged.
        if(GL_state_ext.nvTexEnvComb)
        {
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_ADD);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_ZERO);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,
                      GL_ONE_MINUS_SRC_ALPHA);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA, GL_ZERO);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA, GL_SRC_ALPHA);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_ZERO);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA);
        }
        else
        {
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        }

        GL_ActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        break;

    case 6:
        // Simple dynlight addition (add to primary color).
        GL_ActiveTexture(GL_TEXTURE0);
        envAddColoredAlpha(true, GL_SRC_ALPHA);
        break;

    case 7:
        // Dynlight addition without primary color.
        GL_ActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
        break;

    case 8:
    case 9:
        // Texture and Detail.
        GL_ActiveTexture(GL_TEXTURE1);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 2);

        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

        GL_ActiveTexture(GL_TEXTURE0);
        if(mode == 8)
        {
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        }
        else
        {   // Mode 9: Ignore primary color.
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        }
        break;

    case 11:
        // Normal modulation, alpha of 2nd stage.
        // Tex0: texture
        // Tex1: shiny texture
        GL_ActiveTexture(GL_TEXTURE1);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

        GL_ActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE1);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
        break;

    default:
        break;
    }
}

void GL_BlendOp(int op)
{
#ifndef UNIX
    if(!glBlendEquationEXT)
        return;
#endif
    glBlendEquationEXT(op);
}

boolean GL_Grab(int x, int y, int width, int height, dgltexformat_t format, void *buffer)
{
    if(format != DGL_RGB)
        return false;

    // y+height-1 is the bottom edge of the rectangle. It's
    // flipped to change the origin.
    glReadPixels(x, FLIP(y + height - 1), width, height, GL_RGB,
                 GL_UNSIGNED_BYTE, buffer);
    return true;
}

void GL_EnableTexUnit(byte id)
{
    GL_ActiveTexture(GL_TEXTURE0 + id);
    glEnable(GL_TEXTURE_2D);
}

void GL_DisableTexUnit(byte id)
{
    GL_ActiveTexture(GL_TEXTURE0 + id);
    glDisable(GL_TEXTURE_2D);

    // Implicit disabling of texcoord array.
    if(GL_state.noArrays)
    {
        GL_DisableArrays(0, 0, 1 << id);
    }
}

/**
 * The first selected unit is active after this call.
 */
void GL_SelectTexUnits(int count)
{
    int                 i;

    // Disable extra units.
    for(i = numTexUnits - 1; i >= count; i--)
        GL_DisableTexUnit(i);

    // Enable the selected units.
    for(i = count - 1; i >= 0; i--)
    {
        if(i >= numTexUnits)
            continue;

        GL_EnableTexUnit(i);
    }
}

void GL_SetTextureCompression(boolean on)
{
    GL_state.allowCompression = on;
}

void GL_SetVSync(boolean on)
{
#ifdef WIN32
    if(GL_state_ext.wglSwapIntervalEXT)
    {
        wglSwapIntervalEXT(on? 1 : 0);
        GL_state.useVSync = on;
    }
#endif
}

void GL_SetMultisample(boolean on)
{
#if WIN32
    if(on)
        glEnable(GL_MULTISAMPLE_ARB);
    else
        glDisable(GL_MULTISAMPLE_ARB);
#endif
}

void DGL_Scissor(int x, int y, int width, int height)
{
    glScissor(x, FLIP(y + height - 1), width, height);
}

boolean DGL_GetIntegerv(int name, int *v)
{
    int         i;
    float       color[4];

    switch(name)
    {
    case DGL_MODULATE_ADD_COMBINE:
        *v = GL_state_ext.nvTexEnvComb || GL_state_ext.atiTexEnvComb;
        break;

    case DGL_SCISSOR_TEST:
        glGetIntegerv(GL_SCISSOR_TEST, (GLint*) v);
        break;

    case DGL_SCISSOR_BOX:
        glGetIntegerv(GL_SCISSOR_BOX, (GLint*) v);
        v[1] = FLIP(v[1] + v[3] - 1);
        break;

    case DGL_FOG:
        *v = GL_state.useFog;
        break;

    case DGL_CURRENT_COLOR_R:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = (int) (color[0] * 255);
        break;

    case DGL_CURRENT_COLOR_G:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = (int) (color[1] * 255);
        break;

    case DGL_CURRENT_COLOR_B:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = (int) (color[2] * 255);
        break;

    case DGL_CURRENT_COLOR_A:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = (int) (color[3] * 255);
        break;

    case DGL_CURRENT_COLOR_RGBA:
        glGetFloatv(GL_CURRENT_COLOR, color);
        for(i = 0; i < 4; i++)
            v[i] = (int) (color[i] * 255);
        break;

    default:
        return false;
    }

    return true;
}

int DGL_GetInteger(int name)
{
    int         values[10];

    DGL_GetIntegerv(name, values);
    return values[0];
}

boolean DGL_SetInteger(int name, int value)
{
    switch(name)
    {
    case DGL_ACTIVE_TEXTURE:
        GL_ActiveTexture(GL_TEXTURE0 + (byte) value);
        break;

    case DGL_MODULATE_TEXTURE:
        GL_ModulateTexture(value);
        break;

    default:
        return false;
    }

    return true;
}

float DGL_GetFloat(int name)
{
    switch(name)
    {
    case DGL_LINE_WIDTH:
        return GL_state.currentLineWidth;

    case DGL_POINT_SIZE:
        return GL_state.currentPointSize;

    default:
        return 0;
    }

    return 1;
}

boolean DGL_SetFloat(int name, float value)
{
    switch(name)
    {
    case DGL_LINE_WIDTH:
        GL_state.currentLineWidth = value;
        glLineWidth(value);
        break;

    case DGL_POINT_SIZE:
        GL_state.currentPointSize = value;
        glPointSize(value);
        break;

    default:
        return false;
    }

    return true;
}

void DGL_EnableTexUnit(byte id)
{
    GL_EnableTexUnit(id);
}

void DGL_DisableTexUnit(byte id)
{
    GL_DisableTexUnit(id);
}

int DGL_Enable(int cap)
{
    switch(cap)
    {
    case DGL_TEXTURING:
#ifndef DRMESA
        glEnable(GL_TEXTURE_2D);
#endif
        break;

    case DGL_FOG:
        glEnable(GL_FOG);
        GL_state.useFog = true;
        break;

    case DGL_SCISSOR_TEST:
        glEnable(GL_SCISSOR_TEST);
        break;

    case DGL_LINE_SMOOTH:
        glEnable(GL_LINE_SMOOTH);
        break;

    case DGL_POINT_SMOOTH:
        glEnable(GL_POINT_SMOOTH);
        break;

    default:
        return 0;
    }

    return 1;
}

void DGL_Disable(int cap)
{
    switch(cap)
    {
    case DGL_TEXTURING:
        glDisable(GL_TEXTURE_2D);
        break;

    case DGL_FOG:
        glDisable(GL_FOG);
        GL_state.useFog = false;
        break;

    case DGL_SCISSOR_TEST:
        glDisable(GL_SCISSOR_TEST);
        break;

    case DGL_LINE_SMOOTH:
        glDisable(GL_LINE_SMOOTH);
        break;

    case DGL_POINT_SMOOTH:
        glDisable(GL_POINT_SMOOTH);
        break;

    default:
        break;
    }
}

void DGL_BlendOp(int op)
{
    GL_BlendOp(op == DGL_SUBTRACT ? GL_FUNC_SUBTRACT : op ==
               DGL_REVERSE_SUBTRACT ? GL_FUNC_REVERSE_SUBTRACT :
               GL_FUNC_ADD);
}

void DGL_BlendFunc(int param1, int param2)
{
    glBlendFunc(param1 == DGL_ZERO ? GL_ZERO : param1 ==
                DGL_ONE ? GL_ONE : param1 ==
                DGL_DST_COLOR ? GL_DST_COLOR : param1 ==
                DGL_ONE_MINUS_DST_COLOR ? GL_ONE_MINUS_DST_COLOR : param1
                == DGL_SRC_ALPHA ? GL_SRC_ALPHA : param1 ==
                DGL_ONE_MINUS_SRC_ALPHA ? GL_ONE_MINUS_SRC_ALPHA : param1
                == DGL_DST_ALPHA ? GL_DST_ALPHA : param1 ==
                DGL_ONE_MINUS_DST_ALPHA ? GL_ONE_MINUS_DST_ALPHA : param1
                ==
                DGL_SRC_ALPHA_SATURATE ? GL_SRC_ALPHA_SATURATE : GL_ZERO,
                param2 == DGL_ZERO ? GL_ZERO : param2 ==
                DGL_ONE ? GL_ONE : param2 ==
                DGL_SRC_COLOR ? GL_SRC_COLOR : param2 ==
                DGL_ONE_MINUS_SRC_COLOR ? GL_ONE_MINUS_SRC_COLOR : param2
                == DGL_SRC_ALPHA ? GL_SRC_ALPHA : param2 ==
                DGL_ONE_MINUS_SRC_ALPHA ? GL_ONE_MINUS_SRC_ALPHA : param2
                == DGL_DST_ALPHA ? GL_DST_ALPHA : param2 ==
                DGL_ONE_MINUS_DST_ALPHA ? GL_ONE_MINUS_DST_ALPHA :
                GL_ZERO);
}

void DGL_BlendMode(blendmode_t mode)
{
    GL_BlendMode(mode);
}

void DGL_MatrixMode(int mode)
{
    glMatrixMode(mode == DGL_PROJECTION ? GL_PROJECTION :
                 mode == DGL_TEXTURE ? GL_TEXTURE :
                 GL_MODELVIEW);
}

void DGL_PushMatrix(void)
{
    glPushMatrix();

#if _DEBUG
if(glGetError() == GL_STACK_OVERFLOW)
    Con_Error("DG_PushMatrix: Stack overflow.\n");
#endif
}

void DGL_SetMaterial(material_t* mat)
{
    GL_SetMaterial(mat);
}

void DGL_SetNoMaterial(void)
{
    GL_SetNoTexture();
}

void DGL_SetPatch(patchid_t id, int wrapS, int wrapT)
{
    GL_BindTexture(GL_PreparePatch(R_FindPatchTex(id)), (filterUI ? GL_LINEAR : GL_NEAREST));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (wrapS == DGL_CLAMP? GL_CLAMP : wrapS == DGL_CLAMP_TO_EDGE? GL_CLAMP_TO_EDGE : GL_REPEAT));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (wrapT == DGL_CLAMP? GL_CLAMP : wrapT == DGL_CLAMP_TO_EDGE? GL_CLAMP_TO_EDGE : GL_REPEAT));
}

void DGL_SetTranslatedSprite(material_t* mat, int tclass, int tmap)
{
    GL_SetTranslatedSprite(mat, tclass, tmap);
}

void DGL_SetPSprite(material_t* mat)
{
    GL_SetPSprite(mat);
}

void DGL_SetRawImage(lumpnum_t lump, int wrapS, int wrapT)
{
    GL_SetRawImage(lump,
        (wrapS == DGL_CLAMP? GL_CLAMP :
         wrapS == DGL_CLAMP_TO_EDGE? GL_CLAMP_TO_EDGE : GL_REPEAT),
        (wrapT == DGL_CLAMP? GL_CLAMP :
         wrapT == DGL_CLAMP_TO_EDGE? GL_CLAMP_TO_EDGE : GL_REPEAT));
}

void DGL_PopMatrix(void)
{
    glPopMatrix();

#if _DEBUG
if(glGetError() == GL_STACK_UNDERFLOW)
    Con_Error("DG_PopMatrix: Stack underflow.\n");
#endif
}

void DGL_LoadIdentity(void)
{
    glLoadIdentity();
}

void DGL_Translatef(float x, float y, float z)
{
    glTranslatef(x, y, z);
}

void DGL_Rotatef(float angle, float x, float y, float z)
{
    glRotatef(angle, x, y, z);
}

void DGL_Scalef(float x, float y, float z)
{
    glScalef(x, y, z);
}

void DGL_Ortho(float left, float top, float right, float bottom, float znear,
               float zfar)
{
    glOrtho(left, right, bottom, top, znear, zfar);
}

void DGL_DeleteTextures(int num, const DGLuint *names)
{
    if(!num || !names)
        return;

    glDeleteTextures(num, (const GLuint*) names);
}

int DGL_Bind(DGLuint texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
#ifdef _DEBUG
    Sys_CheckGLError();
#endif
    return 0;
}

#if 0 // Currently unused.
int DGL_Project(int num, dgl_fc3vertex_t *inVertices,
               dgl_fc3vertex_t *outVertices)
{
    GLdouble    modelMatrix[16], projMatrix[16];
    GLint       viewport[4];
    GLdouble    x, y, z;
    int         i, numOut;
    dgl_fc3vertex_t *in = inVertices, *out = outVertices;

    if(num == 0)
        return 0;

    // Get the data we'll need in the operation.
    glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
    glGetIntegerv(GL_VIEWPORT, viewport);
    for(i = numOut = 0; i < num; ++i, in++)
    {
        if(gluProject
           (in->pos[VX], in->pos[VY], in->pos[VZ], modelMatrix, projMatrix,
            viewport, &x, &y, &z) == GL_TRUE)
        {
            // A success: add to the out vertices.
            out->pos[VX] = (float) x;
            out->pos[VY] = (float) FLIP(y);
            out->pos[VZ] = (float) z;

            // Check that it's truly visible.
            if(out->pos[VX] < 0 || out->pos[VY] < 0 ||
               out->pos[VX] >= theWindow->width ||
               out->pos[VY] >= theWindow->height)
                continue;

            memcpy(out->color, in->color, sizeof(in->color));
            numOut++;
            out++;
        }
    }
    return numOut;
}
#endif

/**
 * \todo No need for this special method now. Refactor callers to use the
 * normal DGL drawing methods.
 */
void DGL_DrawRawScreen(lumpnum_t lump, int x, int y)
{
    rawtex_t* raw;

    if(lump < 0 || lump >= numLumps)
        return;

    GL_SetRawImage(lump, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    raw = R_GetRawTex(lump);

    // The first part is rendered in any case.
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2f(x, y);
        glTexCoord2f(1, 0);
        glVertex2f(x + raw->width, y);
        glTexCoord2f(1, 1);
        glVertex2f(x + raw->width, y + raw->height);
        glTexCoord2f(0, 1);
        glVertex2f(x, y + raw->height);
    glEnd();
}
