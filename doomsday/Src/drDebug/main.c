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
 * main.c: Debugging Layer for the Doomsday Rendering DLLs
 *
 * The real rendering DLL can be specified using the -dgl option.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#include "doomsday.h"
#include "dd_dgl.h"

/* 
 * This macro is used to import all the exported functions from the 
 * rendering DLL.
 */
#define Imp(fname)	(gl.fname = (void*) GetProcAddress(dglInstance, "DG_"#fname))

HINSTANCE dglInstance;
dgldriver_t gl;
int level;	// Debug message level.

/*
 * Entry point of the DLL.
 */
BOOL DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	const char *dglFileName = "drOpenGL.dll";

	switch(fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		freopen("drDebug.log", "w", stdout);

		// Some other renderer?
		if(ArgExists("-dgl")) dglFileName = ArgNext();	

		// Load the real rendering DLL.
		if((dglInstance = LoadLibrary(dglFileName)) == NULL)
			Con_Error("drDebug: Failed to load %s.\n", dglFileName);

		// What is the level of debug messages?
		if(ArgExists("-d0"))
			level = 0;
		else if(ArgExists("-d1"))
			level = 1;
		else if(ArgExists("-d3"))
			level = 3;
		else
			level = 2; // This is the default level.

		Imp(Init);
		Imp(Shutdown);

		// Viewport.
		Imp(Clear);
		Imp(Show);
		Imp(Viewport);
		Imp(Scissor);

		// State.
		Imp(GetInteger);
		Imp(GetIntegerv);
		Imp(SetInteger);
		Imp(SetFloatv);
		Imp(GetString);
		Imp(Enable);
		Imp(Disable);
		Imp(EnableArrays);
		Imp(DisableArrays);
		Imp(Arrays);
		Imp(UnlockArrays);
		Imp(Func);
		//Imp(ZBias);	

		// Textures.
		Imp(NewTexture);
		Imp(DeleteTextures);
		Imp(TexImage);
		Imp(TexParameter);
		Imp(GetTexParameterv);
		Imp(Palette);	
		Imp(Bind);

		// Matrix operations.
		Imp(MatrixMode);
		Imp(PushMatrix);
		Imp(PopMatrix);
		Imp(LoadIdentity);
		Imp(Translatef);
		Imp(Rotatef);
		Imp(Scalef);
		Imp(Ortho);
		Imp(Perspective);

		// Colors.
		Imp(Color3ub);
		Imp(Color3ubv);
		Imp(Color4ub);
		Imp(Color4ubv);
		Imp(Color3f);
		Imp(Color3fv);
		Imp(Color4f);
		Imp(Color4fv);

		// Drawing.
		Imp(Begin);
		Imp(End);
		Imp(Vertex2f);
		Imp(Vertex2fv);
		Imp(Vertex3f);
		Imp(Vertex3fv);
		Imp(TexCoord2f);
		Imp(TexCoord2fv);
		Imp(MultiTexCoord2f);
		Imp(MultiTexCoord2fv);
		Imp(Vertices2ftv);
		Imp(Vertices3ftv);
		Imp(Vertices3fctv);
		Imp(Arrays);
		Imp(UnlockArrays);
		Imp(ArrayElement);
		Imp(DrawElements);

		// Miscellaneous.
		Imp(Grab);
		Imp(Fog);
		Imp(Fogv);
		Imp(Project);
		Imp(ReadPixels); 

		break;

	case DLL_PROCESS_DETACH:
		FreeLibrary(dglInstance);
		break;
	}
	return TRUE;
}

/*
 * The debug message printers.
 */
void printer(boolean in, int messageLevel, const char *format, va_list args)
{
	if(messageLevel > level) return;

	printf(in? "-> " : "<- ");
	vprintf(format, args);
	printf("\n");
	fflush(stdout);
}

void in(int messageLevel, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	printer(true, messageLevel, format, args);
	va_end(args);
}

void out(int messageLevel, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	printer(false, messageLevel, format, args);
	va_end(args);
}

/*
 * The debugging layer follows. Input parameters and return values are printed
 * to stdout according to the debug message level.
 */

#define SIMPLE(name)	in(1, #name); gl.name(); out(1, #name);
#define INT1_VOID(name, a) \
	in(1, #name" (%i)", a); \
	gl.name(a); \
	out(1, #name); 
#define INT1_INT(name, a) \
	int result;	\
	in(1, #name" (%i)", a); \
	result = gl.name(a); \
	out(1, #name": %i", result); \
	return result;
#define INT4_VOID(name, a, b, c, d) \
	in(1, #name" (%i, %i, %i, %i)", a, b, c, d); \
	gl.name(a, b, c, d); \
	out(1, #name); 
#define INT4_INT(name, a, b, c, d) \
	int result;	\
	in(1, #name" (%i, %i, %i, %i)", a, b, c, d); \
	result = gl.name(a, b, c, d); \
	out(1, #name": %i", result); \
	return result;

int DG_Init(int width, int height, int bpp, int mode)
{
	INT4_INT( Init, width, height, bpp, mode );
}

void DG_Shutdown(void)
{
	SIMPLE( Shutdown );
}

void DG_Clear(int bufferbits)
{
	in(1, "Clear (%x)", bufferbits);
	gl.Clear(bufferbits);
	out(1, "Clear");
}

void DG_Show(void)
{
	SIMPLE( Show );
}

void DG_Viewport(int x, int y, int width, int height)
{
	INT4_VOID( Viewport, x, y, width, height );
}

void DG_Scissor(int x, int y, int width, int height)
{
	INT4_VOID( Scissor, x, y, width, height );
}

int	DG_GetIntegerv(int name, int *v)
{
	int result;

	in(1, "GetIntegerv (0x%x, 0x%p)", name, v);
	result = gl.GetIntegerv(name, v);
	out(1, "GetIntegerv: %i, %i", result, v[0]);
	return result;
}

int DG_GetInteger(int name)
{
	int result;

	in(1, "GetInteger (0x%x)", name);
	result = gl.GetInteger(name);
	out(1, "GetInteger: %i", result);
	return result;
}

int	DG_SetInteger(int name, int value)
{
	int result;

	in(1, "SetInteger (0x%x, %i)", name, value);
	result = gl.SetInteger(name, value);
	out(1, "SetInteger: %i", result);
	return result;
}

char* DG_GetString(int name)
{
	char *result;

	in(1, "GetString (0x%x)", name);
	result = gl.GetString(name);
	out(1, "GetString: %p (%s)", result, result? result : "(null)");
	return result;
}

int DG_SetFloatv(int name, float *values)
{
	int result;

	in(1, "SetFloatv (0x%x, 0x%p)", name, values);
	result = gl.SetFloatv(name, values);
	out(1, "SetFloatv: %i", result);
	return result;
}
	
int DG_Enable(int cap)
{
	INT1_INT( Enable, cap );
}

void DG_Disable(int cap)
{
	INT1_VOID( Disable, cap );
}

void DG_Func(int func, int param1, int param2)
{
	in(1, "Func (0x%x, 0x%x, 0x%x)", func, param1, param2);
	gl.Func(func, param1, param2);
	out(1, "Func");
}

void DG_ZBias(int level)
{
	INT1_VOID( ZBias, level );
}

void DG_MatrixMode(int mode)
{
	INT1_VOID( MatrixMode, mode);
}

void DG_PushMatrix(void)
{
	SIMPLE( PushMatrix );
}

void DG_PopMatrix(void)
{
	SIMPLE( PopMatrix );
}

void DG_LoadIdentity(void)
{
	SIMPLE( LoadIdentity );
}

void DG_Translatef(float x, float y, float z)
{
	in(2, "Translatef (%f, %f, %f)", x, y, z);
	gl.Translatef(x, y, z);
	out(2, "Translatef");
}

void DG_Rotatef(float angle, float x, float y, float z)
{
	in(2, "Rotatef (%f, %f, %f, %f)", angle, x, y, z);
	gl.Rotatef(angle, x, y, z);
	out(2, "Rotatef");
}

void DG_Scalef(float x, float y, float z)
{
	in(2, "Scalef (%f, %f, %f)", x, y, z);
	gl.Scalef(x, y, z);
	out(2, "Scalef");
}

void DG_Ortho(float left, float top, float right, float bottom, float znear, float zfar)
{
	in(2, "Ortho (%f, %f, %f, %f, %f, %f)", left, top, right, bottom, znear, zfar);
	gl.Ortho(left, top, right, bottom, znear, zfar);
	out(2, "Ortho");
}

void DG_Perspective(float fovy, float aspect, float zNear, float zFar)
{
	in(2, "Perspective (%f, %f, %f, %f)", fovy, aspect, zNear, zFar);
	gl.Perspective(fovy, aspect, zNear, zFar);
	out(2, "Perspective");
}

int DG_Grab(int x, int y, int width, int height, int format, void *buffer)
{
	int result;

	in(1, "Grab (%x, %x, %x, %x, 0x%x, 0x%p)",
		x, y, width, height, format, buffer);
	result = gl.Grab(x, y, width, height, format, buffer);
	out(1, "Grab: %i", result);
	return result;
}

void DG_Fog(int pname, float param)
{
	in(2, "Fog (0x%x, %f)", pname, param);
	gl.Fog(pname, param);
	out(2, "Fog");
}

void DG_Fogv(int pname, void *data)
{
	in(2, "Fogv (0x%x, %p)", pname, data);
	gl.Fogv(pname, data);
	out(2, "Fogv");
}

int DG_Project(int num, gl_fc3vertex_t *inVertices, gl_fc3vertex_t *outVertices)
{
	return gl.Project(num, inVertices, outVertices);
}

int DG_ReadPixels(int *inData, int format, void *pixels)
{
	return gl.ReadPixels(inData, format, pixels);
}

void DG_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
	in(3, "Color3ub (%i, %i, %i)", r, g, b);
	gl.Color3ub(r, g, b);
	out(3, "Color3ub");
}

void DG_Color3ubv(DGLubyte *data)
{
	in(3, "Color3ubv (0x%p)", data);
	gl.Color3ubv(data);
	out(3, "Color3ubv");
}

void DG_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
	in(3, "Color4ub (%i, %i, %i, %i)", r, g, b, a);
	gl.Color4ub(r, g, b, a);
	out(3, "Color4ub");
}

void DG_Color4ubv(DGLubyte *data)
{
	in(3, "Color4ubv (0x%p)", data);
	gl.Color4ubv(data);
	out(3, "Color4ubv");
}

void DG_Color3f(float r, float g, float b)
{
	in(3, "Color3f (%f, %f, %f)", r, g, b);
	gl.Color3f(r, g, b);
	out(3, "Color3f");
}

void DG_Color3fv(float *data)
{
	in(3, "Color3fv (0x%p)", data);
	gl.Color3fv(data);
	out(3, "Color3fv");
}

void DG_Color4f(float r, float g, float b, float a)
{
	in(3, "Color4f (%f, %f, %f, %f)", r, g, b, a);
	gl.Color4f(r, g, b, a);
	out(3, "Color4f");
}

void DG_Color4fv(float *data)
{
	in(3, "Color4fv (0x%p)", data);
	gl.Color4fv(data);
	out(3, "Color4fv");
}

void DG_TexCoord2f(float s, float t)
{
	in(3, "TexCoord2f (%f, %f)", s, t);
	gl.TexCoord2f(s, t);
	out(3, "TexCoord2f");
}

void DG_TexCoord2fv(float *data)
{
	in(3, "TexCoord2fv (0x%p)", data);
	gl.TexCoord2fv(data);
	out(3, "TexCoord2fv");
}

void DG_MultiTexCoord2f(int target, float s, float t)
{
	in(3, "MultiTexCoord2f (0x%x, %f, %f)", target, s, t);
	gl.MultiTexCoord2f(target, s, t);
	out(3, "MultiTexCoord2f");
}

void DG_MultiTexCoord2fv(int target, float *data)
{
	in(3, "MultiTexCoord2fv (0x%x, 0x%p)", target, data);
	gl.MultiTexCoord2fv(target, data);
	out(3, "MultiTexCoord2fv");
}

void DG_Vertex2f(float x, float y)
{
	in(3, "Vertex2f (%f, %f)", x, y);
	gl.Vertex2f(x, y);
	out(3, "Vertex2f");
}

void DG_Vertex2fv(float *data)
{
	in(3, "Vertex2fv (0x%p)", data);
	gl.Vertex2fv(data);
	out(3, "Vertex2fv");
}

void DG_Vertex3f(float x, float y, float z)
{
	in(3, "Vertex3f (%f, %f, %f)", x, y, z);
	gl.Vertex3f(x, y, z);
	out(3, "Vertex3f");
}

void DG_Vertex3fv(float *data)
{
	in(3, "Vertex3fv (0x%p)", data);
	gl.Vertex3fv(data);
	out(3, "Vertex3fv");
}

void DG_Vertices2ftv(int num, gl_ft2vertex_t *data)
{
	gl.Vertices2ftv(num, data);
}

void DG_Vertices3ftv(int num, gl_ft3vertex_t *data)
{
	gl.Vertices3ftv(num, data);
}

void DG_Vertices3fctv(int num, gl_fct3vertex_t *data)
{
	gl.Vertices3fctv(num, data);
}

void DG_Begin(int mode)
{
	INT1_VOID( Begin, mode );
}

void DG_End(void)
{
	SIMPLE( End );
}

void DG_EnableArrays(int vertices, int colors, int coords)
{
	in(1, "EnableArrays (%i, %i, 0x%x)", vertices, colors, coords);
	gl.EnableArrays(vertices, colors, coords);
	out(1, "EnableArrays");
}

void DG_DisableArrays(int vertices, int colors, int coords)
{
	in(1, "DisableArrays (%i, %i, 0x%x)", vertices, colors, coords);
	gl.DisableArrays(vertices, colors, coords);
	out(1, "DisableArrays");
}

void DG_Arrays(void *vertices, void *colors, int numCoords, 
			   void **coords, int lock)
{
	in(1, "Arrays (0x%p, 0x%p, %i, 0x%p, %i)", vertices, colors, numCoords,
		coords, lock);
	gl.Arrays(vertices, colors, numCoords, coords, lock);
	out(1, "Arrays");
}

void DG_UnlockArrays(void)
{
	SIMPLE( UnlockArrays );
}

void DG_ArrayElement(int index)
{
	INT1_VOID( ArrayElement, index );
}

void DG_DrawElements(int type, int count, unsigned int *indices)
{
	in(1, "DrawElements (%i, %i, 0x%p)", type, count, indices);
	gl.DrawElements(type, count, indices);
	out(1, "DrawElements");
}

int DG_NewTexture(void)
{
	int result;

	in(1, "NewTexture");
	result = gl.NewTexture();
	out(1, "NewTexture: %i", result);
	return result;
}

int DG_TexImage(int format, int width, int height, int genMips, void *data)
{
	int result;

	in(1, "TexImage (0x%x, %i, %i, 0x%x, 0x%p)", format, width, height,
		genMips, data);
	result = gl.TexImage(format, width, height, genMips, data);
	out(1, "TexImage: %i", result);
	return result;
}

void DG_DeleteTextures(int num, DGLuint *names)
{
	in(1, "DeleteTextures (%i, 0x%p)", num, names);
	gl.DeleteTextures(num, names);
	out(1, "DeleteTextures");
}

void DG_TexParameter(int pname, int param)
{
	in(1, "TexParameter (0x%x, 0x%x)", pname, param);
	gl.TexParameter(pname, param);
	out(1, "TexParameter");
}

void DG_GetTexParameterv(int level, int pname, int *v)
{
	in(1, "GetTexParameterv (%i, 0x%p, 0x%p)", level, pname, v);
	gl.GetTexParameterv(level, pname, v);
	out(1, "GetTexParameterv");
}

void DG_Palette(int format, void *data)
{
	in(1, "Palette (0x%x, 0x%p)", format, data);
	gl.Palette(format, data);
	out(1, "Palette");
}

int	DG_Bind(DGLuint texture)
{
	INT1_INT( Bind, texture );
}