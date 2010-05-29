/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008-2010 Daniel Swanson <danij@dengine.net>
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
 * r_vectorgraphic.c: Vector graphics.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <assert.h>

#include "doomsday.h"
#include "r_vectorgraphic.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_SCALE               (0)
#define DEFAULT_ANGLE               (0)

// TYPES -------------------------------------------------------------------

enum { VX, VY, VZ }; // Vertex indices.

typedef struct vectorgraphic_s {
    vectorgraphicid_t id;
    DGLuint dlist;
    size_t count;
    struct vgline_s* lines;
} vectorgraphic_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;
static uint numVectorGraphics;
static vectorgraphic_t* vectorGraphics;

// CODE --------------------------------------------------------------------

static vectorgraphic_t* vectorGraphicForId(vectorgraphicid_t id)
{
    if(numVectorGraphics && id != 0)
    {
        uint i;
        for(i = 0; i < numVectorGraphics; ++i)
        {
            vectorgraphic_t* vg = &vectorGraphics[i];
            if(vg->id == id)
                return vg;
        }
    }
    return 0;
}

static void draw(const vectorgraphic_t* vg)
{
    uint i;
    DGL_Begin(DGL_LINES);
    for(i = 0; i < vg->count; ++i)
    {
        vgline_t* l = &vg->lines[i];
        DGL_TexCoord2f(0, l->a.pos[VX], l->a.pos[VY]);
        DGL_Vertex2f(l->a.pos[VX], l->a.pos[VY]);
        DGL_TexCoord2f(0, l->b.pos[VX], l->b.pos[VY]);
        DGL_Vertex2f(l->b.pos[VX], l->b.pos[VY]);
    }
    DGL_End();
}

static DGLuint constructDisplayList(DGLuint name, const vectorgraphic_t* vg)
{
    if(DGL_NewList(name, DGL_COMPILE))
    {
        draw(vg);
        return DGL_EndList();
    }
    return 0;
}

void R_InitVectorGraphics(void)
{
    if(inited)
        return;
    numVectorGraphics = 0;
    vectorGraphics = 0;
    inited = true;
}

void R_ShutdownVectorGraphics(void)
{
    if(!inited)
        return;

    if(numVectorGraphics != 0)
    {
        uint i;
        for(i = 0; i < numVectorGraphics; ++i)
        {
            vectorgraphic_t* vg = &vectorGraphics[i];
            if(!vg)
                continue;
            if(!(DD_GetInteger(DD_NOVIDEO) || DD_GetInteger(DD_DEDICATED)))
            {
                if(vg->dlist)
                    DGL_DeleteLists(vg->dlist, 1);
            }
            free(vg->lines);
        }
        free(vectorGraphics);
        vectorGraphics = 0;
        numVectorGraphics = 0;
    }
    inited = false;
}

/**
 * Unload any resources needed for vector graphics.
 * Called during shutdown and before a renderer restart.
 */
void R_UnloadVectorGraphics(void)
{
    if(!inited)
        return;
    if(DD_GetInteger(DD_NOVIDEO) || DD_GetInteger(DD_DEDICATED))
        return; // Nothing to do.

    {uint i;
    for(i = 0; i < numVectorGraphics; ++i)
    {
        vectorgraphic_t* vg = &vectorGraphics[i];
        if(vg->dlist)
            DGL_DeleteLists(vg->dlist, 1);
        vg->dlist = 0;
    }}
}

static vectorgraphic_t* prepareVectorGraphic(vectorgraphicid_t vgId)
{
    vectorgraphic_t* vg;
    if((vg = vectorGraphicForId(vgId)))
    {
        if(!vg->dlist)
        {
            vg->dlist = constructDisplayList(0, vg);
        }
        return vg;
    }
    Con_Message("prepareVectorGraphic: Warning, no vectorgraphic is known by id %i.", (int) vgId);
    return NULL;
}

void GL_DrawVectorGraphic3(vectorgraphicid_t vgId, float x, float y, float scale, float angle)
{
    vectorgraphic_t* vg;
    
    if((vg = prepareVectorGraphic(vgId)))
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(x, y, 0);
        if(angle != 0 || scale != 0)
        {
            DGL_PushMatrix();
            DGL_Rotatef(angle, 0, 0, 1);
            DGL_Scalef(scale, scale, 1);
        }

        if(vg->dlist)
        {   // We have a display list available; call it and get out of here.
            DGL_CallList(vg->dlist);
        }
        else
        {   // No display list available. Lets draw it manually.
            draw(vg);
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        if(angle != 0 || scale != 0)
            DGL_PopMatrix();
        DGL_Translatef(-x, -y, 0);
    }
}

void GL_DrawVectorGraphic2(vectorgraphicid_t vgId, float x, float y, float scale)
{
    GL_DrawVectorGraphic3(vgId, x, y, scale, DEFAULT_ANGLE);
}

void GL_DrawVectorGraphic(vectorgraphicid_t vgId, float x, float y)
{
    GL_DrawVectorGraphic2(vgId, x, y, DEFAULT_SCALE);
}

void R_NewVectorGraphic(vectorgraphicid_t vgId, const vgline_t* lines, size_t numLines)
{
    vectorgraphic_t* vg;
    size_t i;

    // Valid id?
    if(vgId == 0)
        Con_Error("R_NewVectorGraphic: Invalid id, zero is reserved.");

    // Already a vector graphic with this id?
    if(vectorGraphicForId(vgId))
        Con_Error("R_NewVectorGraphic: A vector graphic with id %i already exists.", (int) vgId);

    // Not loaded yet.
    vectorGraphics = realloc(vectorGraphics, sizeof(*vectorGraphics) * ++numVectorGraphics);
    vg = &vectorGraphics[numVectorGraphics-1];
    vg->id = vgId;
    vg->dlist = 0;
    vg->count = numLines;
    vg->lines = malloc(numLines * sizeof(vgline_t));
    for(i = 0; i < numLines; ++i)
    {
        memcpy(&vg->lines[i], &lines[i], sizeof(vgline_t));
    }
}
