/** @file glentrypoints.cpp  API entry points for OpenGL (Windows/Linux).
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

#include "de/graphics/glentrypoints.h"
#include <de/GLInfo>

#ifdef LIBGUI_USE_GLENTRYPOINTS

#ifdef DENG_X11
#  include <GL/glx.h>
#endif

#ifdef LIBGUI_FETCH_GL_1_3
PFNGLACTIVETEXTUREPROC            glActiveTexture;
PFNGLBLENDEQUATIONPROC            glBlendEquation;
PFNGLCLIENTACTIVETEXTUREPROC      glClientActiveTexture;
PFNGLMULTITEXCOORD2FPROC          glMultiTexCoord2f;
PFNGLMULTITEXCOORD2FVPROC         glMultiTexCoord2fv;
#endif

#ifdef WIN32
PFNWGLGETEXTENSIONSSTRINGARBPROC  wglGetExtensionsStringARB;
#endif

PFNGLATTACHSHADERPROC             glAttachShader;

PFNGLBINDATTRIBLOCATIONPROC       glBindAttribLocation;
PFNGLBINDBUFFERPROC               glBindBuffer;
PFNGLBINDFRAMEBUFFERPROC          glBindFramebuffer;
PFNGLBINDRENDERBUFFERPROC         glBindRenderbuffer;
PFNGLBLENDFUNCSEPARATEPROC        glBlendFuncSeparate;
PFNGLBUFFERDATAPROC               glBufferData;

PFNGLCHECKFRAMEBUFFERSTATUSPROC   glCheckFramebufferStatus;
PFNGLCOMPILESHADERPROC            glCompileShader;
PFNGLCREATEPROGRAMPROC            glCreateProgram;
PFNGLCREATESHADERPROC             glCreateShader;

PFNGLDELETEBUFFERSPROC            glDeleteBuffers;
PFNGLDELETEFRAMEBUFFERSPROC       glDeleteFramebuffers;
PFNGLDELETEPROGRAMPROC            glDeleteProgram;
PFNGLDELETERENDERBUFFERSPROC      glDeleteRenderbuffers;
PFNGLDELETESHADERPROC             glDeleteShader;
PFNGLDETACHSHADERPROC             glDetachShader;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;

PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray;

PFNGLFRAMEBUFFERRENDERBUFFERPROC  glFramebufferRenderbuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC     glFramebufferTexture2D;

PFNGLGENBUFFERSPROC               glGenBuffers;
PFNGLGENFRAMEBUFFERSPROC          glGenFramebuffers;
PFNGLGENERATEMIPMAPPROC           glGenerateMipmap;
PFNGLGENRENDERBUFFERSPROC         glGenRenderbuffers;
PFNGLGETATTRIBLOCATIONPROC        glGetAttribLocation;
PFNGLGETPROGRAMINFOLOGPROC        glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC             glGetProgramiv;
PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog;
PFNGLGETSHADERIVPROC              glGetShaderiv;
PFNGLGETSHADERSOURCEPROC          glGetShaderSource;
PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation;

PFNGLISBUFFERPROC                 glIsBuffer;
PFNGLISFRAMEBUFFERPROC            glIsFramebuffer;
PFNGLISPROGRAMPROC                glIsProgram;

PFNGLLINKPROGRAMPROC              glLinkProgram;

PFNGLRENDERBUFFERSTORAGEPROC      glRenderbufferStorage;

PFNGLSHADERSOURCEPROC             glShaderSource;

PFNGLUNIFORM1FPROC                glUniform1f;
PFNGLUNIFORM1IPROC                glUniform1i;
PFNGLUNIFORM2FPROC                glUniform2f;
PFNGLUNIFORM3FPROC                glUniform3f;
PFNGLUNIFORM3FVPROC               glUniform3fv;
PFNGLUNIFORM4FPROC                glUniform4f;
PFNGLUNIFORM4FVPROC               glUniform4fv;
PFNGLUNIFORMMATRIX3FVPROC         glUniformMatrix3fv;
PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv;
PFNGLUSEPROGRAMPROC               glUseProgram;

PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer;

// Extensions:

PFNGLBLITFRAMEBUFFEREXTPROC                         glBlitFramebufferEXT;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC          glRenderbufferStorageMultisampleEXT;

#ifdef GL_ARB_draw_instanced
PFNGLDRAWARRAYSINSTANCEDARBPROC                     glDrawArraysInstancedARB;
PFNGLDRAWELEMENTSINSTANCEDARBPROC                   glDrawElementsInstancedARB;
#endif

#ifdef GL_ARB_instanced_arrays
PFNGLVERTEXATTRIBDIVISORARBPROC                     glVertexAttribDivisorARB;
#endif

#ifdef GL_NV_framebuffer_multisample_coverage
PFNGLRENDERBUFFERSTORAGEMULTISAMPLECOVERAGENVPROC   glRenderbufferStorageMultisampleCoverageNV;
#endif

void getAllOpenGLEntryPoints()
{
    static bool haveProcs = false;
    if(haveProcs) return;

#ifdef WIN32
#  ifdef MSVC
#    define GET_PROC_EXT(name) *((void**)&name) = wglGetProcAddress(#name)
#  else
#    define GET_PROC_EXT(name) *reinterpret_cast<PROC *>(&name) = wglGetProcAddress(#name)
#  endif
#else
#  define GET_PROC_EXT(name) *((void (**)())&name) = glXGetProcAddress((GLubyte const *)#name)
#endif

#define GET_PROC(name) GET_PROC_EXT(name); DENG2_ASSERT(name != 0) // must have

#ifdef LIBGUI_FETCH_GL_1_3
    GET_PROC(glActiveTexture);
    GET_PROC(glBlendEquation);
    GET_PROC(glClientActiveTexture);
    GET_PROC(glMultiTexCoord2f);
    GET_PROC(glMultiTexCoord2fv);
#endif

#ifdef WIN32
    GET_PROC(wglGetExtensionsStringARB);
#endif

    GET_PROC(glAttachShader);

    GET_PROC(glBindAttribLocation);
    GET_PROC(glBindBuffer);
    GET_PROC(glBindFramebuffer);
    GET_PROC(glBindRenderbuffer);
    GET_PROC(glBlendFuncSeparate);
    GET_PROC(glBufferData);

    GET_PROC(glCheckFramebufferStatus);
    GET_PROC(glCompileShader);
    GET_PROC(glCreateProgram);
    GET_PROC(glCreateShader);

    GET_PROC(glDeleteBuffers);
    GET_PROC(glDeleteFramebuffers);
    GET_PROC(glDeleteProgram);
    GET_PROC(glDeleteRenderbuffers);
    GET_PROC(glDeleteShader);
    GET_PROC(glDetachShader);
    GET_PROC(glDisableVertexAttribArray);

    GET_PROC(glEnableVertexAttribArray);

    GET_PROC(glFramebufferRenderbuffer);
    GET_PROC(glFramebufferTexture2D);

    GET_PROC(glGenBuffers);
    GET_PROC(glGenFramebuffers);
    GET_PROC(glGenerateMipmap);
    GET_PROC(glGenRenderbuffers);
    GET_PROC(glGetAttribLocation);
    GET_PROC(glGetProgramInfoLog);
    GET_PROC(glGetProgramiv);
    GET_PROC(glGetShaderInfoLog);
    GET_PROC(glGetShaderiv);
    GET_PROC(glGetShaderSource);
    GET_PROC(glGetUniformLocation);

    GET_PROC(glIsBuffer);
    GET_PROC(glIsFramebuffer);
    GET_PROC(glIsProgram);

    GET_PROC(glLinkProgram);

    GET_PROC(glRenderbufferStorage);

    GET_PROC(glShaderSource);

    GET_PROC(glUniform1f);
    GET_PROC(glUniform1i);
    GET_PROC(glUniform2f);
    GET_PROC(glUniform3f);
    GET_PROC(glUniform3fv);
    GET_PROC(glUniform4f);
    GET_PROC(glUniform4fv);
    GET_PROC(glUniformMatrix3fv);
    GET_PROC(glUniformMatrix4fv);
    GET_PROC(glUseProgram);

    GET_PROC(glVertexAttribPointer);

    // Extensions:

    GET_PROC_EXT(glBlitFramebufferEXT);
    GET_PROC_EXT(glRenderbufferStorageMultisampleEXT);

#ifdef GL_ARB_draw_instanced
    GET_PROC_EXT(glDrawArraysInstancedARB);
    GET_PROC_EXT(glDrawElementsInstancedARB);
#endif

#ifdef GL_ARB_instanced_arrays
    GET_PROC_EXT(glVertexAttribDivisorARB);
#endif

#ifdef GL_NV_framebuffer_multisample_coverage
    GET_PROC_EXT(glRenderbufferStorageMultisampleCoverageNV);
#endif

#ifdef DENG_X11
    getGLXEntryPoints();
#endif

    haveProcs = true;
}

#endif // LIBGUI_USE_GLENTRYPOINTS
