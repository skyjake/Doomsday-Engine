/**\file sys_opengl.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * OpenGL interface, low-level.
 *
 * Get OpenGL header files from:
 * http://oss.sgi.com/projects/ogl-sample/
 */

#ifndef LIBDENG_SYSTEM_OPENGL_H
#define LIBDENG_SYSTEM_OPENGL_H

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <GL/gl.h>
#  include <GL/wglext.h>
#  include <GL/glext.h>
#  include <GL/glu.h>
#endif

#ifdef UNIX
#  define GL_GLEXT_PROTOTYPES
#  ifdef MACOSX
#    include <SDL.h>
#    include <SDL_opengl.h>
#  else
#    include <SDL.h>
#    include <SDL_opengl.h>
#  endif
#endif

#include <string.h>

#include "sys_window.h"

/**
 * Configure available features
 * \todo Move out of this header.
 */
#define USE_MULTITEXTURE                1
#define USE_TEXTURE_COMPRESSION_S3      1

/**
 * High-level GL state information.
 */
typedef struct gl_state_s {
    int maxTexFilterAniso;
    int maxTexSize;
#ifdef USE_MULTITEXTURE
    int maxTexUnits;
#endif
#if WIN32
    int multisampleFormat;
#endif
    boolean allowTexCompression;
    boolean forceFinishBeforeSwap;
    boolean haveCubeMap;
    boolean useArrays;
    boolean useFog;
    boolean useMultitexture;
    boolean useTexCompression;
    boolean useTexFilterAniso;
    boolean useVSync;
    float currentLineWidth, currentPointSize;
    float currentGrayMipmapFactor;
    // Extension availability bits:
    struct {
        uint atiTexEnvComb : 1;
        uint blendSub : 1;
        uint framebufferObject : 1;
        uint genMip : 1;
        uint lockArray : 1;
#ifdef USE_MULTITEXTURE
        uint multiTex : 1;
#endif
        uint nvTexEnvComb : 1;
#ifdef USE_TEXTURE_COMPRESSION_S3
        uint texCompressionS3 : 1;
#endif
        uint texEnvComb : 1;
        uint texFilterAniso : 1;
        uint texNonPow2 : 1;
#if WIN32
        uint wglMultisampleARB : 1;
        uint wglSwapIntervalEXT : 1;
#endif
    } extensions;
} gl_state_t;

typedef enum arraytype_e {
    AR_VERTEX,
    AR_COLOR,
    AR_TEXCOORD0,
    AR_TEXCOORD1,
    AR_TEXCOORD2,
    AR_TEXCOORD3,
    AR_TEXCOORD4,
    AR_TEXCOORD5,
    AR_TEXCOORD6,
    AR_TEXCOORD7
} arraytype_t;

#ifdef USE_MULTITEXTURE
#  define DGL_MAX_TEXTURE_UNITS  (GL_state.useMultitexture? GL_state.maxTexUnits : 1)
#  ifdef WIN32
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;
extern PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
extern PFNGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB;
#  endif
#else
#  define DGL_MAX_TEXTURE_UNITS  (1)
#endif

extern gl_state_t GL_state;

#ifdef WIN32
# ifdef GL_EXT_framebuffer_object
extern PFNGLGENERATEMIPMAPEXTPROC glGenerateMipmapEXT;
# endif

extern PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
extern PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;

extern PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT;
extern PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
extern PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;
#endif

#ifndef GL_ATI_texture_env_combine3
#define GL_MODULATE_ADD_ATI             0x8744
#define GL_MODULATE_SIGNED_ADD_ATI      0x8745
#define GL_MODULATE_SUBTRACT_ATI        0x8746
#endif

#ifndef GL_ATI_texture_env_combine3
#define GL_ATI_texture_env_combine3     1
#endif

boolean Sys_GLPreInit(void);

/**
 * Initializes our OpenGL interface. Called once during engine statup.
 */
boolean Sys_GLInitialize(void);

/**
 * Close our OpenGL interface for good. Called once during engine shutdown.
 */
void Sys_GLShutdown(void);

void Sys_GLConfigureDefaultState(void);

/**
 * Echo the full list of available GL extensions to the console.
 */
void Sys_GLPrintExtensions(void);

/**
 * @return  Non-zero iff the extension is found.
 */
boolean Sys_GLQueryExtension(const char* name, const GLubyte* extensions);

boolean Sys_GLCheckError(void);

#endif /* LIBDENG_SYSTEM_OPENGL_H */
