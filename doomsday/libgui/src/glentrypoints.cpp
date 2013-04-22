/** @file glentrypoints.cpp  API entry points for OpenGL (Windows).
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

#include "de/gui/glentrypoints.h"
#include <de/Log>

#ifdef WIN32

PFNGLACTIVETEXTUREPROC            glActiveTexture;
PFNGLATTACHSHADERPROC             glAttachShader;

PFNGLBINDATTRIBLOCATIONPROC       glBindAttribLocation;
PFNGLBINDBUFFERPROC               glBindBuffer;
PFNGLBUFFERDATAPROC               glBufferData;

PFNGLCOMPILESHADERPROC            glCompileShader;
PFNGLCREATEPROGRAMPROC            glCreateProgram;
PFNGLCREATESHADERPROC             glCreateShader;

PFNGLDELETEBUFFERSPROC            glDeleteBuffers;
PFNGLDELETEPROGRAMPROC            glDeleteProgram;
PFNGLDELETESHADERPROC             glDeleteShader;
PFNGLDETACHSHADERPROC             glDetachShader;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;

PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray;

PFNGLGENBUFFERSPROC               glGenBuffers;
//PFNGLGENERATEMIPMAPPROC           glGenerateMipmap;
PFNGLGETATTRIBLOCATIONPROC        glGetAttribLocation;
PFNGLGETPROGRAMINFOLOGPROC        glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC             glGetProgramiv;
PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog;
PFNGLGETSHADERIVPROC              glGetShaderiv;
PFNGLGETSHADERSOURCEPROC          glGetShaderSource;
PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation;

PFNGLISBUFFERPROC                 glIsBuffer;

PFNGLLINKPROGRAMPROC              glLinkProgram;

PFNGLSHADERSOURCEPROC             glShaderSource;

PFNGLUNIFORM1FPROC                glUniform1f;
PFNGLUNIFORM1IPROC                glUniform1i;
PFNGLUNIFORM2FPROC                glUniform2f;
PFNGLUNIFORM3FPROC                glUniform3f;
PFNGLUNIFORM4FPROC                glUniform4f;
PFNGLUNIFORMMATRIX3FVPROC         glUniformMatrix3fv;
PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv;
PFNGLUSEPROGRAMPROC               glUseProgram;

PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer;

void getAllOpenGLEntryPoints()
{
#define GET_PROC(name) { \
    void **assign = (void **) &name; \
    (*assign) = (void *) wglGetProcAddress(#name); \
    DENG2_ASSERT(name != 0); }

    LOG_AS("getAllOpenGLEntryPoints");

    LOG_VERBOSE("GL_VERSION: ") << (char const *) glGetString(GL_VERSION);

    GET_PROC(glActiveTexture);
    GET_PROC(glAttachShader);
    GET_PROC(glBindAttribLocation);
    GET_PROC(glBindBuffer);
    GET_PROC(glBufferData);
    GET_PROC(glCompileShader);
    GET_PROC(glCreateProgram);
    GET_PROC(glCreateShader);
    GET_PROC(glDeleteBuffers);
    GET_PROC(glDeleteProgram);
    GET_PROC(glDeleteShader);
    GET_PROC(glDetachShader);
    GET_PROC(glDisableVertexAttribArray);
    GET_PROC(glEnableVertexAttribArray);
    GET_PROC(glGenBuffers);
    //GET_PROC(glGenerateMipmap);
    GET_PROC(glGetAttribLocation);
    GET_PROC(glGetProgramInfoLog);
    GET_PROC(glGetProgramiv);
    GET_PROC(glGetShaderInfoLog);
    GET_PROC(glGetShaderiv);
    GET_PROC(glGetShaderSource);
    GET_PROC(glGetUniformLocation);
    GET_PROC(glIsBuffer);
    GET_PROC(glLinkProgram);
    GET_PROC(glShaderSource);
    GET_PROC(glUniform1f);
    GET_PROC(glUniform1i);
    GET_PROC(glUniform2f);
    GET_PROC(glUniform3f);
    GET_PROC(glUniform4f);
    GET_PROC(glUniformMatrix3fv);
    GET_PROC(glUniformMatrix4fv);
    GET_PROC(glUseProgram);
    GET_PROC(glVertexAttribPointer);

}

#endif // WIN32
