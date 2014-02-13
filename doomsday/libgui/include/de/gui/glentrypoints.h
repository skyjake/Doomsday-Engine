/** @file glentrypoints.h  API entry points for OpenGL (Windows/Linux).
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBGUI_GLENTRYPOINTS_H
#define LIBGUI_GLENTRYPOINTS_H

#include "libgui.h"

#if defined(WIN32) || (defined(UNIX) && !defined(MACOSX))
#  define LIBGUI_USE_GLENTRYPOINTS
#endif

#ifdef LIBGUI_USE_GLENTRYPOINTS

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  ifdef min
#    undef min
#  endif
#  ifdef max
#    undef max
#  endif
#endif

#undef GL_GLEXT_PROTOTYPES

#include <GL/gl.h>
#include <GL/glext.h>

#if defined(WIN32)
#  include <GL/wglext.h>
#  define LIBGUI_FETCH_GL_1_3
#endif

#ifdef LIBGUI_FETCH_GL_1_3
LIBGUI_EXTERN_C LIBGUI_PUBLIC PFNGLACTIVETEXTUREPROC            glActiveTexture;
LIBGUI_EXTERN_C LIBGUI_PUBLIC PFNGLBLENDEQUATIONPROC            glBlendEquation;
LIBGUI_EXTERN_C LIBGUI_PUBLIC PFNGLCLIENTACTIVETEXTUREPROC      glClientActiveTexture;
LIBGUI_EXTERN_C LIBGUI_PUBLIC PFNGLMULTITEXCOORD2FPROC          glMultiTexCoord2f;
LIBGUI_EXTERN_C LIBGUI_PUBLIC PFNGLMULTITEXCOORD2FVPROC         glMultiTexCoord2fv;
#endif

#ifdef WIN32
LIBGUI_EXTERN_C LIBGUI_PUBLIC PFNWGLGETEXTENSIONSSTRINGARBPROC  wglGetExtensionsStringARB;
#endif

LIBGUI_EXTERN_C PFNGLATTACHSHADERPROC             glAttachShader;

LIBGUI_EXTERN_C PFNGLBINDATTRIBLOCATIONPROC       glBindAttribLocation;
LIBGUI_EXTERN_C PFNGLBINDBUFFERPROC               glBindBuffer;
LIBGUI_EXTERN_C PFNGLBINDFRAMEBUFFERPROC          glBindFramebuffer;
LIBGUI_EXTERN_C PFNGLBINDRENDERBUFFERPROC         glBindRenderbuffer;
LIBGUI_EXTERN_C PFNGLBLENDFUNCSEPARATEPROC        glBlendFuncSeparate;
LIBGUI_EXTERN_C PFNGLBUFFERDATAPROC               glBufferData;

LIBGUI_EXTERN_C PFNGLCHECKFRAMEBUFFERSTATUSPROC   glCheckFramebufferStatus;
LIBGUI_EXTERN_C PFNGLCOMPILESHADERPROC            glCompileShader;
LIBGUI_EXTERN_C PFNGLCREATEPROGRAMPROC            glCreateProgram;
LIBGUI_EXTERN_C PFNGLCREATESHADERPROC             glCreateShader;

LIBGUI_EXTERN_C PFNGLDELETEBUFFERSPROC            glDeleteBuffers;
LIBGUI_EXTERN_C PFNGLDELETEFRAMEBUFFERSPROC       glDeleteFramebuffers;
LIBGUI_EXTERN_C PFNGLDELETEPROGRAMPROC            glDeleteProgram;
LIBGUI_EXTERN_C PFNGLDELETERENDERBUFFERSPROC      glDeleteRenderbuffers;
LIBGUI_EXTERN_C PFNGLDELETESHADERPROC             glDeleteShader;
LIBGUI_EXTERN_C PFNGLDETACHSHADERPROC             glDetachShader;
LIBGUI_EXTERN_C PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;

LIBGUI_EXTERN_C PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray;

LIBGUI_EXTERN_C PFNGLFRAMEBUFFERRENDERBUFFERPROC  glFramebufferRenderbuffer;
LIBGUI_EXTERN_C PFNGLFRAMEBUFFERTEXTURE2DPROC     glFramebufferTexture2D;

LIBGUI_EXTERN_C PFNGLGENBUFFERSPROC               glGenBuffers;
LIBGUI_EXTERN_C PFNGLGENFRAMEBUFFERSPROC          glGenFramebuffers;
LIBGUI_EXTERN_C PFNGLGENERATEMIPMAPPROC           glGenerateMipmap;
LIBGUI_EXTERN_C PFNGLGENRENDERBUFFERSPROC         glGenRenderbuffers;
LIBGUI_EXTERN_C PFNGLGETATTRIBLOCATIONPROC        glGetAttribLocation;
LIBGUI_EXTERN_C PFNGLGETPROGRAMINFOLOGPROC        glGetProgramInfoLog;
LIBGUI_EXTERN_C PFNGLGETPROGRAMIVPROC             glGetProgramiv;
LIBGUI_EXTERN_C PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog;
LIBGUI_EXTERN_C PFNGLGETSHADERIVPROC              glGetShaderiv;
LIBGUI_EXTERN_C PFNGLGETSHADERSOURCEPROC          glGetShaderSource;
LIBGUI_EXTERN_C PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation;

LIBGUI_EXTERN_C PFNGLISBUFFERPROC                 glIsBuffer;
LIBGUI_EXTERN_C PFNGLISFRAMEBUFFERPROC            glIsFramebuffer;
LIBGUI_EXTERN_C PFNGLISPROGRAMPROC                glIsProgram;

LIBGUI_EXTERN_C PFNGLLINKPROGRAMPROC              glLinkProgram;

LIBGUI_EXTERN_C PFNGLRENDERBUFFERSTORAGEPROC      glRenderbufferStorage;

LIBGUI_EXTERN_C PFNGLSHADERSOURCEPROC             glShaderSource;

LIBGUI_EXTERN_C PFNGLUNIFORM1FPROC                glUniform1f;
LIBGUI_EXTERN_C PFNGLUNIFORM1IPROC                glUniform1i;
LIBGUI_EXTERN_C PFNGLUNIFORM2FPROC                glUniform2f;
LIBGUI_EXTERN_C PFNGLUNIFORM3FPROC                glUniform3f;
LIBGUI_EXTERN_C PFNGLUNIFORM4FPROC                glUniform4f;
LIBGUI_EXTERN_C PFNGLUNIFORMMATRIX3FVPROC         glUniformMatrix3fv;
LIBGUI_EXTERN_C PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv;
LIBGUI_EXTERN_C PFNGLUSEPROGRAMPROC               glUseProgram;

LIBGUI_EXTERN_C PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer;

// Extensions:

#ifdef GL_ARB_debug_output
LIBGUI_EXTERN_C PFNGLDEBUGMESSAGECONTROLARBPROC   glDebugMessageControlARB;
LIBGUI_EXTERN_C PFNGLDEBUGMESSAGECALLBACKARBPROC  glDebugMessageCallbackARB;
#endif
LIBGUI_EXTERN_C PFNGLBLITFRAMEBUFFEREXTPROC                         glBlitFramebufferEXT;
LIBGUI_EXTERN_C PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC          glRenderbufferStorageMultisampleEXT;
#ifdef GL_NV_framebuffer_multisample_coverage
LIBGUI_EXTERN_C PFNGLRENDERBUFFERSTORAGEMULTISAMPLECOVERAGENVPROC   glRenderbufferStorageMultisampleCoverageNV;
#endif

void getAllOpenGLEntryPoints();

#ifdef Q_WS_X11
LIBGUI_PUBLIC char const *getGLXExtensionsString();
LIBGUI_PUBLIC void setXSwapInterval(int interval);
void getGLXEntryPoints();
#endif

#endif // LIBGUI_USE_GLENTRYPOINTS

#endif // LIBGUI_GLENTRYPOINTS_H
