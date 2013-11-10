/** @file dgl_common.cpp Misc Drawing Routines
 * @ingroup gl
 *
 * @authors Copyright &copy; 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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

#define DENG_NO_API_MACROS_GL

#include <cstdlib>
#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_filesys.h"
#include "de_resource.h"
#include "render/r_draw.h"
#include "gl/sys_opengl.h"
#include "api_gl.h"

#include <de/GLState>

using namespace de;

/**
 * Requires a texture environment mode that can add and multiply.
 * Nvidia's and ATI's appropriate extensions are supported, other cards will
 * not be able to utilize multitextured lights.
 */
static void envAddColoredAlpha(int activate, GLenum addFactor)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(activate)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
                  GL_state.extensions.texEnvCombNV ? GL_COMBINE4_NV : GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);

        // Combine: texAlpha * constRGB + 1 * prevRGB.
        if(GL_state.extensions.texEnvCombNV)
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
        else if(GL_state.extensions.texEnvCombATI)
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
static void envModMultiTex(int activate)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Setup TU 2: The modulated texture.
    glActiveTexture(GL_TEXTURE1);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // Setup TU 1: The dynamic light.
    glActiveTexture(GL_TEXTURE0);
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

void GL_ModulateTexture(int mode)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    switch(mode)
    {
    case 0:
        // No modulation: just replace with texture.
        glActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        break;

    case 1:
        // Normal texture modulation with primary color.
        glActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        break;

    case 12:
        // Normal texture modulation on both stages. TU 1 modulates with
        // primary color, TU 2 with TU 1.
        glActiveTexture(GL_TEXTURE1);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        break;

    case 2:
    case 3:
        // Texture modulation and interpolation.
        glActiveTexture(GL_TEXTURE1);
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
        glActiveTexture(GL_TEXTURE0);
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
        glActiveTexture(GL_TEXTURE1);
        envAddColoredAlpha(true, mode == 5 ? GL_SRC_ALPHA : GL_SRC_COLOR);

        // Alpha remains unchanged.
        if(GL_state.extensions.texEnvCombNV)
        {
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_ADD);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_ZERO);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

        glActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        break;

    case 6:
        // Simple dynlight addition (add to primary color).
        glActiveTexture(GL_TEXTURE0);
        envAddColoredAlpha(true, GL_SRC_ALPHA);
        break;

    case 7:
        // Dynlight addition without primary color.
        glActiveTexture(GL_TEXTURE0);
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
        glActiveTexture(GL_TEXTURE1);
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

        glActiveTexture(GL_TEXTURE0);
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
        glActiveTexture(GL_TEXTURE1);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

        glActiveTexture(GL_TEXTURE0);
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

/*void GL_BlendOp(int op)
{
    if(!GL_state.features.blendSubtract)
        return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBlendEquationEXT(op);
}*/

void GL_SetVSync(boolean on)
{
    // Outside the main thread we'll need to defer the call.
    if(!Sys_InMainThread())
    {
        GL_DeferSetVSync(on);
        return;
    }

    if(!GL_state.features.vsync) return;

#ifdef WIN32
    wglSwapIntervalEXT(on? 1 : 0);
#endif

#ifdef MACOSX
    {
        // Tell CGL to wait for vertical refresh.
        CGLContextObj context = CGLGetCurrentContext();
        assert(context != 0);
        if(context)
        {
            GLint params[1] = { on? 1 : 0 };
            CGLSetParameter(context, kCGLCPSwapInterval, params);
        }
    }
#endif
}

void GL_SetMultisample(boolean on)
{
    if(!GL_state.features.multisample) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

#if defined(WIN32)
    if(on) glEnable(GL_MULTISAMPLE_ARB);
    else  glDisable(GL_MULTISAMPLE_ARB);
#else
    if(on) glEnable(GL_MULTISAMPLE);
    else  glDisable(GL_MULTISAMPLE);
#endif
}

#undef DGL_SetScissor
DENG_EXTERN_C void DGL_SetScissor(RectRaw const *rect)
{
    if(!rect) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    GLState::top().setNormalizedScissor(
                ClientWindow::main().game().normalizedRect(
                    Rectanglei(rect->origin.x, rect->origin.y,
                               rect->size.width, rect->size.height))).apply();
}

#undef DGL_SetScissor2
DENG_EXTERN_C void DGL_SetScissor2(int x, int y, int width, int height)
{
    RectRaw rect;
    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width  = width;
    rect.size.height = height;
    DGL_SetScissor(&rect);
}

#undef DGL_GetIntegerv
boolean DGL_GetIntegerv(int name, int *v)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    float color[4];
    switch(name)
    {
    case DGL_MODULATE_ADD_COMBINE:
        *v = GL_state.extensions.texEnvCombNV || GL_state.extensions.texEnvCombATI;
        break;

    case DGL_SCISSOR_TEST:
        //glGetIntegerv(GL_SCISSOR_TEST, (GLint*) v);
        *(GLint *) v = GLState::top().scissor();
        break;

    case DGL_FOG:
        *v = GL_state.currentUseFog;
        break;

    case DGL_CURRENT_COLOR_R:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = int(color[0] * 255);
        break;

    case DGL_CURRENT_COLOR_G:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = int(color[1] * 255);
        break;

    case DGL_CURRENT_COLOR_B:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = int(color[2] * 255);
        break;

    case DGL_CURRENT_COLOR_A:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = int(color[3] * 255);
        break;

    case DGL_CURRENT_COLOR_RGBA:
        glGetFloatv(GL_CURRENT_COLOR, color);
        for(int i = 0; i < 4; ++i)
        {
            v[i] = int(color[i] * 255);
        }
        break;

    default:
        return false;
    }

    return true;
}

#undef DGL_GetInteger
int DGL_GetInteger(int name)
{
    int values[10];
    DGL_GetIntegerv(name, values);
    return values[0];
}

#undef DGL_SetInteger
boolean DGL_SetInteger(int name, int value)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    switch(name)
    {
    case DGL_ACTIVE_TEXTURE:
        glActiveTexture(GL_TEXTURE0 + byte(value));
        break;

    case DGL_MODULATE_TEXTURE:
        GL_ModulateTexture(value);
        break;

    default:
        return false;
    }

    return true;
}

#undef DGL_GetFloatv
boolean DGL_GetFloatv(int name, float *v)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    float color[4];
    switch(name)
    {
    case DGL_CURRENT_COLOR_R:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = color[0];
        break;

    case DGL_CURRENT_COLOR_G:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = color[1];
        break;

    case DGL_CURRENT_COLOR_B:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = color[2];
        break;

    case DGL_CURRENT_COLOR_A:
        glGetFloatv(GL_CURRENT_COLOR, color);
        *v = color[3];
        break;

    case DGL_CURRENT_COLOR_RGBA:
        glGetFloatv(GL_CURRENT_COLOR, color);
        for(int i = 0; i < 4; ++i)
        {
            v[i] = color[i];
        }
        break;

    default:
        return false;
    }

    return true;
}

#undef DGL_GetFloat
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
}

#undef DGL_SetFloat
boolean DGL_SetFloat(int name, float value)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

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

#undef DGL_PushState
void DGL_PushState(void)
{
    GLState::push();
}

#undef DGL_PopState
void DGL_PopState(void)
{
    GLState::pop();

    // Make sure the restored state is immediately in effect.
    GLState::top().apply();
}

#undef DGL_Enable
int DGL_Enable(int cap)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    switch(cap)
    {
    case DGL_TEXTURE_2D:
#ifndef DRMESA
        glEnable(GL_TEXTURE_2D);
#endif
        break;

    case DGL_FOG:
        glEnable(GL_FOG);
        GL_state.currentUseFog = true;
        break;

    case DGL_SCISSOR_TEST:
        //glEnable(GL_SCISSOR_TEST);
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

#undef DGL_Disable
void DGL_Disable(int cap)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    switch(cap)
    {
    case DGL_TEXTURE_2D:
        glDisable(GL_TEXTURE_2D);
        break;

    case DGL_FOG:
        glDisable(GL_FOG);
        GL_state.currentUseFog = false;
        break;

    case DGL_SCISSOR_TEST:
        //glDisable(GL_SCISSOR_TEST);
        GLState::top().clearScissor().apply();
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

#undef DGL_BlendOp
void DGL_BlendOp(int op)
{
    GLState::top().setBlendOp(op == DGL_SUBTRACT         ? gl::Subtract :
                              op == DGL_REVERSE_SUBTRACT ? gl::ReverseSubtract :
                                                           gl::Add)
                  .apply();
}

#undef DGL_BlendFunc
void DGL_BlendFunc(int param1, int param2)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    GLState::top().setBlendFunc(param1 == DGL_ZERO                ? gl::Zero :
                                param1 == DGL_ONE                 ? gl::One  :
                                param1 == DGL_DST_COLOR           ? gl::DestColor :
                                param1 == DGL_ONE_MINUS_DST_COLOR ? gl::OneMinusDestColor :
                                param1 == DGL_SRC_ALPHA           ? gl::SrcAlpha :
                                param1 == DGL_ONE_MINUS_SRC_ALPHA ? gl::OneMinusSrcAlpha :
                                param1 == DGL_DST_ALPHA           ? gl::DestAlpha :
                                param1 == DGL_ONE_MINUS_DST_ALPHA ? gl::OneMinusDestAlpha :
                                                                    gl::Zero
                                ,
                                param2 == DGL_ZERO                ? gl::Zero :
                                param2 == DGL_ONE                 ? gl::One :
                                param2 == DGL_SRC_COLOR           ? gl::SrcColor :
                                param2 == DGL_ONE_MINUS_SRC_COLOR ? gl::OneMinusSrcColor :
                                param2 == DGL_SRC_ALPHA           ? gl::SrcAlpha :
                                param2 == DGL_ONE_MINUS_SRC_ALPHA ? gl::OneMinusSrcAlpha :
                                param2 == DGL_DST_ALPHA           ? gl::DestAlpha :
                                param2 == DGL_ONE_MINUS_DST_ALPHA ? gl::OneMinusDestAlpha :
                                                                    gl::Zero)
                  .apply();
}

#undef DGL_BlendMode
void DGL_BlendMode(blendmode_t mode)
{
    GL_BlendMode(mode);
}

#undef DGL_MatrixMode
void DGL_MatrixMode(int mode)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(mode == DGL_PROJECTION ? GL_PROJECTION :
                 mode == DGL_TEXTURE ? GL_TEXTURE :
                 GL_MODELVIEW);
}

#undef DGL_PushMatrix
void DGL_PushMatrix(void)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glPushMatrix();

#if _DEBUG
if(glGetError() == GL_STACK_OVERFLOW)
    Con_Error("DG_PushMatrix: Stack overflow.\n");
#endif
}

#undef DGL_SetNoMaterial
void DGL_SetNoMaterial(void)
{
    GL_SetNoTexture();
}

static gl::Wrapping DGL_ToGLWrapCap(DGLint cap)
{
    switch(cap)
    {
    case DGL_CLAMP:
    case DGL_CLAMP_TO_EDGE: return gl::ClampToEdge;

    case DGL_REPEAT:        return gl::Repeat;
    default:
        Con_Error("DGL_ToGLWrapCap: Unknown cap value %i.", (int)cap);
        exit(1); // Unreachable.
    }
}

#undef DGL_SetMaterialUI
void DGL_SetMaterialUI(Material *mat, DGLint wrapS, DGLint wrapT)
{
    GL_SetMaterialUI2(mat, DGL_ToGLWrapCap(wrapS), DGL_ToGLWrapCap(wrapT));
}

#undef DGL_SetPatch
void DGL_SetPatch(patchid_t id, DGLint wrapS, DGLint wrapT)
{
    try
    {
        TextureManifest &manifest = App_Textures().scheme("Patches").findByUniqueId(id);
        if(!manifest.hasTexture()) return;

        Texture &tex = manifest.texture();
        texturevariantspecification_t &texSpec =
            Rend_PatchTextureSpec(0 | (tex.isFlagged(Texture::Monochrome)        ? TSF_MONOCHROME : 0)
                                    | (tex.isFlagged(Texture::UpscaleAndSharpen) ? TSF_UPSCALE_AND_SHARPEN : 0),
                                  DGL_ToGLWrapCap(wrapS), DGL_ToGLWrapCap(wrapT));
        GL_BindTexture(tex.prepareVariant(texSpec));
    }
    catch(TextureScheme::NotFoundError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_WARNING(er.asText() + ", ignoring.");
    }
}

#undef DGL_SetPSprite
void DGL_SetPSprite(Material *mat)
{
    GL_SetPSprite(mat, 0, 0);
}

#undef DGL_SetPSprite2
void DGL_SetPSprite2(Material *mat, int tclass, int tmap)
{
    GL_SetPSprite(mat, tclass, tmap);
}

#undef DGL_SetRawImage
void DGL_SetRawImage(lumpnum_t lumpNum, DGLint wrapS, DGLint wrapT)
{
    GL_SetRawImage(lumpNum, DGL_ToGLWrapCap(wrapS), DGL_ToGLWrapCap(wrapT));
}

#undef DGL_PopMatrix
void DGL_PopMatrix(void)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glPopMatrix();

#if _DEBUG
if(glGetError() == GL_STACK_UNDERFLOW)
    Con_Error("DG_PopMatrix: Stack underflow.\n");
#endif
}

#undef DGL_LoadIdentity
void DGL_LoadIdentity(void)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glLoadIdentity();
}

#undef DGL_Translatef
void DGL_Translatef(float x, float y, float z)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glTranslatef(x, y, z);
}

#undef DGL_Rotatef
void DGL_Rotatef(float angle, float x, float y, float z)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glRotatef(angle, x, y, z);
}

#undef DGL_Scalef
void DGL_Scalef(float x, float y, float z)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glScalef(x, y, z);
}

#undef DGL_Ortho
void DGL_Ortho(float left, float top, float right, float bottom, float znear, float zfar)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glOrtho(left, right, bottom, top, znear, zfar);
}

#undef DGL_DeleteTextures
void DGL_DeleteTextures(int num, DGLuint const *names)
{
    if(!num || !names) return;

    glDeleteTextures(num, (GLuint const *) names);
}

#undef DGL_Bind
int DGL_Bind(DGLuint texture)
{
    GL_BindTextureUnmanaged(texture);
    DENG_ASSERT(!Sys_GLCheckError());
    return 0;
}

#undef DGL_NewTextureWithParams
DGLuint DGL_NewTextureWithParams(dgltexformat_t format, int width, int height,
    uint8_t const *pixels, int flags, int minFilter, int magFilter,
    int anisoFilter, int wrapS, int wrapT)
{
    return GL_NewTextureWithParams(format, width, height, (uint8_t *)pixels, flags, 0,
                                    (minFilter == DGL_LINEAR                 ? GL_LINEAR :
                                     minFilter == DGL_NEAREST                ? GL_NEAREST :
                                     minFilter == DGL_NEAREST_MIPMAP_NEAREST ? GL_NEAREST_MIPMAP_NEAREST :
                                     minFilter == DGL_LINEAR_MIPMAP_NEAREST  ? GL_LINEAR_MIPMAP_NEAREST :
                                     minFilter == DGL_NEAREST_MIPMAP_LINEAR  ? GL_NEAREST_MIPMAP_LINEAR :
                                                                               GL_LINEAR_MIPMAP_LINEAR),
                                    (magFilter == DGL_LINEAR                 ? GL_LINEAR : GL_NEAREST),
                                    anisoFilter,
                                    (wrapS == DGL_CLAMP         ? GL_CLAMP :
                                     wrapS == DGL_CLAMP_TO_EDGE ? GL_CLAMP_TO_EDGE : GL_REPEAT),
                                    (wrapT == DGL_CLAMP         ? GL_CLAMP :
                                     wrapT == DGL_CLAMP_TO_EDGE ? GL_CLAMP_TO_EDGE : GL_REPEAT));
}

// dgl_draw.cpp
DENG_EXTERN_C void DGL_Begin(dglprimtype_t mode);
DENG_EXTERN_C void DGL_End(void);
DENG_EXTERN_C boolean DGL_NewList(DGLuint list, int mode);
DENG_EXTERN_C DGLuint DGL_EndList(void);
DENG_EXTERN_C void DGL_CallList(DGLuint list);
DENG_EXTERN_C void DGL_DeleteLists(DGLuint list, int range);
DENG_EXTERN_C void DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b);
DENG_EXTERN_C void DGL_Color3ubv(const DGLubyte* vec);
DENG_EXTERN_C void DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
DENG_EXTERN_C void DGL_Color4ubv(const DGLubyte* vec);
DENG_EXTERN_C void DGL_Color3f(float r, float g, float b);
DENG_EXTERN_C void DGL_Color3fv(const float* vec);
DENG_EXTERN_C void DGL_Color4f(float r, float g, float b, float a);
DENG_EXTERN_C void DGL_Color4fv(const float* vec);
DENG_EXTERN_C void DGL_TexCoord2f(byte target, float s, float t);
DENG_EXTERN_C void DGL_TexCoord2fv(byte target, float* vec);
DENG_EXTERN_C void DGL_Vertex2f(float x, float y);
DENG_EXTERN_C void DGL_Vertex2fv(const float* vec);
DENG_EXTERN_C void DGL_Vertex3f(float x, float y, float z);
DENG_EXTERN_C void DGL_Vertex3fv(const float* vec);
DENG_EXTERN_C void DGL_Vertices2ftv(int num, const dgl_ft2vertex_t* vec);
DENG_EXTERN_C void DGL_Vertices3ftv(int num, const dgl_ft3vertex_t* vec);
DENG_EXTERN_C void DGL_Vertices3fctv(int num, const dgl_fct3vertex_t* vec);
DENG_EXTERN_C void DGL_DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a);
DENG_EXTERN_C void DGL_DrawRect(const RectRaw* rect);
DENG_EXTERN_C void DGL_DrawRect2(int x, int y, int w, int h);
DENG_EXTERN_C void DGL_DrawRectf(const RectRawf* rect);
DENG_EXTERN_C void DGL_DrawRectf2(double x, double y, double w, double h);
DENG_EXTERN_C void DGL_DrawRectf2Color(double x, double y, double w, double h, float r, float g, float b, float a);
DENG_EXTERN_C void DGL_DrawRectf2Tiled(double x, double y, double w, double h, int tw, int th);
DENG_EXTERN_C void DGL_DrawCutRectfTiled(const RectRawf* rect, int tw, int th, int txoff, int tyoff, const RectRawf* cutRect);
DENG_EXTERN_C void DGL_DrawCutRectf2Tiled(double x, double y, double w, double h, int tw, int th, int txoff, int tyoff, double cx, double cy, double cw, double ch);
DENG_EXTERN_C void DGL_DrawQuadOutline(const Point2Raw* tl, const Point2Raw* tr, const Point2Raw* br, const Point2Raw* bl, const float color[4]);
DENG_EXTERN_C void DGL_DrawQuad2Outline(int tlX, int tlY, int trX, int trY, int brX, int brY, int blX, int blY, const float color[4]);

// gl_draw.cpp
DENG_EXTERN_C void GL_UseFog(int yes);
DENG_EXTERN_C void GL_SetFilter(boolean enable);
DENG_EXTERN_C void GL_SetFilterColor(float r, float g, float b, float a);
DENG_EXTERN_C void GL_ConfigureBorderedProjection2(dgl_borderedprojectionstate_t* bp, int flags, int width, int height, int availWidth, int availHeight, scalemode_t overrideMode, float stretchEpsilon);
DENG_EXTERN_C void GL_ConfigureBorderedProjection(dgl_borderedprojectionstate_t* bp, int flags, int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);
DENG_EXTERN_C void GL_BeginBorderedProjection(dgl_borderedprojectionstate_t* bp);
DENG_EXTERN_C void GL_EndBorderedProjection(dgl_borderedprojectionstate_t* bp);

DENG_DECLARE_API(GL) =
{
    { DE_API_GL },
    DGL_Enable,
    DGL_Disable,
    DGL_PushState,
    DGL_PopState,
    DGL_GetIntegerv,
    DGL_GetInteger,
    DGL_SetInteger,
    DGL_GetFloatv,
    DGL_GetFloat,
    DGL_SetFloat,
    DGL_Ortho,
    DGL_SetScissor,
    DGL_SetScissor2,
    DGL_MatrixMode,
    DGL_PushMatrix,
    DGL_PopMatrix,
    DGL_LoadIdentity,
    DGL_Translatef,
    DGL_Rotatef,
    DGL_Scalef,
    DGL_Begin,
    DGL_End,
    DGL_NewList,
    DGL_EndList,
    DGL_CallList,
    DGL_DeleteLists,
    DGL_SetNoMaterial,
    DGL_SetMaterialUI,
    DGL_SetPatch,
    DGL_SetPSprite,
    DGL_SetPSprite2,
    DGL_SetRawImage,
    DGL_BlendOp,
    DGL_BlendFunc,
    DGL_BlendMode,
    DGL_Color3ub,
    DGL_Color3ubv,
    DGL_Color4ub,
    DGL_Color4ubv,
    DGL_Color3f,
    DGL_Color3fv,
    DGL_Color4f,
    DGL_Color4fv,
    DGL_TexCoord2f,
    DGL_TexCoord2fv,
    DGL_Vertex2f,
    DGL_Vertex2fv,
    DGL_Vertex3f,
    DGL_Vertex3fv,
    DGL_Vertices2ftv,
    DGL_Vertices3ftv,
    DGL_Vertices3fctv,
    DGL_DrawLine,
    DGL_DrawRect,
    DGL_DrawRect2,
    DGL_DrawRectf,
    DGL_DrawRectf2,
    DGL_DrawRectf2Color,
    DGL_DrawRectf2Tiled,
    DGL_DrawCutRectfTiled,
    DGL_DrawCutRectf2Tiled,
    DGL_DrawQuadOutline,
    DGL_DrawQuad2Outline,
    DGL_NewTextureWithParams,
    DGL_Bind,
    DGL_DeleteTextures,
    GL_UseFog,
    GL_SetFilter,
    GL_SetFilterColor,
    GL_ConfigureBorderedProjection2,
    GL_ConfigureBorderedProjection,
    GL_BeginBorderedProjection,
    GL_EndBorderedProjection
};
