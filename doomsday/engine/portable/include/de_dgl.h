/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * de_dgl.h: OpenGL Rasterizer for the Doomsday Engine
 */

#ifndef __DROPENGL_H__
#define __DROPENGL_H__

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

#include "dgl_atiext.h"

#define USE_MULTITEXTURE    1
#define MAX_TEX_UNITS       2      // More won't be used.

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

typedef struct dgl_state_s {
    int         maxTexSize;
    int         palExtAvailable, sharedPalExtAvailable;
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
} dgl_state_t;

typedef struct rgba_s {
    unsigned char color[4];
} rgba_t;

typedef struct dgl_state_texture_s {
    rgba_t   palette[256];
    boolean  usePalTex, dumpTextures, useCompr;
    float    grayMipmapFactor;
} dgl_state_texture_t;

typedef struct dgl_state_ext_s {
    int         multiTex;
    int         texEnvComb;
    int         nvTexEnvComb;
    int         atiTexEnvComb;
    int         aniso;
    int         genMip;
    int         blendSub;
    int         s3TC;
    int         lockArray;
#if WIN32
    int         wglSwapIntervalEXT;
    int         wglMultisampleARB;
#endif
} dgl_state_ext_t;

extern int polyCounter;

extern dgl_state_t DGL_state;
extern dgl_state_texture_t DGL_state_texture;
extern dgl_state_ext_t DGL_state_ext;

#ifdef WIN32
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

void            initState(void);
void            activeTexture(const GLenum texture);
void            InitArrays(void);
void            CheckError(void);
boolean         enablePalTexExt(boolean enable);

boolean         DGL_PreInit(void);
boolean         DGL_Init(void);
void            DGL_Shutdown(void);

void            DGL_InitExtensions(void);
boolean         DGL_QueryExtension(const char* name, const GLubyte* extensions);
#ifdef WIN32
void            DGL_InitWGLExtensions(void);
#endif
void            DGL_PrintExtensions(void);

void            DGL_Show(void);
boolean         DGL_GetIntegerv(int name, int *v);
int             DGL_GetInteger(int name);
boolean         DGL_SetInteger(int name, int value);
float           DGL_GetFloat(int name);
boolean         DGL_SetFloat(int name, float value);
void            DGL_Clear(int bufferbits);
void            DGL_Viewport(int x, int y, int width, int height);
void            DGL_Scissor(int x, int y, int width, int height);
int             DGL_Enable(int cap);
void            DGL_Disable(int cap);
void            DGL_EnableTexUnit(byte id);
void            DGL_DisableTexUnit(byte id);
void            DGL_BlendOp(int op);
void            DGL_BlendFunc(int param1, int param2);
void            DGL_Translatef(float x, float y, float z);
void            DGL_Rotatef(float angle, float x, float y, float z);
void            DGL_Scalef(float x, float y, float z);
void            DGL_Ortho(float left, float top, float right, float bottom, float znear,
                          float zfar);
void            DGL_MatrixMode(int mode);
void            DGL_PushMatrix(void);
void            DGL_PopMatrix(void);
void            DGL_LoadIdentity(void);
void            DGL_Begin(glprimtype_t type);
void            DGL_End(void);
boolean         DGL_NewList(DGLuint list, int mode);
DGLuint         DGL_EndList(void);
void            DGL_CallList(DGLuint list);
void            DGL_DeleteLists(DGLuint list, int range);
void            DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b);
void            DGL_Color3ubv(const DGLubyte *data);
void            DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
void            DGL_Color4ubv(const DGLubyte *data);
void            DGL_Color3f(float r, float g, float b);
void            DGL_Color3fv(const float *data);
void            DGL_Color4f(float r, float g, float b, float a);
void            DGL_Color4fv(const float *data);
void            DGL_TexCoord2f(float s, float t);
void            DGL_TexCoord2fv(const float *data);
void            DGL_MultiTexCoord2f(byte target, float s, float t);
void            DGL_MultiTexCoord2fv(byte target, float *data);
void            DGL_Vertex2f(float x, float y);
void            DGL_Vertex2fv(const float *data);
void            DGL_Vertex3f(float x, float y, float z);
void            DGL_Vertex3fv(const float *data);
void            DGL_Vertices2ftv(int num, const gl_ft2vertex_t *data);
void            DGL_Vertices3ftv(int num, const gl_ft3vertex_t *data);
void            DGL_Vertices3fctv(int num, const gl_fct3vertex_t *data);
void            DGL_EnableArrays(int vertices, int colors, int coords);
void            DGL_DisableArrays(int vertices, int colors, int coords);
void            DGL_Arrays(void *vertices, void *colors, int numCoords, void **coords,
                           int lock);
void            DGL_UnlockArrays(void);
void            DGL_ArrayElement(int index);
void            DGL_DrawElements(glprimtype_t type, int count, const uint *indices);
boolean         DGL_Grab(int x, int y, int width, int height, gltexformat_t format, void *buffer);
void            DGL_DeleteTextures(int num, const DGLuint *names);
DGLuint         DGL_NewTexture(void);
boolean         DGL_TexImage(gltexformat_t format, int width, int height, int genMips, void *data);
void            DGL_TexFilter(int pname, int param);
void            DGL_GetTexParameterv(int level, int pname, int *v);
int             DGL_GetTexAnisoMul(int level);
void            DGL_Palette(gltexformat_t format, void *data);
int             DGL_Bind(DGLuint texture);
#endif
