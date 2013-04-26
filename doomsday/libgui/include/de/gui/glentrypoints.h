/** @file glentrypoints.h  API entry points for OpenGL (Windows).
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBGUI_GLENTRYPOINTS_H
#define LIBGUI_GLENTRYPOINTS_H

#include "libgui.h"

#ifdef WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>

extern PFNGLACTIVETEXTUREPROC            glActiveTexture;
extern PFNGLATTACHSHADERPROC             glAttachShader;

extern PFNGLBINDATTRIBLOCATIONPROC       glBindAttribLocation;
extern PFNGLBINDBUFFERPROC               glBindBuffer;
extern PFNGLBINDFRAMEBUFFERPROC          glBindFramebuffer;
extern PFNGLBINDRENDERBUFFERPROC         glBindRenderbuffer;
extern PFNGLBLENDEQUATIONPROC            glBlendEquation;
extern PFNGLBUFFERDATAPROC               glBufferData;

extern PFNGLCHECKFRAMEBUFFERSTATUSPROC   glCheckFramebufferStatus;
extern PFNGLCOMPILESHADERPROC            glCompileShader;
extern PFNGLCREATEPROGRAMPROC            glCreateProgram;
extern PFNGLCREATESHADERPROC             glCreateShader;

extern PFNGLDELETEBUFFERSPROC            glDeleteBuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC       glDeleteFramebuffers;
extern PFNGLDELETEPROGRAMPROC            glDeleteProgram;
extern PFNGLDELETERENDERBUFFERSPROC      glDeleteRenderbuffers;
extern PFNGLDELETESHADERPROC             glDeleteShader;
extern PFNGLDETACHSHADERPROC             glDetachShader;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;

extern PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray;

extern PFNGLFRAMEBUFFERRENDERBUFFERPROC  glFramebufferRenderbuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC     glFramebufferTexture2D;

extern PFNGLGENBUFFERSPROC               glGenBuffers;
extern PFNGLGENFRAMEBUFFERSPROC          glGenFramebuffers;
extern PFNGLGENERATEMIPMAPPROC           glGenerateMipmap;
extern PFNGLGENRENDERBUFFERSPROC         glGenRenderbuffers;
extern PFNGLGETATTRIBLOCATIONPROC        glGetAttribLocation;
extern PFNGLGETPROGRAMINFOLOGPROC        glGetProgramInfoLog;
extern PFNGLGETPROGRAMIVPROC             glGetProgramiv;
extern PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog;
extern PFNGLGETSHADERIVPROC              glGetShaderiv;
extern PFNGLGETSHADERSOURCEPROC          glGetShaderSource;
extern PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation;

extern PFNGLISBUFFERPROC                 glIsBuffer;

extern PFNGLLINKPROGRAMPROC              glLinkProgram;

extern PFNGLRENDERBUFFERSTORAGEPROC      glRenderbufferStorage;

extern PFNGLSHADERSOURCEPROC             glShaderSource;

extern PFNGLUNIFORM1FPROC                glUniform1f;
extern PFNGLUNIFORM1IPROC                glUniform1i;
extern PFNGLUNIFORM2FPROC                glUniform2f;
extern PFNGLUNIFORM3FPROC                glUniform3f;
extern PFNGLUNIFORM4FPROC                glUniform4f;
extern PFNGLUNIFORMMATRIX3FVPROC         glUniformMatrix3fv;
extern PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv;
extern PFNGLUSEPROGRAMPROC               glUseProgram;

extern PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer;

void getAllOpenGLEntryPoints();

#endif // WIN32

#endif // LIBGUI_GLENTRYPOINTS_H
