
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

int			polyCounter;	// Triangle counter, really.
char		useTexCoords[MAX_TEX_UNITS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

int			primType;		// Set in Begin() if the stack is needed.
int			primMode;		// Begin() sets this always.
int			inSequence;
glvertex_t	currentVertex;		

glvertex_t	*vertexStack;
int			stackSize, stackPos;

// CODE --------------------------------------------------------------------

//===========================================================================
// InitVertexStack
//===========================================================================
void InitVertexStack(void)
{
	int i;

	memset(&currentVertex, 0, sizeof(currentVertex));
	for(i = 0; i < 4; i++) currentVertex.color[i] = 1;

	polyCounter = 0;
	primType = DGL_FALSE;
	primMode = DGL_FALSE;
	inSequence = DGL_FALSE;
	stackPos = 0;
	stackSize = 4096;
	vertexStack = malloc(sizeof(glvertex_t) * stackSize);
	memset(vertexStack, 0, sizeof(glvertex_t) * stackSize);
}

//===========================================================================
// KillVertexStack
//===========================================================================
void KillVertexStack(void)
{
	stackSize = 0;
	free(vertexStack);
}

//===========================================================================
// VtxToStack
//===========================================================================
void VtxToStack(void)
{
	memcpy(vertexStack + stackPos++, &currentVertex, sizeof(currentVertex));
	if(stackPos == stackSize)
	{
		// We need to allocate more memory.
		stackSize += 2048;
		vertexStack = realloc(vertexStack, sizeof(glvertex_t) * stackSize);
	}
}

//===========================================================================
// DG_Color3ub
//===========================================================================
void DG_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
	currentVertex.color[0] = r / 255.0f;
	currentVertex.color[1] = g / 255.0f;
	currentVertex.color[2] = b / 255.0f;
	currentVertex.color[3] = 1;
}

//===========================================================================
// DG_Color3ubv
//===========================================================================
void DG_Color3ubv(DGLubyte *data)
{
	currentVertex.color[0] = data[0] / 255.0f;
	currentVertex.color[1] = data[1] / 255.0f;
	currentVertex.color[2] = data[2] / 255.0f;
	currentVertex.color[3] = 1;	
}

//===========================================================================
// DG_Color4ub
//===========================================================================
void DG_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
	currentVertex.color[0] = r / 255.0f;
	currentVertex.color[1] = g / 255.0f;
	currentVertex.color[2] = b / 255.0f;
	currentVertex.color[3] = a / 255.0f;
}

//===========================================================================
// DG_Color4ubv
//===========================================================================
void DG_Color4ubv(DGLubyte *data)
{
	currentVertex.color[0] = data[0] / 255.0f;
	currentVertex.color[1] = data[1] / 255.0f;
	currentVertex.color[2] = data[2] / 255.0f;
	currentVertex.color[3] = data[3] / 255.0f;	
}

//===========================================================================
// DG_Color3f
//===========================================================================
void DG_Color3f(float r, float g, float b)
{
	currentVertex.color[0] = r;
	currentVertex.color[1] = g;
	currentVertex.color[2] = b;
	currentVertex.color[3] = 1;	
}

//===========================================================================
// DG_Color3fv
//===========================================================================
void DG_Color3fv(float *data)
{
	currentVertex.color[0] = data[0];
	currentVertex.color[1] = data[1];
	currentVertex.color[2] = data[2];
	currentVertex.color[3] = 1;	
}

//===========================================================================
// DG_Color4f
//===========================================================================
void DG_Color4f(float r, float g, float b, float a)
{
	currentVertex.color[0] = r;
	currentVertex.color[1] = g;
	currentVertex.color[2] = b;
	currentVertex.color[3] = a;	
}

//===========================================================================
// DG_Color4fv
//===========================================================================
void DG_Color4fv(float *data)
{
	currentVertex.color[0] = data[0];
	currentVertex.color[1] = data[1];
	currentVertex.color[2] = data[2];
	currentVertex.color[3] = data[3];	
}

//===========================================================================
// DG_TexCoord2f
//===========================================================================
void DG_TexCoord2f(float s, float t)
{
	currentVertex.tex[0] = s;
	currentVertex.tex[1] = t;
	useTexCoords[0] = DGL_TRUE;
}

//===========================================================================
// DG_TexCoord2fv
//===========================================================================
void DG_TexCoord2fv(float *data)
{
	currentVertex.tex[0] = data[0];
	currentVertex.tex[1] = data[1];
	useTexCoords[0] = DGL_TRUE;
}

//===========================================================================
// DG_MultiTexCoord2f
//===========================================================================
void DG_MultiTexCoord2f(int target, float s, float t)
{
	float *tex = 
		 (target == DGL_TEXTURE0? currentVertex.tex
		: target == DGL_TEXTURE1? currentVertex.tex2 
		: target == DGL_TEXTURE2? currentVertex.tex3
		: currentVertex.tex4);

	tex[0] = s;
	tex[1] = t;
	useTexCoords[target - DGL_TEXTURE0] = DGL_TRUE;
}

//===========================================================================
// DG_MultiTexCoord2fv
//===========================================================================
void DG_MultiTexCoord2fv(int target, float *data)
{
	currentVertex.tex[0] = data[0];
	currentVertex.tex[1] = data[1];
	useTexCoords[target - DGL_TEXTURE0] = DGL_TRUE;
}

//===========================================================================
// DG_Vertex2f
//===========================================================================
void DG_Vertex2f(float x, float y)
{
	currentVertex.pos[0] = x;
	currentVertex.pos[1] = y;
	currentVertex.pos[2] = 0;
	VtxToStack();
}

//===========================================================================
// DG_Vertex2fv
//===========================================================================
void DG_Vertex2fv(float *data)
{
	currentVertex.pos[0] = data[0];
	currentVertex.pos[1] = data[1];
	currentVertex.pos[2] = 0;
	VtxToStack();
}

//===========================================================================
// DG_Vertex3f
//===========================================================================
void DG_Vertex3f(float x, float y, float z)
{
	currentVertex.pos[0] = x;
	currentVertex.pos[1] = y;
	currentVertex.pos[2] = z;
	VtxToStack();
}

//===========================================================================
// DG_Vertex3fv
//===========================================================================
void DG_Vertex3fv(float *data)
{
	currentVertex.pos[0] = data[0];
	currentVertex.pos[1] = data[1];
	currentVertex.pos[2] = data[2];
	VtxToStack();
}

//===========================================================================
// DG_Vertices2ftv
//===========================================================================
void DG_Vertices2ftv(int num, gl_ft2vertex_t *data)
{
	for(; num > 0; num--, data++)
	{
		DG_TexCoord2fv(data->tex);
		DG_Vertex2fv(data->pos);
	}
}

//===========================================================================
// DG_Vertices3ftv
//===========================================================================
void DG_Vertices3ftv(int num, gl_ft3vertex_t *data)
{
	for(; num>0; num--, data++)
	{
		DG_TexCoord2fv(data->tex);
		DG_Vertex3fv(data->pos);
	}
}

//===========================================================================
// DG_Vertices3fctv
//===========================================================================
void DG_Vertices3fctv(int num, gl_fct3vertex_t *data)
{
	for(; num>0; num--, data++)
	{
		DG_Color4fv(data->color);
		DG_TexCoord2fv(data->tex);
		DG_Vertex3fv(data->pos);
	}
}

//===========================================================================
// DG_Begin
//===========================================================================
void DG_Begin(int mode)
{
	if(mode == DGL_SEQUENCE)
	{
		inSequence = DGL_TRUE;
		return;
	}

	primType = mode;
	stackPos = 0;

	memset(useTexCoords, 0, sizeof(useTexCoords));
}

//===========================================================================
// DG_End
//===========================================================================
void DG_End(void)
{
	int i, k, glPrimitive = 
		  primType == DGL_POINTS? GL_POINTS
		: primType == DGL_LINES? GL_LINES 
		: primType == DGL_TRIANGLES? GL_TRIANGLES
		: primType == DGL_TRIANGLE_FAN? GL_TRIANGLE_FAN
		: primType == DGL_TRIANGLE_STRIP? GL_TRIANGLE_STRIP
		: primType == DGL_QUAD_STRIP? GL_QUAD_STRIP
		: GL_QUADS;

	if(!primType && inSequence)
	{
		inSequence = DGL_FALSE;
		return;
	}

	if(noArrays)
	{
		glvertex_t *v;
		// Make individual vertex calls.
		glBegin(glPrimitive);
		for(i = 0, v = vertexStack; i < stackPos; i++, v++)
		{
			glColor4fv(v->color);
			if(useTexCoords[0]) glTexCoord2fv(v->tex);
			for(k = 1; k < MAX_TEX_UNITS; k++)
			{
				if(useTexCoords[k]) 
				{
					glMultiTexCoord2fvARB(GL_TEXTURE0 + k, 
						  k == 1? v->tex2
						: k == 2? v->tex3 
						: v->tex4);
				}
			}
			glVertex3fv(v->pos);
		}
		glEnd();
	}
	else
	{
		// Load in as arrays.
		glVertexPointer(3, GL_FLOAT, sizeof(glvertex_t), vertexStack->pos);
		glColorPointer(4, GL_FLOAT, sizeof(glvertex_t), vertexStack->color);
		if(useTexCoords) 
		{
			if(!texCoordPtrEnabled)
			{
				// We need TexCoords, but the array is disabled.
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				texCoordPtrEnabled = true;
			}
			glTexCoordPointer(2, GL_FLOAT, sizeof(glvertex_t), vertexStack->tex);
		}
		else if(texCoordPtrEnabled)
		{
			// TexCoords are not needed, but the array is enabled.
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			texCoordPtrEnabled = false;
		}
		glDrawArrays(glPrimitive, 0, stackPos);
	}
	
	// Increase the triangle counter.
	polyCounter += 
		  primType == DGL_TRIANGLES? stackPos/3
		: primType == DGL_TRIANGLE_FAN? stackPos - 2
		: primType == DGL_TRIANGLE_STRIP? stackPos - 2
		: primType == DGL_QUAD_STRIP? stackPos - 2
		: primType == DGL_QUADS? stackPos/2
		: 0;

	// Clear the stack.
	stackPos = 0;
	primType = DGL_FALSE;
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
			{
				glClientActiveTextureARB(GL_TEXTURE0 + i);
			}
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
			{
				glClientActiveTextureARB(GL_TEXTURE0 + i);
			}
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
			{
				glClientActiveTextureARB(GL_TEXTURE0 + i);
			}
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
// DG_DrawElements
//===========================================================================
void DG_DrawElements(int type, int count, unsigned int *indices)
{
	if(type == DGL_TRIANGLE_FAN)
	{
		glDrawElements(GL_TRIANGLE_FAN, count, GL_UNSIGNED_INT, indices);
	}
	else if(type == DGL_TRIANGLES) 
	{
		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, indices);
	}
}