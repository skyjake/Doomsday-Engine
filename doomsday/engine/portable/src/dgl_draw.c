/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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
 * dgl_draw.c: Drawing Operations and Vertex Arrays.
 *
 * Get OpenGL header files from:
 * http://oss.sgi.com/projects/ogl-sample/
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_dgl.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAX_ARRAYS	(2 + MAX_TEX_UNITS)

// TYPES -------------------------------------------------------------------

typedef struct array_s {
	boolean enabled;
	void   *data;
} array_t;

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     polyCounter;			// Triangle counter, really.
int     primLevel;

#ifdef _DEBUG
int     inPrim;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

array_t arrays[MAX_ARRAYS];

// CODE --------------------------------------------------------------------

void InitArrays(void)
{
	double  version = strtod((const char*) glGetString(GL_VERSION), NULL);

	// If the driver's OpenGL version is older than 1.3, disable arrays
	// by default.
	DGL_state.noArrays = (version < 1.3);

	// Override the automatic selection?
	if(ArgExists("-vtxar"))
		DGL_state.noArrays = false;
	if(ArgExists("-novtxar"))
		DGL_state.noArrays = true;

	if(!DGL_state.noArrays)
		return;
	memset(arrays, 0, sizeof(arrays));
}

void CheckError(void)
{
#ifdef _DEBUG
	GLenum  error;

	if((error = glGetError()) != GL_NO_ERROR)
		Con_Error("OpenGL error: %i\n", error);
#endif
}

void DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
	glColor3ub(r, g, b);
}

void DGL_Color3ubv(const DGLubyte *data)
{
	glColor3ubv(data);
}

void DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
	glColor4ub(r, g, b, a);
}

void DGL_Color4ubv(const DGLubyte *data)
{
	glColor4ubv(data);
}

void DGL_Color3f(float r, float g, float b)
{
	glColor3f(r, g, b);
}

void DGL_Color3fv(const float *data)
{
	glColor3fv(data);
}

void DGL_Color4f(float r, float g, float b, float a)
{
	glColor4f(r, g, b, a);
}

void DGL_Color4fv(const float *data)
{
	glColor4fv(data);
}

void DGL_TexCoord2f(float s, float t)
{
	glTexCoord2f(s, t);
}

void DGL_TexCoord2fv(const float *data)
{
	glTexCoord2fv(data);
}

void DGL_MultiTexCoord2f(byte target, float s, float t)
{
	if(target == 0)
		glTexCoord2f(s, t);
	else
		glMultiTexCoord2fARB(GL_TEXTURE0 + target, s, t);
}

void DGL_MultiTexCoord2fv(byte target, float *data)
{
	if(target == 0)
		glTexCoord2fv(data);
	else
		glMultiTexCoord2fvARB(GL_TEXTURE0 + target, data);
}

void DGL_Vertex2f(float x, float y)
{
	glVertex2f(x, y);
}

void DGL_Vertex2fv(const float *data)
{
	glVertex2fv(data);
}

void DGL_Vertex3f(float x, float y, float z)
{
	glVertex3f(x, y, z);
}

void DGL_Vertex3fv(const float *data)
{
	glVertex3fv(data);
}

void DGL_Vertices2ftv(int num, const gl_ft2vertex_t *data)
{
	for(; num > 0; num--, data++)
	{
		glTexCoord2fv(data->tex);
		glVertex2fv(data->pos);
	}
}

void DGL_Vertices3ftv(int num, const gl_ft3vertex_t *data)
{
	for(; num > 0; num--, data++)
	{
		glTexCoord2fv(data->tex);
		glVertex3fv(data->pos);
	}
}

void DGL_Vertices3fctv(int num, const gl_fct3vertex_t *data)
{
	for(; num > 0; num--, data++)
	{
		glColor4fv(data->color);
		glTexCoord2fv(data->tex);
		glVertex3fv(data->pos);
	}
}

void DGL_Begin(glprimtype_t mode)
{
	// We enter a Begin/End section.
	primLevel++;

#ifdef _DEBUG
	if(inPrim)
		Con_Error("OpenGL: already inPrim");
	inPrim = true;
	CheckError();
#endif

	glBegin(mode == DGL_POINTS ? GL_POINTS : mode ==
			DGL_LINES ? GL_LINES : mode ==
			DGL_TRIANGLES ? GL_TRIANGLES : mode ==
			DGL_TRIANGLE_FAN ? GL_TRIANGLE_FAN : mode ==
			DGL_TRIANGLE_STRIP ? GL_TRIANGLE_STRIP : mode ==
			DGL_QUAD_STRIP ? GL_QUAD_STRIP : GL_QUADS);
}

void DGL_End(void)
{
	if(primLevel > 0)
	{
		primLevel--;
		glEnd();
	}

#ifdef _DEBUG
	inPrim = false;
	CheckError();
#endif
}

void DGL_EnableArrays(int vertices, int colors, int coords)
{
	int         i;

	if(vertices)
	{
		if(DGL_state.noArrays)
			arrays[AR_VERTEX].enabled = true;
		else
			glEnableClientState(GL_VERTEX_ARRAY);
	}

	if(colors)
	{
		if(DGL_state.noArrays)
			arrays[AR_COLOR].enabled = true;
		else
			glEnableClientState(GL_COLOR_ARRAY);
	}

	for(i = 0; i < DGL_state.maxTexUnits && i < MAX_TEX_UNITS; i++)
	{
		if(coords & (1 << i))
		{
			if(DGL_state.noArrays)
			{
				arrays[AR_TEXCOORD0 + i].enabled = true;
			}
			else
			{
#ifndef UNIX
				if(glClientActiveTextureARB)
#endif
					glClientActiveTextureARB(GL_TEXTURE0 + i);

				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		}
	}

#ifdef _DEBUG
	CheckError();
#endif
}

void DGL_DisableArrays(int vertices, int colors, int coords)
{
	int         i;

	if(vertices)
	{
		if(DGL_state.noArrays)
			arrays[AR_VERTEX].enabled = false;
		else
			glDisableClientState(GL_VERTEX_ARRAY);
	}

	if(colors)
	{
		if(DGL_state.noArrays)
			arrays[AR_COLOR].enabled = false;
		else
			glDisableClientState(GL_COLOR_ARRAY);
	}

	for(i = 0; i < DGL_state.maxTexUnits && i < MAX_TEX_UNITS; i++)
	{
		if(coords & (1 << i))
		{
			if(DGL_state.noArrays)
			{
				arrays[AR_TEXCOORD0 + i].enabled = false;
			}
			else
			{
#ifndef UNIX
				if(glClientActiveTextureARB)
#endif
					glClientActiveTextureARB(GL_TEXTURE0 + i);

				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				glTexCoordPointer(2, GL_FLOAT, 0, NULL);
			}
		}
	}

#ifdef _DEBUG
	CheckError();
#endif
}

/**
 * Enable, set and optionally lock all enabled arrays.
 */
void DGL_Arrays(void *vertices, void *colors, int numCoords, void **coords,
			    int lock)
{
	int         i;

	if(vertices)
	{
		if(DGL_state.noArrays)
		{
			arrays[AR_VERTEX].enabled = true;
			arrays[AR_VERTEX].data = vertices;
		}
		else
		{
			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(3, GL_FLOAT, 16, vertices);
		}
	}

	if(colors)
	{
		if(DGL_state.noArrays)
		{
			arrays[AR_COLOR].enabled = true;
			arrays[AR_COLOR].data = colors;
		}
		else
		{
			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
		}
	}

	for(i = 0; i < numCoords && i < MAX_TEX_UNITS; ++i)
	{
		if(coords[i])
		{
			if(DGL_state.noArrays)
			{
				arrays[AR_TEXCOORD0 + i].enabled = true;
				arrays[AR_TEXCOORD0 + i].data = coords[i];
			}
			else
			{
#ifndef UNIX
				if(glClientActiveTextureARB)
#endif
					glClientActiveTextureARB(GL_TEXTURE0 + i);

				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glTexCoordPointer(2, GL_FLOAT, 0, coords[i]);
			}
		}
	}

#ifndef UNIX
	if(glLockArraysEXT)
#endif
        if(!DGL_state.noArrays && lock > 0)
        {   // 'lock' is the number of vertices to lock.
            glLockArraysEXT(0, lock);
        }

#ifdef _DEBUG
	CheckError();
#endif
}

void DGL_UnlockArrays(void)
{
	if(!DGL_state.noArrays)
	{
#ifndef UNIX
        if(glUnlockArraysEXT)
#endif
            glUnlockArraysEXT();
	}

#ifdef _DEBUG
	CheckError();
#endif
}

void DGL_ArrayElement(int index)
{
	if(!DGL_state.noArrays)
	{
		glArrayElement(index);
	}
	else
	{
		int         i;

		for(i = 0; i < DGL_state.maxTexUnits && i < MAX_TEX_UNITS; ++i)
		{
			if(arrays[AR_TEXCOORD0 + i].enabled)
			{
				glMultiTexCoord2fvARB(GL_TEXTURE0 + i,
									  ((gl_texcoord_t *)
									   arrays[AR_TEXCOORD0 +
											  i].data)[index].st);
			}
		}

		if(arrays[AR_COLOR].enabled)
			glColor4ubv(((gl_color_t *) arrays[AR_COLOR].data)[index].rgba);

		if(arrays[AR_VERTEX].enabled)
			glVertex3fv(((gl_vertex_t *) arrays[AR_VERTEX].data)[index].xyz);
	}
}

void DGL_DrawElements(glprimtype_t type, int count, const uint *indices)
{
	GLenum          primType =
		(type == DGL_TRIANGLE_FAN ? GL_TRIANGLE_FAN : type ==
		 DGL_TRIANGLE_STRIP ? GL_TRIANGLE_STRIP : GL_TRIANGLES);

	if(!DGL_state.noArrays)
	{
		glDrawElements(primType, count, GL_UNSIGNED_INT, indices);
	}
	else
	{
		int         i;

		glBegin(primType);
		for(i = 0; i < count; ++i)
		{
			DGL_ArrayElement(indices[i]);
		}
		glEnd();
	}

#ifdef _DEBUG
	CheckError();
#endif
}
