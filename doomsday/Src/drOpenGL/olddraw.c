// drOpenGL.dll
// OpenGL driver for the Doomsday Graphics Library
//
// draw.c : Drawing operations, vertex handling

#include "drOpenGL.h"
#include <stdio.h>
#include <stdlib.h>

/*#define DEBUGMODE
#define DEBUG
#include <dprintf.h>
#include <assert.h>*/

// Vertex flags.
#define VTXF_POS_2		0x1
#define VTXF_POS_3		0x2
#define VTXF_POS_MASK	0x3
#define VTXF_COLOR_3	0x4
#define VTXF_COLOR_4	0x8
#define VTXF_COLOR_MASK	0xc
#define VTXF_TEX		0x10		

typedef struct
{
	float pos[3];
	float color[4];
	float tex[2];
	char flags;
}
vertex_t;


int			primType = DGL_FALSE;	// Set in Begin() if the stack is needed.
int			primMode = DGL_FALSE;	// Begin() sets this always.
vertex_t	*vertexStack = 0;
int			stackSize = 0, stackPos = 0;
//int			vtxStackPos = 0, vtxCounter = 0;
float		current_color[4] = { 1, 1, 1, 1 };

vertex_t	currentVertex;		
int			inSequence = DGL_FALSE;




// ------------------------------------------------------------------------

void InitVertexStack()
{
	stackSize = 4096;
	vertexStack = malloc(sizeof(vertex_t) * stackSize);
}

void KillVertexStack()
{
	stackSize = 0;
	free(vertexStack);
}

//===========================================================================
// CurrentColor
//	Update the current color.
//===========================================================================
void CurrentColor(int comps)
{
	vertex_t *vtx = vertexStack + stackPos;

	if(comps == 3)
	{
		memcpy(current_color, vtx->color, sizeof(float) * 3);	
		current_color[3] = 1;
	}
	else
	{
		memcpy(current_color, vtx->color, sizeof(float) * 4);
	}
}

// API FUNCTIONS ----------------------------------------------------------

void Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
	vertex_t *vtx = vertexStack + stackPos;

	vtx->flags &= ~VTXF_COLOR_MASK;
	vtx->flags |= VTXF_COLOR_3;
	vtx->color[0] = r / (float) 0xff;
	vtx->color[1] = g / (float) 0xff;
	vtx->color[2] = b / (float) 0xff;
	CurrentColor(3);	
}


void Color3ubv(void *data)
{
	vertex_t *vtx = vertexStack + stackPos;
	int i;

	vtx->flags &= ~VTXF_COLOR_MASK;
	vtx->flags |= VTXF_COLOR_3;
	for(i=0; i<3; i++) vtx->color[i] = ((DGLubyte*)data)[i] / (float) 0xff;
	CurrentColor(3);
}


void Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
	vertex_t *vtx = vertexStack + stackPos;

	vtx->flags &= ~VTXF_COLOR_MASK;
	vtx->flags |= VTXF_COLOR_4;
	vtx->color[0] = r / (float) 0xff;
	vtx->color[1] = g / (float) 0xff;
	vtx->color[2] = b / (float) 0xff;
	vtx->color[3] = a / (float) 0xff;
	CurrentColor(4);
}


void Color4ubv(void *data)
{
	vertex_t *vtx = vertexStack + stackPos;
	int i;

	vtx->flags &= ~VTXF_COLOR_MASK;
	vtx->flags |= VTXF_COLOR_4;
	for(i=0; i<4; i++) vtx->color[i] = ((DGLubyte*)data)[i] / (float) 0xff;
	CurrentColor(4);
}


void Color3f(float r, float g, float b)
{
/*	DGLubyte col[3];

	CLAMP01(r);	
	CLAMP01(g);
	CLAMP01(b);
	col[0] = (DGLubyte) (0xff * r);
	col[1] = (DGLubyte) (0xff * g);
	col[2] = (DGLubyte) (0xff * b);
	currentVertex.rgba = MAKE_RGBA(col[0], col[1], col[2], 0xff);*/

	vertex_t *vtx = vertexStack + stackPos;

	vtx->flags &= ~VTXF_COLOR_MASK;
	vtx->flags |= VTXF_COLOR_3;
	vtx->color[0] = r;
	vtx->color[1] = g;
	vtx->color[2] = b;
	CurrentColor(3);
}


void Color3fv(float *data)
{
	vertex_t *vtx = vertexStack + stackPos;
	int i;

	vtx->flags &= ~VTXF_COLOR_MASK;
	vtx->flags |= VTXF_COLOR_3;
	for(i=0; i<3; i++) vtx->color[i] = data[i];
	CurrentColor(3);
}


void Color4f(float r, float g, float b, float a)
{
/*	DGLubyte col[4];

	CLAMP01(r);	
	CLAMP01(g);
	CLAMP01(b);
	CLAMP01(a);
	col[0] = (DGLubyte) (0xff * r);
	col[1] = (DGLubyte) (0xff * g);
	col[2] = (DGLubyte) (0xff * b);
	col[3] = (DGLubyte) (0xff * a);
	currentVertex.rgba = MAKE_RGBA(col[0], col[1], col[2], col[3]);*/

	vertex_t *vtx = vertexStack + stackPos;

	vtx->flags &= ~VTXF_COLOR_MASK;
	vtx->flags |= VTXF_COLOR_4;
	vtx->color[0] = r;
	vtx->color[1] = g;
	vtx->color[2] = b;
	vtx->color[3] = a;
	CurrentColor(4);
}


void Color4fv(float *data)
{
/*	float		clamped[4] = { data[0], data[1], data[2], data[3] };
	DGLubyte	col[4];
	int			i;

	for(i=0; i<4; i++)
	{
		CLAMP01(clamped[i]);
		col[i] = (DGLubyte) (0xff * clamped[i]);
	}
	currentVertex.rgba = MAKE_RGBA(col[0], col[1], col[2], col[3]);*/

	vertex_t *vtx = vertexStack + stackPos;
	int i;

	vtx->flags &= ~VTXF_COLOR_MASK;
	vtx->flags |= VTXF_COLOR_4;
	for(i=0; i<4; i++) vtx->color[i] = data[i];
	CurrentColor(4);
}


void TexCoord2f(float s, float t)
{
/*	float	vec[3] = { s, t, 0 };

	if(!texturesEnabled) return;

	// We must apply the current texture matrix on the coordinates.
	vecMatMul(vec, getTexMatrix(), vec);

	currentVertex.s = vec[0];
	currentVertex.t = vec[1];*/

	vertex_t *vtx = vertexStack + stackPos;

	vtx->flags |= VTXF_TEX;
	vtx->tex[0] = s;
	vtx->tex[1] = t;
}


void TexCoord2fv(float *data)
{
/*	float	vec[3] = { data[0], data[1], 0 };

	if(!texturesEnabled) return;

	// We must apply the current texture matrix on the coordinates.
	vecMatMul(vec, getTexMatrix(), vec);

	currentVertex.s = vec[0];
	currentVertex.t = vec[1];*/

	vertex_t *vtx = vertexStack + stackPos;

	vtx->flags |= VTXF_TEX;
	vtx->tex[0] = data[0];
	vtx->tex[1] = data[1];
}


/*int	BeginScene(void)
{
	if(FAILED(hr = IDirect3DDevice3_BeginScene(d3dDevice)))
	{
		gim.Message( "drD3D.BeginScene: Failure! Error: %x\n", hr);
		return DGL_ERROR;
	}
	return DGL_OK;
}


int	EndScene(void)
{
	if(FAILED(hr = IDirect3DDevice3_EndScene(d3dDevice)))
	{
		gim.Message( "drD3D.EndScene: Failure! Error: %x\n", hr);
		return DGL_ERROR;
	}
	return DGL_OK;
}*/


void Begin(int mode)
{
/*	D3DPRIMITIVETYPE prim = (mode == DGL_LINES? D3DPT_LINELIST
		: mode == DGL_TRIANGLES? D3DPT_TRIANGLELIST
		: mode == DGL_TRIANGLE_FAN? D3DPT_TRIANGLEFAN
		: D3DPT_TRIANGLESTRIP);

	if(mode == DGL_SEQUENCE)
	{
		inSequence = DGL_TRUE;
		BeginScene();
		return;
	}

	primType = DGL_FALSE;
	primMode = prim;
	vtxCounter = 0;
	vtxStackPos = 0;

	if(!inSequence) BeginScene();

	if(mode == DGL_QUADS || mode == DGL_QUAD_STRIP)
	{
		// Setting primType will tell the vertex loader what to do
		// in these cases.
		primType = mode;
		if(mode == DGL_QUADS) return;	// We'll be using DrawPrimitive.
	}
	if(FAILED(hr = IDirect3DDevice3_Begin(d3dDevice, prim, VERTEX_FORMAT, 
		D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS)))
		gim.Message( "drD3D.Begin: Failure! Error: %x\n", hr);
		*/

	if(mode == DGL_SEQUENCE)
	{
		inSequence = DGL_TRUE;
		return;
	}

	primType = mode;
	stackPos = 0;
}


void End(void)
{
	int	i;
	vertex_t *vtx;

	if(!primType && inSequence)
	{
		inSequence = DGL_FALSE;
		return;
	}

	// FIXME: Why not use OpenGL arrays? The data could rather easily
	// be organized in arrays suitable for this kind of use. Should be
	// a "tad" faster than making lots of calls to the various color,
	// vertex and texcoord functions.

	glBegin(primType==DGL_POINTS? GL_POINTS
		: primType==DGL_LINES? GL_LINES 
		: primType==DGL_TRIANGLES? GL_TRIANGLES
		: primType==DGL_TRIANGLE_FAN? GL_TRIANGLE_FAN
		: primType==DGL_TRIANGLE_STRIP? GL_TRIANGLE_STRIP
		: primType==DGL_QUAD_STRIP? GL_QUAD_STRIP
		: GL_QUADS);

	for(i=0, vtx=vertexStack; i<stackPos; i++, vtx++)
	{
		if(vtx->flags & VTXF_COLOR_3)
			glColor3fv(vtx->color);
		else if(vtx->flags & VTXF_COLOR_4)
			glColor4fv(vtx->color);

		if(vtx->flags & VTXF_TEX)
			glTexCoord2fv(vtx->tex);

		if(vtx->flags & VTXF_POS_2)
			glVertex2fv(vtx->pos);
		else 
			glVertex3fv(vtx->pos);
	}

	glEnd();
#ifdef DEBUGMODE
	{
		int code = glGetError();
		dprintf("GL Error: %x", code);
		//assert(code == GL_NO_ERROR);
	}
#endif
	primType = DGL_FALSE;
	stackPos = 0;
	vertexStack[0].flags = 0;

	/*if(primMode == DGL_FALSE)
	{
		if(inSequence)
		{
			// Sequence ends.
			inSequence = DGL_FALSE;
			EndScene();
		}
		return;
	}

	if(primType != DGL_QUADS)
	{
		if(FAILED(hr = IDirect3DDevice3_End(d3dDevice, 0)))
			gim.Message( "drD3D.End: Failure! Error: %x\n", hr);
	}
	if(!inSequence)	EndScene();

	primMode = primType = DGL_FALSE;*/
}


static void VtxToStack()
{
	stackPos++;
	if(stackPos == stackSize)
	{
		// We need to allocate more memory.
		stackSize += 1024;
		vertexStack = realloc(vertexStack, sizeof(vertex_t) * stackSize);
	}
	vertexStack[stackPos].flags = 0;
}


void Vertex2f(float x, float y)
{
/*	currentVertex.x = x;
	currentVertex.y = y;
	currentVertex.z = 0;
	uploadVertex();*/

	vertex_t *vtx = vertexStack + stackPos;

	vtx->flags &= ~VTXF_POS_MASK;
	vtx->flags |= VTXF_POS_2;
	vtx->pos[0] = x;
	vtx->pos[1] = y;
	
	VtxToStack();
}


void Vertex2fv(float *data)
{
	/*currentVertex.x = data[0];
	currentVertex.y = data[1];
	currentVertex.z = 0;
	uploadVertex();*/
	vertex_t *vtx = vertexStack + stackPos;

	vtx->flags &= ~VTXF_POS_MASK;
	vtx->flags |= VTXF_POS_2;
	vtx->pos[0] = data[0];
	vtx->pos[1] = data[1];
	
	VtxToStack();
}


void Vertex3f(float x, float y, float z)
{
/*	currentVertex.x = x;
	currentVertex.y = y;
	currentVertex.z = z;
	uploadVertex();*/
	vertex_t *vtx = vertexStack + stackPos;

	vtx->flags &= ~VTXF_POS_MASK;
	vtx->flags |= VTXF_POS_3;
	vtx->pos[0] = x;
	vtx->pos[1] = y;
	vtx->pos[2] = z;
	
	VtxToStack();
}


void Vertex3fv(float *data)
{
/*	currentVertex.x = data[0];
	currentVertex.y = data[1];
	currentVertex.z = data[2];
	uploadVertex();*/
	vertex_t *vtx = vertexStack + stackPos;

	vtx->flags &= ~VTXF_POS_MASK;
	vtx->flags |= VTXF_POS_3;
	vtx->pos[0] = data[0];
	vtx->pos[1] = data[1];
	vtx->pos[2] = data[2];
	
	VtxToStack();
}


void Vertices2ftv(int num, gl_ft2vertex_t *data)
{
	for(; num>0; num--, data++)
	{
		TexCoord2fv(data->tex);
		Vertex2fv(data->pos);
	}
}


void Vertices3ftv(int num, gl_ft3vertex_t *data)
{
	for(; num>0; num--, data++)
	{
		TexCoord2fv(data->tex);
		Vertex3fv(data->pos);
	}
}


void Vertices3fctv(int num, gl_fct3vertex_t *data)
{
	for(; num>0; num--, data++)
	{
		Color4fv(data->color);
		TexCoord2fv(data->tex);
		Vertex3fv(data->pos);
	}
}

