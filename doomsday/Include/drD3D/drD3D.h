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
 * drD3D.h: DGL Direct3D 8.1 Driver
 */

#ifndef __DRD3D_H__
#define __DRD3D_H__

#define DIRECT3D_VERSION	0x0800
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <d3dx8.h>

// Warning about a function having no return value.
#pragma warning (disable: 4035)

#include "../doomsday.h"
#include "../dglib.h"

#define DRD3D_VERSION		220
#define DRD3D_VERSION_TEXT	"2.2."DOOMSDAY_RELEASE_NAME
#define DRD3D_VERSION_FULL	"DGL Direct3D8 Driver Version "DRD3D_VERSION_TEXT" ("__DATE__")"
#define MAX_TEX_UNITS		2
#define MAX_TEX_STAGES		8

#define	CLAMP01(f)			{ if(f < 0) f = 0; if(f > 1) f = 1; }
#define SetRS(x, y)			dev->SetRenderState(x, y)
#ifdef _DEBUG
# define SetTSS(s, x, y)	if(FAILED(dev->SetTextureStageState(s, x, y))) Con_Error("D3D: SetTSS %i (0x%X, 0x%X) failed!\n", s, x, y);
#else
# define SetTSS(s, x, y)	dev->SetTextureStageState(s, x, y)
#endif

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

//------------------------------------------------------------------------
// Types and other useful stuff
//
enum { VX, VY, VZ };
enum { CR, CG, CB, CA };

#define PI 3.14159265

#define DRVTX_FORMAT (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEX2)

typedef struct drvertex_s {
	D3DVECTOR	pos;
	D3DCOLOR	color;
	float		tex[2];
	float		tex2[2];
} drvertex_t;

#define DRVSIZE ((int)sizeof(drvertex_t))

#include "window.h"
#include "box.h"

// We will export things in "C" mode.
extern "C" {

//===========================================================================
// main.cpp
//===========================================================================
extern HINSTANCE	hinst;	// Instance handle to the DLL.
extern Window		*window;
extern boolean		verbose, diagnose;
extern int			maxTexSize, maxTextures, maxStages, maxAniso;
extern boolean		useBadAlpha;
extern boolean		availPalTex, availMulAdd;

void	DP(const char *format, ...);
int		DG_Init(int width, int height, int bpp, int mode);
void	DG_Shutdown(void);
void	DG_Clear(int bufferbits);
void	DG_Show(void);
int		DG_Grab(int x, int y, int width, int height, int format, void *buffer);
int		DG_ReadPixels(int *inData, int format, void *pixels);

//===========================================================================
// config.cpp
//===========================================================================
extern int			wantedAdapter;
extern int			wantedColorDepth;
extern int			wantedTexDepth;
extern int			wantedZDepth;

void	ReadConfig(void);

//===========================================================================
// d3dinit.cpp
//===========================================================================
extern IDirect3D8	*d3d;
extern IDirect3DDevice8	*dev;
extern HRESULT		hr;	
extern D3DCAPS8		caps;

int		InitDirect3D(void);
void	ShutdownDirect3D(void);
void	DXError(const char *funcname);

//===========================================================================
// state.cpp
//===========================================================================
/*extern int		usefog;
extern int		texturesEnabled;
*/
void	InitState(void);
int		DG_GetInteger(int name);
int		DG_GetIntegerv(int name, int *v);
int		DG_SetInteger(int name, int value);
char*	DG_GetString(int name);
int		DG_Enable(int cap);
void	DG_Disable(int cap);
void	DG_Func(int func, int param1, int param2);
void	DG_Fog(int pname, float param);
void	DG_Fogv(int pname, void *data);

/*
//===========================================================================
// vertex.cpp
//===========================================================================
extern int			triCounter;
extern boolean		skipDraw;

void	InitVertexBuffers(void);
void	ShutdownVertexBuffers(void);
void	BufferVertices(drvertex_t *verts, int num);
void	BufferIndices(unsigned short *indices, int num);
void	DrawBuffers(D3DPRIMITIVETYPE primType);
void	EmptyBuffers(void);
*/

//===========================================================================
// draw.cpp
//===========================================================================
extern drvertex_t	currentVertex;	

void	InitDraw();
void	DG_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b);
void	DG_Color3ubv(void *data);
void	DG_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
void	DG_Color4ubv(void *data);
void	DG_Color3f(float r, float g, float b);
void	DG_Color3fv(float *data);
void	DG_Color4f(float r, float g, float b, float a);
void	DG_Color4fv(float *data);
void	DG_Begin(int mode);
void	DG_End(void);
void	DG_Vertex2f(float x, float y);
void	DG_Vertex2fv(float *data);
void	DG_Vertex3f(float x, float y, float z);
void	DG_Vertex3fv(float *data);
void	DG_TexCoord2f(float s, float t);
void	DG_TexCoord2fv(float *data);
void	DG_Vertices2ftv(int num, gl_ft2vertex_t *data);
void	DG_Vertices3ftv(int num, gl_ft3vertex_t *data);
void	DG_Vertices3fctv(int num, gl_fct3vertex_t *data);
void	DG_DisableArrays(int vertices, int colors, int coords);

//===========================================================================
// matrix.cpp
//===========================================================================
void	InitMatrices(void);
void	ShutdownMatrices(void);
void	TransformTexCoord(float st[2]);
void	ScissorProjection(void);
void	DG_MatrixMode(int mode);
void	DG_PushMatrix(void);
void	DG_PopMatrix(void);
void	DG_LoadIdentity(void);
void	DG_Translatef(float x, float y, float z);
void	DG_PostTranslatef(float x, float y, float z);
void	DG_Rotatef(float angle, float x, float y, float z);
void	DG_Scalef(float x, float y, float z);
void	DG_Ortho(float left, float top, float right, float bottom, float znear, float zfar);
void	DG_Perspective(float fovy, float aspect, float zNear, float zFar);
int		DG_Project(int num, gl_fc3vertex_t *inVertices, gl_fc3vertex_t *outVertices);

//===========================================================================
// texture.cpp
//===========================================================================
extern float grayMipmapFactor;

void	InitTextures(void);
void	ShutdownTextures(void);
void	StageIdentity(void);
void	SetUnitStage(int logicalUnit, int actualStage);
void	ActiveTexture(int index);
void	TextureOperatingMode(int isActive);
byte *	GetPaletteColor(int index);
DGLuint	DG_NewTexture(void);
int		DG_TexImage(int format, int width, int height, int mipmap, void *data);
void	DG_DeleteTextures(int num, DGLuint *names);
void	DG_TexParameter(int pname, int param);
void	DG_GetTexParameterv(int level, int pname, int *v);
void	DG_Palette(int format, void *data);
int		DG_Bind(DGLuint texture);

//===========================================================================
// viewport.cpp
//===========================================================================
extern boolean		scissorActive;
extern Box			scissor, viewport;

void	InitViewport(void);
void	EnableScissor(bool enable);
void	DG_Viewport(int x, int y, int width, int height);
void	DG_Scissor(int x, int y, int width, int height);
void	DG_ZBias(int level);

}

#endif
