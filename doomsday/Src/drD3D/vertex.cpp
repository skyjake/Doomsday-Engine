
//**************************************************************************
//**
//** VERTEX.CPP
//**
//** Target:		DGL Driver for Direct3D 8.1
//** Description:	The vertex buffers
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "drD3D.h"

// MACROS ------------------------------------------------------------------

#define MAX_BUFFERS			2
#define VERTICES_PER_BUFFER	5100
#define BUFFER_SIZE			((int)(sizeof(drvertex_t)*VERTICES_PER_BUFFER))

#define MAX_INDICES			4096
#define IDXBUFFER_SIZE		(2 * MAX_INDICES)

// TYPES -------------------------------------------------------------------

typedef struct vtxbuffer_s {
	IDirect3DVertexBuffer8 *vb;
	bool hasData;
} vtxbuffer_t;

typedef struct idxbuffer_s {
	IDirect3DIndexBuffer8 *ib;
	bool hasData;
} idxbuffer_t;

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int			triCounter = 0;
int			vtxCursor;		// Offset in *bytes*.
int			vbIndex;
int			idxCursor;		// Offset in *bytes*.

boolean		skipDraw = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static vtxbuffer_t vbuf[MAX_BUFFERS];
static idxbuffer_t ibuf;	

// CODE --------------------------------------------------------------------

//===========================================================================
// CreateBuffer
//===========================================================================
IDirect3DVertexBuffer8 *CreateBuffer(void)
{
	IDirect3DVertexBuffer8 *vb = NULL;

	dev->CreateVertexBuffer(BUFFER_SIZE, 
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, DRVERTEX_FORMAT,
		D3DPOOL_DEFAULT, &vb);
	return vb;
}

//===========================================================================
// InitVertexBuffers
//===========================================================================
void InitVertexBuffers(void)
{
	vtxCursor = 0;
	vbIndex = 0;
	idxCursor = 0;

	// Clear the buffers and create the first one.
	memset(vbuf, 0, sizeof(vbuf));
	vbuf[0].vb = CreateBuffer();

	// Create the vertex buffer (currently used only with quad lists).
	memset(&ibuf, 0, sizeof(ibuf));
	dev->CreateIndexBuffer(IDXBUFFER_SIZE,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
		D3DPOOL_DEFAULT, &ibuf.ib);
	dev->SetIndices(ibuf.ib, 0);
}

//===========================================================================
// ShutdownVertexBuffers
//===========================================================================
void ShutdownVertexBuffers(void)
{
	if(dev) dev->SetIndices(NULL, 0);
	for(int i = 0; i < MAX_BUFFERS; i++)
		if(vbuf[i].vb)
		{
			vbuf[i].vb->Release();
			vbuf[i].vb = NULL;
		}

	if(ibuf.ib) ibuf.ib->Release();
	ibuf.ib = NULL;
}

//===========================================================================
// BufferVertices
//	Copies the given vertices into the vertex buffer. It's better to call
//	this with as large a number of vertices as possible.
//===========================================================================
void BufferVertices(drvertex_t *verts, int num)
{
	int needMemory, writeBytes;
	BYTE *ptr, *src = (BYTE*) verts;
	vtxbuffer_t *buf = &vbuf[vbIndex];

	// If there is no valid vertex buffer, something's not right...
	if(!buf->vb) return;

	// Copy chunks until all is gone or we run out of buffers.
	needMemory = sizeof(*verts) * num;
	while(needMemory > 0)
	{
		if(BUFFER_SIZE - vtxCursor >= needMemory)
		{
			// Good, there's room; just copy all of it.		
			writeBytes = needMemory;
		}
		else
		{
			// Fill until the end of the buffer.
			writeBytes = BUFFER_SIZE - vtxCursor;
		}
		if(!writeBytes) return; // This is not a valid operation.
		
		// Lock and copy.		
		if(FAILED(buf->vb->Lock(vtxCursor, writeBytes,
			&ptr, buf->hasData? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD)))
			return; // Argh, terrible failure...
		memcpy(ptr, src, writeBytes);
		buf->vb->Unlock();
			
		// Advance cursor.
		vtxCursor += writeBytes;
		src += writeBytes;
		needMemory -= writeBytes;
		buf->hasData = true;
		
		// Is the cursor at the end of a buffer?
		if(vtxCursor == BUFFER_SIZE)
		{
			// Begin the next buffer.
			if(vbIndex == MAX_BUFFERS-1)
			{
				// No more buffers! Must stop here.
				return; 
			}
			buf = &vbuf[++vbIndex];
			if(!buf->vb) buf->vb = CreateBuffer();
			vtxCursor = 0;
		}
	}
}

//===========================================================================
// BufferIndices
//===========================================================================
void BufferIndices(unsigned short *indices, int num)
{
	int writeMemory = 2 * num;
	idxbuffer_t *buf = &ibuf;
	BYTE *ptr;

	if(IDXBUFFER_SIZE - idxCursor < writeMemory)
	{
		// There isn't enough memory! Overflowing...
		num = (IDXBUFFER_SIZE - idxCursor)/2;
		if(num <= 0) return;
		writeMemory = 2 * num;
	}

	// Lock and copy.
	if(FAILED(buf->ib->Lock(idxCursor, writeMemory, &ptr, 
		buf->hasData? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD)))
		return; // A miserable failure...
	memcpy(ptr, indices, writeMemory);
	buf->ib->Unlock();
	buf->hasData = true;
	idxCursor += writeMemory;
}

//===========================================================================
// PrimCount
//===========================================================================
int PrimCount(D3DPRIMITIVETYPE type, int verts)
{
	switch(type)
	{
	case D3DPT_POINTLIST:
		return verts;

	case D3DPT_LINELIST:
		return verts/2;

	case D3DPT_TRIANGLELIST:
		return verts/3;

	case D3DPT_TRIANGLESTRIP:
		return verts - 2;

	case D3DPT_TRIANGLEFAN:
		return verts - 2;
	}
	return 0;
}

//===========================================================================
// DrawBuffers
//	Draw all vertices in the buffers. After everything has been drawn, the
//	vertex cursor is reset back to the beginning and all buffers are 
//	flagged empty.
//===========================================================================
void DrawBuffers(D3DPRIMITIVETYPE primType)
{
	int i, numPrims;

	// Draw all buffers with data.
	for(i = 0; i <= vbIndex; i++)
	{
		if(!vbuf[i].vb || !vbuf[i].hasData) break; // No more data.
		dev->SetStreamSource(0, vbuf[i].vb, DRVSIZE);
		if(ibuf.hasData)
		{
			dev->DrawIndexedPrimitive(primType, 0, idxCursor/2,
				0, numPrims = PrimCount(primType, idxCursor/2));
		}
		else
		{
			dev->DrawPrimitive(primType, 0, numPrims = PrimCount(primType, 
				i < vbIndex? VERTICES_PER_BUFFER : vtxCursor/DRVSIZE));
		}
		triCounter += numPrims;
	}
	dev->SetStreamSource(0, NULL, 0);
	
	// Reset the buffers.
	EmptyBuffers();
}

//===========================================================================
// EmptyBuffers
//===========================================================================
void EmptyBuffers(void)
{
	for(int i = 0; i <= vbIndex; i++)
	{
		if(!vbuf[i].vb) break;
		vbuf[i].hasData = false; // Empty it.
	}
	vbIndex = 0;
	vtxCursor = 0;

	ibuf.hasData = false;
	idxCursor = 0;
}