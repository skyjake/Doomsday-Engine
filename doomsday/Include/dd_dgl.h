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
 * dd_dgl.h: Accessing the Doomsday Graphics Library API
 */

#ifndef __DOOMSDAY_DGL_BASE_H__
#define __DOOMSDAY_DGL_BASE_H__

#include "dglib.h"

/*
 * This driver struct is filled with addresses exported from the rendering
 * DLL right after it has been loaded, in dd_dgl.c.
 */
typedef struct dgldriver_s 
{
	int		(*Init)(int width, int height, int bpp, int mode);
	void	(*Shutdown)(void);
	
	// Viewport.
	void	(*Clear)(int bufferbits);
	void	(*Show)(void);
	void	(*Viewport)(int x, int y, int width, int height);
	void	(*Scissor)(int x, int y, int width, int height);

	// State.
	int		(*GetInteger)(int name);
	int		(*GetIntegerv)(int name, int *values);
	int		(*SetInteger)(int name, int value);
	int		(*SetFloatv)(int name, float *values);
	char*	(*GetString)(int name);
	int		(*Enable)(int cap);
	void	(*Disable)(int cap);
	void	(*EnableArrays)(int, int, int);
	void	(*DisableArrays)(int, int, int);
	void	(*Arrays)(void*, void*, int, void**, int);
	void	(*UnlockArrays)(void);
	void	(*Func)(int func, int param1, int param2);
	void	(*ZBias)(int level);

	// Textures.
	DGLuint	(*NewTexture)(void);
	void	(*DeleteTextures)(int num, DGLuint *names);
	int		(*TexImage)(int format, int width, int height, int mipmap, void *data);
	void	(*TexParameter)(int pname, int param);
	void	(*GetTexParameterv)(int level, int pname, int *v);
	void	(*Palette)(int format, void *data);	
	int		(*Bind)(DGLuint texture);

	// Matrix operations.
	void	(*MatrixMode)(int mode);
	void	(*PushMatrix)(void);
	void	(*PopMatrix)(void);
	void	(*LoadIdentity)(void);
	void	(*Translatef)(float x, float y, float z);
	void	(*Rotatef)(float angle, float x, float y, float z);
	void	(*Scalef)(float x, float y, float z);
	void	(*Ortho)(float left, float top, float right, float bottom, float znear, float zfar);
	void	(*Perspective)(float fovy, float aspect, float zNear, float zFar);

	// Colors.
	void	(*Color3ub)(DGLubyte r, DGLubyte g, DGLubyte b);
	void	(*Color3ubv)(void *data);
	void	(*Color4ub)(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
	void	(*Color4ubv)(void *data);
	void	(*Color3f)(float r, float g, float b);
	void	(*Color3fv)(float *data);
	void	(*Color4f)(float r, float g, float b, float a);
	void	(*Color4fv)(float *data);

	// Drawing.
	void	(*Begin)(int mode);
	void	(*End)(void);
	void	(*Vertex2f)(float x, float y);
	void	(*Vertex2fv)(float *data);
	void	(*Vertex3f)(float x, float y, float z);
	void	(*Vertex3fv)(float *data);
	void	(*TexCoord2f)(float s, float t);
	void	(*TexCoord2fv)(float *data);
	void	(*MultiTexCoord2f)(int target, float s, float t);
	void	(*MultiTexCoord2fv)(int target, float *data);
	void	(*Vertices2ftv)(int num, gl_ft2vertex_t *data);
	void	(*Vertices3ftv)(int num, gl_ft3vertex_t *data);
	void	(*Vertices3fctv)(int num, gl_fct3vertex_t *data);
	void	(*ArrayElement)(int index);
	void	(*DrawElements)(int type, int count, unsigned int *indices);

	// Miscellaneous.
	int		(*Grab)(int x, int y, int width, int height, int format, void *buffer);
	void	(*Fog)(int pname, float param);
	void	(*Fogv)(int pname, void *data);
	int		(*Project)(int num, gl_fc3vertex_t *inVertices, gl_fc3vertex_t *outVertices);
	int		(*ReadPixels)(int *inData, int format, void *pixels); 
} 
dgldriver_t;

extern dgldriver_t __gl;

#define gl __gl

int		DD_InitDGL(void);
void	DD_ShutdownDGL(void);
void *	DD_GetDGLProcAddress(const char *name);

#endif 
