/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * draw.cpp: Drawing of Primitives
 */

// HEADER FILES ------------------------------------------------------------

#include "drd3d.h"

// MACROS ------------------------------------------------------------------

#define VERTICES_SIZE       32768
#define INDICES_SIZE        65536

#define MAX_ARRAYS  (2 + MAX_TEX_UNITS)

// TYPES -------------------------------------------------------------------

typedef struct array_s {
    boolean enabled;
    void *data;
} array_t;

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean     inSequence;
int         primType;
int         primOrder;

// The stack is used for reordering and caching vertices before copying
// them to a vertex buffer.
int         vertexPos;
drvertex_t  verticesStack[VERTICES_SIZE], currentVertex;

int         indexPos;
unsigned short indices[INDICES_SIZE];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static array_t arrays[MAX_ARRAYS];

// CODE --------------------------------------------------------------------

void InitDraw(void)
{
    inSequence = false;
    vertexPos = 0;
    indexPos = 0;
    memset(arrays, 0, sizeof(arrays));
}

/**
 * Used with the immediate mode drawing functions: Begin/End/etc.
 */
void VtxToBuffer(drvertex_t *vtx)
{
    if(vertexPos == VERTICES_SIZE) return; // Stop drawing...

    // Place a copy of the current vertex to the vertex buffer.
    memcpy(verticesStack + vertexPos, vtx, sizeof(*vtx));

    // Quads are translated to a triangle list.
    if(primType == DGL_QUADS)
    {
        if(++primOrder == 4)
        {
            primOrder = 0;

            // A quad is full. Add six indices.
            indices[indexPos++] = vertexPos - 3;
            indices[indexPos++] = vertexPos - 2;
            indices[indexPos++] = vertexPos - 1;

            indices[indexPos++] = vertexPos - 3;
            indices[indexPos++] = vertexPos - 1;
            indices[indexPos++] = vertexPos;
        }
    }
    else
    {
        // Add an index to match the added vertex.
        indices[indexPos++] = vertexPos;
    }

    vertexPos++;
}

void DG_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
    currentVertex.color = D3DCOLOR_XRGB(r, g, b);
}

void DG_Color3ubv(DGLubyte *data)
{
    currentVertex.color = D3DCOLOR_XRGB(data[0], data[1], data[2]);
}

void DG_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
    currentVertex.color = D3DCOLOR_RGBA(r, g, b, a);
}

void DG_Color4ubv(DGLubyte *data)
{
    currentVertex.color = D3DCOLOR_RGBA(data[0], data[1], data[2], data[3]);
}

void DG_Color3f(float r, float g, float b)
{
    CLAMP01(r); CLAMP01(g); CLAMP01(b);
    currentVertex.color = D3DCOLOR_COLORVALUE(r, g, b, 1);
}

void DG_Color3fv(float *data)
{
    float       r = data[0], g = data[1], b = data[2];

    CLAMP01(r); CLAMP01(g); CLAMP01(b);
    currentVertex.color = D3DCOLOR_COLORVALUE(r, g, b, 1);
}

void DG_Color4f(float r, float g, float b, float a)
{
    CLAMP01(r); CLAMP01(g); CLAMP01(b); CLAMP01(a);
    currentVertex.color = D3DCOLOR_COLORVALUE(r, g, b, a);
}

void DG_Color4fv(float *data)
{
    float       r = data[0], g = data[1], b = data[2], a = data[3];
    CLAMP01(r); CLAMP01(g); CLAMP01(b); CLAMP01(a);
    currentVertex.color = D3DCOLOR_COLORVALUE(r, g, b, a);
}

void DG_MultiTexCoord2f(int target, float s, float t)
{
    float      *st =
        (target == DGL_TEXTURE0? currentVertex.tex : currentVertex.tex2);

    st[0] = s;
    st[1] = t;
    TransformTexCoord(st);
}

void DG_MultiTexCoord2fv(int target, float *data)
{
    float      *st =
        (target == DGL_TEXTURE0? currentVertex.tex : currentVertex.tex2);

    st[0] = data[0];
    st[1] = data[1];
    TransformTexCoord(st);
}

void DG_TexCoord2f(float s, float t)
{
    DG_MultiTexCoord2f(DGL_TEXTURE0, s, t);
}

void DG_TexCoord2fv(float *data)
{
    DG_MultiTexCoord2fv(DGL_TEXTURE0, data);
}

void DG_Vertex2f(float x, float y)
{
    currentVertex.pos.x = x;
    currentVertex.pos.y = y;
    currentVertex.pos.z = 0;
    VtxToBuffer(&currentVertex);
}

void DG_Vertex2fv(float *data)
{
    currentVertex.pos.x = data[0];
    currentVertex.pos.y = data[1];
    currentVertex.pos.z = 0;
    VtxToBuffer(&currentVertex);
}

void DG_Vertex3f(float x, float y, float z)
{
    currentVertex.pos.x = x;
    currentVertex.pos.y = y;
    currentVertex.pos.z = z;
    VtxToBuffer(&currentVertex);
}

void DG_Vertex3fv(float *data)
{
    currentVertex.pos.x = data[0];
    currentVertex.pos.y = data[1];
    currentVertex.pos.z = data[2];
    VtxToBuffer(&currentVertex);
}

void DG_Vertices2ftv(int num, gl_ft2vertex_t *data)
{
    for(; num > 0; num--, data++)
    {
        DG_TexCoord2fv(data->tex);
        DG_Vertex2fv(data->pos);
    }
}

void DG_Vertices3ftv(int num, gl_ft3vertex_t *data)
{
    for(; num > 0; num--, data++)
    {
        DG_TexCoord2fv(data->tex);
        DG_Vertex3fv(data->pos);
    }
}

void DG_Vertices3fctv(int num, gl_fct3vertex_t *data)
{
    for(; num > 0; num--, data++)
    {
        DG_Color4fv(data->color);
        DG_TexCoord2fv(data->tex);
        DG_Vertex3fv(data->pos);
    }
}

void DG_Begin(int mode)
{
    if(mode == DGL_SEQUENCE)
    {
        if(inSequence) return;
        inSequence = true;
        if(FAILED(hr = dev->BeginScene()))
        {
#ifdef _DEBUG
            DXError("BeginScene");
            Con_Error("BeginScene Failed\n");
#endif
        }
        return;
    }

    // Begin the scene automatically.
    if(!inSequence)
    {
        if(FAILED(dev->BeginScene()))
        {
#ifdef _DEBUG
            DXError("BeginScene");
            Con_Error("BeginScene Failed\n");
#endif
        }
    }

    primType = mode;
    indexPos = 0;
    primOrder = 0;
    vertexPos = 0;
}

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

void DG_End(void)
{
    if(!primType)
    {
        if(inSequence) dev->EndScene();
        inSequence = false;
        return;
    }

    // We're using the fixed-function shader.
    /* dev->SetVertexShader(DRVTX_FORMAT); */

    // Select the D3D primitive type.
    D3DPRIMITIVETYPE type =
        ( primType == DGL_QUADS?            D3DPT_TRIANGLELIST
        : primType == DGL_TRIANGLE_FAN?     D3DPT_TRIANGLEFAN
        : primType == DGL_TRIANGLE_STRIP?   D3DPT_TRIANGLESTRIP
        : primType == DGL_QUAD_STRIP?       D3DPT_TRIANGLESTRIP
        : primType == DGL_LINES?            D3DPT_LINELIST
        : primType == DGL_TRIANGLES?        D3DPT_TRIANGLELIST
        : /* DGL_POINTS? */                 D3DPT_POINTLIST );

    dev->DrawIndexedPrimitiveUP(
        type,                       // PrimitiveType
        0,                          // MinIndex
        vertexPos,                  // NumVertices
        PrimCount(type, indexPos),  // PrimitiveCount
        indices,
        D3DFMT_INDEX16,
        verticesStack,
        sizeof(drvertex_t));

    primType = 0;

    if(!inSequence) dev->EndScene();
}

void DG_EnableArrays(int vertices, int colors, int coords)
{
    if(vertices)
        arrays[AR_VERTEX].enabled = true;
    if(colors)
        arrays[AR_COLOR].enabled = true;

    for(int i = 0; i < MAX_TEX_UNITS; i++)
    {
        if(coords & (1 << i))
            arrays[AR_TEXCOORD0 + i].enabled = true;
    }
}

void DG_DisableArrays(int vertices, int colors, int coords)
{
    if(vertices)
        arrays[AR_VERTEX].enabled = false;
    if(colors)
        arrays[AR_COLOR].enabled = false;

    for(int i = 0; i < MAX_TEX_UNITS; ++i)
    {
        if(coords & (1 << i))
            arrays[AR_TEXCOORD0 + i].enabled = false;
    }
}

/**
 * Enable, set and optionally lock all enabled arrays.
 */
void DG_Arrays(void *vertices, void *colors, int numCoords,
               void **coords, int lock)
{
    if(vertices)
    {
        arrays[AR_VERTEX].enabled = true;
        arrays[AR_VERTEX].data = vertices;
    }

    if(colors)
    {
        arrays[AR_COLOR].enabled = true;
        arrays[AR_COLOR].data = colors;
    }

    for(int i = 0; i < numCoords && i < MAX_TEX_UNITS; ++i)
    {
        if(coords[i])
        {
            arrays[AR_TEXCOORD0 + i].enabled = true;
            arrays[AR_TEXCOORD0 + i].data = coords[i];
        }
    }
}

void DG_UnlockArrays(void)
{
    // No need to lock anything.
}

void DG_ArrayElement(int index)
{
    for(int i = 0; i < MAX_TEX_UNITS; ++i)
    {
        if(arrays[AR_TEXCOORD0 + i].enabled)
        {
            DG_MultiTexCoord2fv(DGL_TEXTURE0 + i, ((gl_texcoord_t*)
                arrays[AR_TEXCOORD0 + i].data)[index].st);
        }
    }

    if(arrays[AR_COLOR].enabled)
        DG_Color4ubv(((gl_color_t*)arrays[AR_COLOR].data)[index].rgba);

    if(arrays[AR_VERTEX].enabled)
        DG_Vertex3fv(((gl_vertex_t*)arrays[AR_VERTEX].data)[index].xyz);
}

void DG_DrawElements(int type, int count, unsigned int *indices)
{
    DG_Begin(type);

    for(int i = 0; i < count; ++i)
    {
        DG_ArrayElement(indices[i]);
    }
    DG_End();
}
