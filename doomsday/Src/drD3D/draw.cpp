
//**************************************************************************
//**
//** DRAW.CPP
//**
//** Target:		DGL Driver for Direct3D 8.1
//** Description:	Drawing of primitives
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "drD3D.h"

// MACROS ------------------------------------------------------------------

#define STACK_SIZE		240
#define COPYCV(idx)		memcpy(stack + (idx), &currentVertex, DRVSIZE);

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean		inSequence;
int			primType;
int			primOrder;
int			primCount;

// The stack is used for reordering and caching vertices before copying
// them to a vertex buffer.
int			stackPos;	// Beginning of the primitive.
drvertex_t	stack[STACK_SIZE], currentVertex;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// InitDraw
//===========================================================================
void InitDraw(void)
{
	inSequence = false;
	stackPos = 0;
}

//===========================================================================
// UploadStack
//===========================================================================
void UploadStack(void)
{
	BufferVertices(stack, stackPos);
	stackPos = 0;
}

//===========================================================================
// VtxToStack
//===========================================================================
void VtxToStack(void)
{
	switch(primType)
	{
	default:
		COPYCV(stackPos++);
		break;

	case DGL_QUADS:	// Translates to an indexed list of triangles.
		// stackPos always points to the beginning of the primitive.
		COPYCV(stackPos + primOrder++);
		if(primOrder == 4)
		{
			unsigned short first = primCount * 4, indices[6] = 
			{
				first, first + 1, first + 3,
				first + 1, first + 2, first + 3
			};
			// Quad complete, upload the vertex indices.
			BufferIndices(indices, 6);
			primOrder = 0;
			stackPos += 4;
			primCount++;
		}

/*		// stackPos always points to the beginning of the primitive.
		if(primOrder < 3)
		{
			// Normal copy.
			COPYCV(stackPos + primOrder++);
		}
		else
		{
			// The fourth vertex gets a bit of special treatment.
			// Fulfill into a second triangle.
			memcpy(stack + stackPos+3, stack + stackPos, DRVSIZE);
			memcpy(stack + stackPos+4, stack + stackPos+2, DRVSIZE);
			COPYCV(stackPos + 5);
			// Two triangles were written.
			stackPos += 6;
			primOrder = 0;
		}*/
		break;
	}

	if(stackPos == STACK_SIZE)
	{
		// The stack is full, let's upload it.
		UploadStack();
	}
}

//===========================================================================
// DG_Color3ub
//===========================================================================
void DG_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
	currentVertex.color = D3DCOLOR_XRGB(r, g, b);
}

//===========================================================================
// DG_Color3ubv
//===========================================================================
void DG_Color3ubv(DGLubyte *data)
{
	currentVertex.color = D3DCOLOR_XRGB(data[0], data[1], data[2]);
}

//===========================================================================
// DG_Color4ub
//===========================================================================
void DG_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
	currentVertex.color = D3DCOLOR_RGBA(r, g, b, a);
}

//===========================================================================
// DG_Color4ubv
//===========================================================================
void DG_Color4ubv(DGLubyte *data)
{
	currentVertex.color = D3DCOLOR_RGBA(data[0], data[1], data[2], data[3]);
}

//===========================================================================
// DG_Color3f
//===========================================================================
void DG_Color3f(float r, float g, float b)
{
	CLAMP01(r); CLAMP01(g); CLAMP01(b);
	currentVertex.color = D3DCOLOR_COLORVALUE(r, g, b, 1);
}

//===========================================================================
// DG_Color3fv
//===========================================================================
void DG_Color3fv(float *data)
{
	float r = data[0], g = data[1], b = data[2];
	CLAMP01(r); CLAMP01(g); CLAMP01(b);
	currentVertex.color = D3DCOLOR_COLORVALUE(r, g, b, 1);
}

//===========================================================================
// DG_Color4f
//===========================================================================
void DG_Color4f(float r, float g, float b, float a)
{
	CLAMP01(r); CLAMP01(g); CLAMP01(b); CLAMP01(a);
	currentVertex.color = D3DCOLOR_COLORVALUE(r, g, b, a);
}

//===========================================================================
// DG_Color4fv
//===========================================================================
void DG_Color4fv(float *data)
{
	float r = data[0], g = data[1], b = data[2], a = data[3];
	CLAMP01(r); CLAMP01(g); CLAMP01(b); CLAMP01(a);
	currentVertex.color = D3DCOLOR_COLORVALUE(r, g, b, a);
}

//===========================================================================
// DG_TexCoord2f
//===========================================================================
void DG_TexCoord2f(float s, float t)
{
	currentVertex.s = s;
	currentVertex.t = t;
	TransformTexCoord(&currentVertex);
}

//===========================================================================
// DG_TexCoord2fv
//===========================================================================
void DG_TexCoord2fv(float *data)
{
	currentVertex.s = data[0];
	currentVertex.t = data[1];
	TransformTexCoord(&currentVertex);
}

//===========================================================================
// DG_Vertex2f
//===========================================================================
void DG_Vertex2f(float x, float y)
{
	currentVertex.pos.x = x;
	currentVertex.pos.y = y;
	currentVertex.pos.z = 0;
	VtxToStack();
}

//===========================================================================
// DG_Vertex2fv
//===========================================================================
void DG_Vertex2fv(float *data)
{
	currentVertex.pos.x = data[0];
	currentVertex.pos.y = data[1];
	currentVertex.pos.z = 0;
	VtxToStack();
}

//===========================================================================
// DG_Vertex3f
//===========================================================================
void DG_Vertex3f(float x, float y, float z)
{
	currentVertex.pos.x = x;
	currentVertex.pos.y = y;
	currentVertex.pos.z = z;
	VtxToStack();
}

//===========================================================================
// DG_Vertex3fv
//===========================================================================
void DG_Vertex3fv(float *data)
{
	currentVertex.pos.x = data[0];
	currentVertex.pos.y = data[1];
	currentVertex.pos.z = data[2];
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
	for(; num > 0; num--, data++)
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
	for(; num > 0; num--, data++)
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
		if(inSequence) return;
		inSequence = true;
		dev->BeginScene();
		return;
	}

	// Begin the scene automatically.
	if(!inSequence) dev->BeginScene();

	primType = mode;
	primOrder = 0;
	primCount = 0;
	stackPos = 0;
}

//===========================================================================
// DG_End
//===========================================================================
void DG_End(void)
{
	if(!primType)
	{
		if(inSequence) dev->EndScene();
		inSequence = false;
		return;
	}

	// Upload any vertices remaining in the cache.
	if(stackPos) UploadStack();

	// Quads have already been drawn.
	DrawBuffers(primType == DGL_QUADS? D3DPT_TRIANGLELIST
		: primType == DGL_TRIANGLE_FAN? D3DPT_TRIANGLEFAN
		: primType == DGL_TRIANGLE_STRIP? D3DPT_TRIANGLESTRIP
		: primType == DGL_QUAD_STRIP? D3DPT_TRIANGLESTRIP
		: primType == DGL_LINES? D3DPT_LINELIST
		: primType == DGL_TRIANGLES? D3DPT_TRIANGLELIST		
		: /* DGL_POINTS */ D3DPT_POINTLIST);
	primType = 0;

	if(!inSequence) dev->EndScene();
}
