
//**************************************************************************
//**
//** DRAW.C
//**
//** Target:		DGL Driver for OpenGL
//** Description:	Drawing operations, vertex stack management
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "drOpenGL.h"

// MACROS ------------------------------------------------------------------

#define MAX_TEX_UNITS 4

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int	polyCounter;	// Triangle counter, really.
int primLevel;

#ifdef _DEBUG
int	inPrim;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// CheckError
//===========================================================================
void CheckError(void)
{
#ifdef _DEBUG
	GLenum error;

	if((error = glGetError()) != GL_NO_ERROR)
		Con_Error("OpenGL error: %i\n", error);
#endif
}

//===========================================================================
// DG_Color3ub
//===========================================================================
void DG_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
	glColor3ub(r, g, b);
}

//===========================================================================
// DG_Color3ubv
//===========================================================================
void DG_Color3ubv(DGLubyte *data)
{
	glColor3ubv(data);
}

//===========================================================================
// DG_Color4ub
//===========================================================================
void DG_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
	glColor4ub(r, g, b, a);
}

//===========================================================================
// DG_Color4ubv
//===========================================================================
void DG_Color4ubv(DGLubyte *data)
{
	glColor4ubv(data);
}

//===========================================================================
// DG_Color3f
//===========================================================================
void DG_Color3f(float r, float g, float b)
{
	glColor3f(r, g, b);
}

//===========================================================================
// DG_Color3fv
//===========================================================================
void DG_Color3fv(float *data)
{
	glColor3fv(data);
}

//===========================================================================
// DG_Color4f
//===========================================================================
void DG_Color4f(float r, float g, float b, float a)
{
	glColor4f(r, g, b, a);
}

//===========================================================================
// DG_Color4fv
//===========================================================================
void DG_Color4fv(float *data)
{
	glColor4fv(data);
}

//===========================================================================
// DG_TexCoord2f
//===========================================================================
void DG_TexCoord2f(float s, float t)
{
	glTexCoord2f(s, t);
}

//===========================================================================
// DG_TexCoord2fv
//===========================================================================
void DG_TexCoord2fv(float *data)
{
	glTexCoord2fv(data);
}

//===========================================================================
// DG_MultiTexCoord2f
//===========================================================================
void DG_MultiTexCoord2f(int target, float s, float t)
{
	if(target == GL_TEXTURE0)
		glTexCoord2f(s, t);
	else
		glMultiTexCoord2fARB(GL_TEXTURE0 + (target - DGL_TEXTURE0), s, t);
}

//===========================================================================
// DG_MultiTexCoord2fv
//===========================================================================
void DG_MultiTexCoord2fv(int target, float *data)
{
	if(target == GL_TEXTURE0)
		glTexCoord2fv(data);
	else
		glMultiTexCoord2fvARB(GL_TEXTURE0 + (target - DGL_TEXTURE0), data);
}

//===========================================================================
// DG_Vertex2f
//===========================================================================
void DG_Vertex2f(float x, float y)
{
	glVertex2f(x, y);
}

//===========================================================================
// DG_Vertex2fv
//===========================================================================
void DG_Vertex2fv(float *data)
{
	glVertex2fv(data);
}

//===========================================================================
// DG_Vertex3f
//===========================================================================
void DG_Vertex3f(float x, float y, float z)
{
	glVertex3f(x, y, z);
}

//===========================================================================
// DG_Vertex3fv
//===========================================================================
void DG_Vertex3fv(float *data)
{
	glVertex3fv(data);
}

//===========================================================================
// DG_Vertices2ftv
//===========================================================================
void DG_Vertices2ftv(int num, gl_ft2vertex_t *data)
{
	for(; num > 0; num--, data++)
	{
		glTexCoord2fv(data->tex);
		glVertex2fv(data->pos);
	}
}

//===========================================================================
// DG_Vertices3ftv
//===========================================================================
void DG_Vertices3ftv(int num, gl_ft3vertex_t *data)
{
	for(; num > 0; num--, data++)
	{
		glTexCoord2fv(data->tex);
		glVertex3fv(data->pos);
	}
}

//===========================================================================
// DG_Vertices3fctv
//===========================================================================
void DG_Vertices3fctv(int num, gl_fct3vertex_t *data)
{
	for(; num > 0; num--, data++)
	{
		glColor4fv(data->color);
		glTexCoord2fv(data->tex);
		glVertex3fv(data->pos);
	}
}

//===========================================================================
// DG_Begin
//===========================================================================
void DG_Begin(int mode)
{
	if(mode == DGL_SEQUENCE) 
	{
		// We don't need to worry about this.
		return;
	}

	// We enter a Begin/End section.
	primLevel++;

#ifdef _DEBUG
	if(inPrim) Con_Error("OpenGL: already inPrim");
	inPrim = true;
	CheckError();
#endif

	glBegin( 
		  mode == DGL_POINTS?			GL_POINTS
		: mode == DGL_LINES?			GL_LINES 
		: mode == DGL_TRIANGLES?		GL_TRIANGLES
		: mode == DGL_TRIANGLE_FAN?		GL_TRIANGLE_FAN
		: mode == DGL_TRIANGLE_STRIP?	GL_TRIANGLE_STRIP
		: mode == DGL_QUAD_STRIP?		GL_QUAD_STRIP
		: GL_QUADS );
}

//===========================================================================
// DG_End
//===========================================================================
void DG_End(void)
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

//===========================================================================
// DG_EnableArrays
//===========================================================================
void DG_EnableArrays(int vertices, int colors, int coords)
{
	int i;

	if(vertices) glEnableClientState(GL_VERTEX_ARRAY);
	if(colors) glEnableClientState(GL_COLOR_ARRAY);
	
	for(i = 0; i < maxTexUnits; i++)
	{
		if(coords & (1 << i))
		{
			if(glClientActiveTextureARB)
				glClientActiveTextureARB(GL_TEXTURE0 + i);

			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}
}

//===========================================================================
// DG_DisableArrays
//===========================================================================
void DG_DisableArrays(int vertices, int colors, int coords)
{
	int i;
	
	if(vertices) glDisableClientState(GL_VERTEX_ARRAY);
	if(colors) glDisableClientState(GL_COLOR_ARRAY);

	for(i = 0; i < maxTexUnits; i++)
	{
		if(coords & (1 << i))
		{
			if(glClientActiveTextureARB)
				glClientActiveTextureARB(GL_TEXTURE0 + i);

			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}
}

//===========================================================================
// DG_Arrays
//	Enable, set and optionally lock all enabled arrays.
//===========================================================================
void DG_Arrays(void *vertices, void *colors, int numCoords, 
			   void **coords, int lock)
{
	int i;

	if(vertices)
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 16, vertices);
	}

	if(colors)
	{
		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
	}

	for(i = 0; i < numCoords; i++)
	{
		if(coords[i])
		{
			if(glClientActiveTextureARB)
				glClientActiveTextureARB(GL_TEXTURE0 + i);

			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(2, GL_FLOAT, 0, coords[i]);
		}
	}
	
	if(lock > 0 && glLockArraysEXT)
	{
		// 'lock' is the number of vertices to lock.
		glLockArraysEXT(0, lock);
	}
}

//===========================================================================
// DG_UnlockArrays
//===========================================================================
void DG_UnlockArrays(void)
{
	if(glUnlockArraysEXT)
	{
		glUnlockArraysEXT();
	}
}

//===========================================================================
// DG_ArrayElement
//===========================================================================
void DG_ArrayElement(int index)
{
	glArrayElement(index);
}

//===========================================================================
// DG_DrawElements
//===========================================================================
void DG_DrawElements(int type, int count, unsigned int *indices)
{
	glDrawElements( 
		  type == DGL_TRIANGLE_FAN?		GL_TRIANGLE_FAN
		: type == DGL_TRIANGLE_STRIP?	GL_TRIANGLE_STRIP
		: GL_TRIANGLES, 
		count, GL_UNSIGNED_INT, indices);
}