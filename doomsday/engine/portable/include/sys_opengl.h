/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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

#  define wglGetProcAddress SDL_GL_GetProcAddress
#endif

#include <string.h>

#include "sys_window.h"

#define USE_MULTITEXTURE    1
#define MAX_TEX_UNITS       2      // More won't be used.

// A helpful macro that changes the origin of the screen
// coordinate system.
#define FLIP(y) (theWindow->height - (y+1))

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

typedef struct gl_state_s {
    int         maxTexSize;
    int         palExtAvailable;
    boolean     allowCompression;
    boolean     noArrays;
    boolean     forceFinishBeforeSwap;
    int         useAnisotropic;
    boolean     useVSync;
    int         maxAniso;
    int         maxTexUnits;
    boolean     useFog;
    float       nearClip, farClip;
    float       currentLineWidth, currentPointSize;
    int         textureNonPow2;
#if WIN32
    int         multisampleFormat;
#endif
} gl_state_t;

typedef struct rgba_s {
    unsigned char color[4];
} rgba_t;

typedef struct gl_state_texture_s {
    boolean  usePalTex, dumpTextures, useCompr, haveCubeMap;
    float    grayMipmapFactor;
} gl_state_texture_t;

typedef struct gl_state_ext_s {
    int         multiTex;
    int         texEnvComb;
    int         nvTexEnvComb;
    int         atiTexEnvComb;
    int         aniso;
    int         genMip;
    int         blendSub;
    int         s3TC;
    int         lockArray;
    int         framebufferObject;
    int         texEnvLODBias;
#if WIN32
    int         wglSwapIntervalEXT;
    int         wglMultisampleARB;
#endif
} gl_state_ext_t;

extern int polyCounter;

extern gl_state_t GL_state;
extern gl_state_texture_t GL_state_texture;
extern gl_state_ext_t GL_state_ext;

#ifdef WIN32
# ifdef GL_EXT_framebuffer_object
extern PFNGLGENERATEMIPMAPEXTPROC glGenerateMipmapEXT;
# endif

extern PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
extern PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;

extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;
extern PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
extern PFNGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB;
extern PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT;
extern PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
extern PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;
extern PFNGLCOLORTABLEEXTPROC glColorTableEXT;
extern PFNGLCOLORTABLEEXTPROC glColorTableEXT;
#endif

#ifndef GL_ATI_texture_env_combine3
#define GL_MODULATE_ADD_ATI             0x8744
#define GL_MODULATE_SIGNED_ADD_ATI      0x8745
#define GL_MODULATE_SUBTRACT_ATI        0x8746
#endif

#ifndef GL_ATI_texture_env_combine3
#define GL_ATI_texture_env_combine3     1
#endif

boolean         Sys_PreInitGL(void);
boolean         Sys_InitGL(void);
void            Sys_ShutdownGL(void);
void            Sys_InitGLState(void);
void            Sys_InitGLExtensions(void);
#ifdef WIN32
void            Sys_InitWGLExtensions(void);
#endif
void            Sys_PrintGLExtensions(void);
boolean         Sys_QueryGLExtension(const char* name, const GLubyte* extensions);
void            Sys_CheckGLError(void);
#endif /* LIBDENG_SYSTEM_OPENGL_H */
