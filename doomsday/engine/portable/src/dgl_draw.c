/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAX_ARRAYS  (2 + MAX_TEX_UNITS)

// TYPES -------------------------------------------------------------------

typedef struct array_s {
    boolean         enabled;
    void*           data;
} array_t;

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int polyCounter; // Triangle counter, really.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static array_t arrays[MAX_ARRAYS];

static int primLevel = 0;
static DGLuint inList = 0;
#ifdef _DEBUG
static boolean inPrim = false;
#endif

// CODE --------------------------------------------------------------------

void GL_InitArrays(void)
{
    double              version =
        strtod((const char*) glGetString(GL_VERSION), NULL);

    // If the driver's OpenGL version is older than 1.3, disable arrays
    // by default.
    GL_state.noArrays = (version < 1.3);

    // Override the automatic selection?
    if(ArgExists("-vtxar"))
        GL_state.noArrays = false;
    if(ArgExists("-novtxar"))
        GL_state.noArrays = true;

    if(!GL_state.noArrays)
        return;
    memset(arrays, 0, sizeof(arrays));
}

boolean GL_NewList(DGLuint list, int mode)
{
    // We enter a New/End list section.
#ifdef _DEBUG
if(inList)
    Con_Error("OpenGL: already inList");
Sys_CheckGLError();
#endif

    if(list)
    {   // A specific list id was requested. Is it free?
        if(glIsList(list))
        {
#if _DEBUG
Con_Error("OpenGL: List %u already in use.", (unsigned int) list);
#endif
            return false;
        }
    }
    else
    {   // Just get a new list id, it doesn't matter.
        list = glGenLists(1);
    }

    glNewList(list, mode == DGL_COMPILE? GL_COMPILE : GL_COMPILE_AND_EXECUTE);
    inList = list;
    return true;
}

DGLuint GL_EndList(void)
{
    DGLuint             currentList = inList;

    glEndList();
#ifdef _DEBUG
    inList = 0;
    Sys_CheckGLError();
#endif

    return currentList;
}

void GL_CallList(DGLuint list)
{
    if(!list)
        return; // We do not consider zero a valid list id.

    glCallList(list);
}

void GL_DeleteLists(DGLuint list, int range)
{
    glDeleteLists(list, range);
}

void GL_EnableArrays(int vertices, int colors, int coords)
{
    int                 i;

    if(vertices)
    {
        if(GL_state.noArrays)
            arrays[AR_VERTEX].enabled = true;
        else
            glEnableClientState(GL_VERTEX_ARRAY);
    }

    if(colors)
    {
        if(GL_state.noArrays)
            arrays[AR_COLOR].enabled = true;
        else
            glEnableClientState(GL_COLOR_ARRAY);
    }

    for(i = 0; i < GL_state.maxTexUnits && i < MAX_TEX_UNITS; i++)
    {
        if(coords & (1 << i))
        {
            if(GL_state.noArrays)
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
    Sys_CheckGLError();
#endif
}

void GL_DisableArrays(int vertices, int colors, int coords)
{
    int         i;

    if(vertices)
    {
        if(GL_state.noArrays)
            arrays[AR_VERTEX].enabled = false;
        else
            glDisableClientState(GL_VERTEX_ARRAY);
    }

    if(colors)
    {
        if(GL_state.noArrays)
            arrays[AR_COLOR].enabled = false;
        else
            glDisableClientState(GL_COLOR_ARRAY);
    }

    for(i = 0; i < GL_state.maxTexUnits && i < MAX_TEX_UNITS; i++)
    {
        if(coords & (1 << i))
        {
            if(GL_state.noArrays)
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
    Sys_CheckGLError();
#endif
}

/**
 * Enable, set and optionally lock all enabled arrays.
 */
void GL_Arrays(void *vertices, void *colors, int numCoords, void **coords,
                int lock)
{
    int         i;

    if(vertices)
    {
        if(GL_state.noArrays)
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
        if(GL_state.noArrays)
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
            if(GL_state.noArrays)
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
        if(!GL_state.noArrays && lock > 0)
        {   // 'lock' is the number of vertices to lock.
            glLockArraysEXT(0, lock);
        }

#ifdef _DEBUG
    Sys_CheckGLError();
#endif
}

void GL_UnlockArrays(void)
{
    if(!GL_state.noArrays)
    {
#ifndef UNIX
        if(glUnlockArraysEXT)
#endif
            glUnlockArraysEXT();
    }

#ifdef _DEBUG
    Sys_CheckGLError();
#endif
}

void GL_ArrayElement(int index)
{
    if(!GL_state.noArrays)
    {
        glArrayElement(index);
    }
    else
    {
        int         i;

        for(i = 0; i < GL_state.maxTexUnits && i < MAX_TEX_UNITS; ++i)
        {
            if(arrays[AR_TEXCOORD0 + i].enabled)
            {
                glMultiTexCoord2fvARB(GL_TEXTURE0 + i,
                                      ((dgl_texcoord_t *)
                                       arrays[AR_TEXCOORD0 +
                                              i].data)[index].st);
            }
        }

        if(arrays[AR_COLOR].enabled)
            glColor4ubv(((dgl_color_t *) arrays[AR_COLOR].data)[index].rgba);

        if(arrays[AR_VERTEX].enabled)
            glVertex3fv(((dgl_vertex_t *) arrays[AR_VERTEX].data)[index].xyz);
    }
}

void GL_DrawElements(dglprimtype_t type, int count, const uint *indices)
{
    GLenum          primType =
        (type == DGL_TRIANGLE_FAN ? GL_TRIANGLE_FAN : type ==
         DGL_TRIANGLE_STRIP ? GL_TRIANGLE_STRIP : GL_TRIANGLES);

    if(!GL_state.noArrays)
    {
        glDrawElements(primType, count, GL_UNSIGNED_INT, indices);
    }
    else
    {
        int         i;

        glBegin(primType);
        for(i = 0; i < count; ++i)
        {
            GL_ArrayElement(indices[i]);
        }
        glEnd();
    }

#ifdef _DEBUG
    Sys_CheckGLError();
#endif
}

void DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
    glColor3ub(r, g, b);
}

void DGL_Color3ubv(const DGLubyte* vec)
{
    glColor3ubv(vec);
}

void DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
    glColor4ub(r, g, b, a);
}

void DGL_Color4ubv(const DGLubyte* vec)
{
    glColor4ubv(vec);
}

void DGL_Color3f(float r, float g, float b)
{
    glColor3f(r, g, b);
}

void DGL_Color3fv(const float* vec)
{
    glColor3fv(vec);
}

void DGL_Color4f(float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
}

void DGL_Color4fv(const float* vec)
{
    glColor4fv(vec);
}

void DGL_TexCoord2f(byte target, float s, float t)
{
    if(target == 0)
        glTexCoord2f(s, t);
    else
        glMultiTexCoord2fARB(GL_TEXTURE0 + target, s, t);
}

void DGL_TexCoord2fv(byte target, float* vec)
{
    if(target == 0)
        glTexCoord2fv(vec);
    else
        glMultiTexCoord2fvARB(GL_TEXTURE0 + target, vec);
}

void DGL_Vertex2f(float x, float y)
{
    glVertex2f(x, y);
}

void DGL_Vertex2fv(const float* vec)
{
    glVertex2fv(vec);
}

void DGL_Vertex3f(float x, float y, float z)
{
    glVertex3f(x, y, z);
}

void DGL_Vertex3fv(const float* vec)
{
    glVertex3fv(vec);
}

void DGL_Vertices2ftv(int num, const dgl_ft2vertex_t* vec)
{
    for(; num > 0; num--, vec++)
    {
        glTexCoord2fv(vec->tex);
        glVertex2fv(vec->pos);
    }
}

void DGL_Vertices3ftv(int num, const dgl_ft3vertex_t* vec)
{
    for(; num > 0; num--, vec++)
    {
        glTexCoord2fv(vec->tex);
        glVertex3fv(vec->pos);
    }
}

void DGL_Vertices3fctv(int num, const dgl_fct3vertex_t* vec)
{
    for(; num > 0; num--, vec++)
    {
        glColor4fv(vec->color);
        glTexCoord2fv(vec->tex);
        glVertex3fv(vec->pos);
    }
}

void DGL_Begin(dglprimtype_t mode)
{
    // We enter a Begin/End section.
    primLevel++;

#ifdef _DEBUG
    if(inPrim)
        Con_Error("OpenGL: already inPrim");
    inPrim = true;
    Sys_CheckGLError();
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
    Sys_CheckGLError();
#endif
}

boolean DGL_NewList(DGLuint list, int mode)
{
    return GL_NewList(list, mode);
}

DGLuint DGL_EndList(void)
{
    return GL_EndList();
}

void DGL_CallList(DGLuint list)
{
    GL_CallList(list);
}

void DGL_DeleteLists(DGLuint list, int range)
{
    GL_DeleteLists(list, range);
}

void DGL_DrawLine(float x1, float y1, float x2, float y2, float r, float g,
                  float b, float a)
{
    GL_DrawLine(x1, y1, x2, y2, r, g, b, a);
}

void DGL_DrawRect(float x, float y, float w, float h, float r, float g,
                  float b, float a)
{
    GL_DrawRect(x, y, w, h, r, g, b, a);
}

void DGL_DrawRectTiled(float x, float y, float w, float h, int tw, int th)
{
    // Make sure the current texture will be tiled.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GL_DrawRectTiled(x, y, w, h, tw, th);
}

void DGL_DrawCutRectTiled(float x, float y, float w, float h, int tw, int th,
                          int txoff, int tyoff, float cx, float cy, float cw,
                          float ch)
{
    GL_DrawCutRectTiled(x, y, w, h, tw, th, txoff, tyoff, cx, cy, cw, ch);
}
