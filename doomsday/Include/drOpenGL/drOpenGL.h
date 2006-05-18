/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * drOpenGL.h: OpenGL Rasterizer for the Doomsday Engine
 */

#ifndef __DROPENGL_H__
#define __DROPENGL_H__

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <GL/gl.h>
#  include <GL/glext.h>
#  include <GL/glu.h>
#endif

#ifdef UNIX
#  define GL_GLEXT_PROTOTYPES
#  include <SDL/SDL.h>
#  include <SDL/SDL_opengl.h>
#  include "atiext.h"
#  define wglGetProcAddress SDL_GL_GetProcAddress
#endif

#include <string.h>

#include "../doomsday.h"
#include "../dglib.h"

#define USE_MULTITEXTURE    1
#define MAX_TEX_UNITS       2      // More won't be used.

#define DROGL_VERSION       230
#define DROGL_VERSION_TEXT  "2.3."DOOMSDAY_RELEASE_NAME
#define DROGL_VERSION_FULL  "DGL OpenGL Driver Version "DROGL_VERSION_TEXT" ("__DATE__")"

enum { VX, VY, VZ };
enum { CR, CG, CB, CA };

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

typedef struct rgba_s {
    unsigned char   color[4];
} rgba_t;

void            initState(void);

//-------------------------------------------------------------------------
// main.c
//
extern int      screenWidth, screenHeight;
extern int      useFog, maxTexSize;
extern int      palExtAvailable, sharedPalExtAvailable;
extern boolean  texCoordPtrEnabled;
extern boolean  allowCompression;
extern boolean  noArrays;
extern int      verbose;
extern int      useAnisotropic;
extern int      useVSync;
extern float    maxAniso;
extern int      maxTexUnits;
extern boolean  wireframeMode;

void            DG_Clear(int bufferbits);
void            activeTexture(const GLenum texture);

//-------------------------------------------------------------------------
// draw.c
//
extern int      polyCounter;

void            InitArrays(void);
void            CheckError(void);
void            DG_Begin(int mode);
void            DG_End(void);
void            DG_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b);
void            DG_Color3ubv(DGLubyte * data);
void            DG_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
void            DG_Color4ubv(DGLubyte * data);
void            DG_Color3f(float r, float g, float b);
void            DG_Color3fv(float *data);
void            DG_Color4f(float r, float g, float b, float a);
void            DG_Color4fv(float *data);
void            DG_Vertex2f(float x, float y);
void            DG_Vertex2fv(float *data);
void            DG_Vertex3f(float x, float y, float z);
void            DG_Vertex3fv(float *data);
void            DG_TexCoord2f(float s, float t);
void            DG_TexCoord2fv(float *data);
void            DG_Vertices2ftv(int num, gl_ft2vertex_t * data);
void            DG_Vertices3ftv(int num, gl_ft3vertex_t * data);
void            DG_Vertices3fctv(int num, gl_fct3vertex_t * data);
void            DG_DisableArrays(int vertices, int colors, int coords);

//-------------------------------------------------------------------------
// texture.c
//
extern rgba_t   palette[256];
extern int      usePalTex, dumpTextures, useCompr;
extern float    grayMipmapFactor;

int             Power2(int num);
int             enablePalTexExt(int enable);
DGLuint         DG_NewTexture(void);
int             DG_TexImage(int format, int width, int height, int mipmap,
                            void *data);
void            DG_DeleteTextures(int num, DGLuint * names);
void            DG_TexParameter(int pname, int param);
void            DG_GetTexParameterv(int level, int pname, int *v);
void            DG_Palette(int format, void *data);
int             DG_Bind(DGLuint texture);

//-------------------------------------------------------------------------
// ext.c
//
#ifdef WIN32
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;
extern PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
extern PFNGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB;

extern PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT;
extern PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
extern PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;
extern PFNGLCOLORTABLEEXTPROC glColorTableEXT;
#endif

extern int      extMultiTex;
extern int      extTexEnvComb;
extern int      extNvTexEnvComb;
extern int      extAtiTexEnvComb;
extern int      extAniso;
extern int      extVSync;
extern int      extGenMip;
extern int      extBlendSub;
extern int      extS3TC;

void            initExtensions(void);

#endif
