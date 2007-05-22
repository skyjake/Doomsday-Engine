/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2007 Daniel Swanson <danij@dengine.net>
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
 * am_rendlist.c : Automap, rendering lists.
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "am_rendlist.h"

// MACROS ------------------------------------------------------------------

#define AMLT_PALCOL     1
#define AMLT_RGBA       2

// TYPES -------------------------------------------------------------------

typedef struct amrline_s {
    byte    type;
    struct {
        float   pos[2];
    } a, b;
    union {
        struct {
            int     color;
            float   alpha;
        } palcolor;
        struct {
            float rgba[4];
        } f4color;
    } coldata;
} amrline_t;

typedef struct amrquad_s {
    float   rgba[4];
    struct {
        float   pos[2];
        float   tex[2];
    } verts[4];
} amrquad_t;

typedef struct amprim_s {
    union {
        amrquad_t quad;
        amrline_t line;
    } data;
    struct amprim_s *next;
} amprim_t;

typedef struct amprimlist_s {
    int         type; // DGL_QUAD or DGL_LINES
    amprim_t *head, *tail, *unused;
} amprimlist_t;

typedef struct amquadlist_s {
    uint        tex;
    boolean     texIsPatchLumpNum;
    boolean     blend;
    amprimlist_t primlist;
    struct amquadlist_s *next;
} amquadlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean freezeMapRLs = false;

cvar_t  automapListCVars[] = {
    {"rend-dev-freeze-map", CVF_NO_ARCHIVE, CVT_BYTE, &freezeMapRLs, 0, 1},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

//
// Rendering lists.
//
static amquadlist_t *amQuadListHead;

// Lines.
static amprimlist_t amLineList;

// CODE --------------------------------------------------------------------

/**
 * Register cvars and ccmds for the automap rendering lists.
 * Called during the PreInit of each game
 */
void AM_ListRegister(void)
{
    uint        i;

    for(i = 0; automapListCVars[i].name; ++i)
        Con_AddVariable(&automapListCVars[i]);
}

/**
 * Called once during first init.
 */
void AM_ListInit(void)
{
    amLineList.type = DGL_LINES;
    amLineList.head = amLineList.tail = amLineList.unused = NULL;
}

/**
 * Called once during final shutdown.
 */
void AM_ListShutdown(void)
{
    amquadlist_t *list, *listp;
    amprim_t   *n, *np;
    amprim_t   *nq, *npq;

    AM_ClearAllLists(true);

    // Empty the free node lists too.
    // Lines.
    n = amLineList.unused;
    while(n)
    {
        np = n->next;
        free(n);
        n = np;
    }
    amLineList.unused = NULL;

    // Quads.
    list = amQuadListHead;
    while(list)
    {
        listp = list->next;

        nq = list->primlist.unused;
        while(nq)
        {
            npq = nq->next;
            free(nq);
            nq = npq;
        }
        list->primlist.unused = NULL;
        free(list);

        list = listp;
    }
}

/**
 * Return a new automap render primitive.
 *
 * @param list      Ptr to an existing automap render list. If a list is
 *                  specified and it has at least one unused primitive, it
 *                  will be used. ELSE a new primitive is allocated.
 *
 * @return          Ptr to the new automap render primitive.
 */
static amprim_t *AM_NewPrimitive(amprimlist_t *list)
{
    amprim_t   *p;

    // Any primitives available in a passed primlist's, unused list?
    if(list && list->unused)
    {   // Yes, unlink from the unused list and use it.
        p = list->unused;
        list->unused = p->next;
    }
    else
    {   // No, allocate another.
        p = malloc(sizeof(*p));
    }

    return p;
}

/**
 * Link the given automap render primitive into the given list.
 *
 * @param list      Ptr to the automap render list to link the primitive to.
 * @param p         Ptr to the automap render primitive to be linked.
 */
static void AM_LinkPrimitiveToList(amprimlist_t *list, amprim_t *p)
{
    p->next = list->head;
    if(!list->tail)
        list->tail = p;
    list->head = p;
}

/**
 * Create a new automap line primitive and link it into the appropriate list.
 *
 * @return          Ptr to the new line, automap render primitive. 
 */
static amrline_t *AM_AllocateLine(void)
{
    amprimlist_t *list = &amLineList;
    amprim_t   *p = AM_NewPrimitive(list);

    AM_LinkPrimitiveToList(list, p);
    return &(p->data.line);
}

/**
 * Create a new automap quad primitive and link it into the appropriate list.
 *
 * @param tex       DGLuint texture identifier for quad.
 * @param blend     If <code>true</code> this quad will be added to a list
 *                  suitable for blended primitives.
 *
 * @return          Ptr to the new quad, automap render primitive. 
 */
static amrquad_t *AM_AllocateQuad(uint tex, boolean texIsPatchLumpNum,
                                  boolean blend)
{
    amquadlist_t *list;
    amprim_t   *p;
    boolean     found;

    // Find a suitable primitive list (matching texture & blend).
    list = amQuadListHead;
    found = false;
    while(list && !found)
    {
        if(list->tex == tex &&
           list->texIsPatchLumpNum == texIsPatchLumpNum &&
           list->blend == blend)
            found = true;
        else
            list = list->next;
    }

    if(!found)
    {   // Allocate a new list.
        list = malloc(sizeof(*list));

        list->tex = tex;
        list->texIsPatchLumpNum = texIsPatchLumpNum;
        list->blend = blend;
        list->primlist.type = DGL_QUADS;
        list->primlist.head = list->primlist.tail =
            list->primlist.unused = NULL;

        // Link it in.
        list->next = amQuadListHead;
        amQuadListHead = list;
    }

    p = AM_NewPrimitive(&list->primlist);
    AM_LinkPrimitiveToList(&list->primlist, p);

    return &(p->data.quad);
}

/**
 * Empties or destroys all primitives in the given automap render list.
 *
 * @param list      Ptr to the list to be emptied/destroyed.
 * @param destroy   If <code>true</code> all primitives in the list will be
 *                  free'd, ELSE they'll be moved to the list's unused
 *                  store, ready for later re-use.
 */
static void AM_DeleteList(amprimlist_t *list, boolean destroy)
{
    amprim_t *n, *np;

    // Are we destroying the lists?
    if(destroy)
    {   // Yes, free the nodes.
        n = list->head;
        while(n)
        {
            np = n->next;
            free(n);
            n = np;
        }
    }
    else
    {   // No, move all nodes to the free list.
        n = list->tail;
        if(list->tail)
        {
            if(list->unused)
                n->next = list->unused;
            list->unused = list->head;
        }
    }

    list->head = list->tail = NULL;
}

/**
 * Empties or destroys all primitives in ALL automap render lists.
 *
 * @param destroy   If <code>true</code> all primitives in each list will be
 *                  free'd, ELSE they'll be moved to each list's unused
 *                  store, ready for later re-use.
 */
void AM_ClearAllLists(boolean destroy)
{
    amquadlist_t *list;

    // Lines.
    AM_DeleteList(&amLineList, destroy);

    // Quads.
    list = amQuadListHead;
    while(list)
    {
        AM_DeleteList(&list->primlist, destroy);
        list = list->next;
    }
}

/**
 * Write a line to the automap render list, color specified by palette idx.
 *
 * @param x         X coordinate of the line origin.
 * @param y         Y coordinate of the line origin.
 * @param x2        X coordinate of the line destination.
 * @param y2        Y coordinate of the line destination.
 * @param color     Palette color idx.
 * @param alpha     Alpha value of the line (opacity).
 */
void AM_AddLine(float x, float y, float x2, float y2, int color,
                float alpha)
{
    amrline_t *l;

    l = AM_AllocateLine();

    l->a.pos[0] = x;
    l->a.pos[1] = y;
    l->b.pos[0] = x2;
    l->b.pos[1] = y2;

    l->type = AMLT_PALCOL;
    l->coldata.palcolor.color = color;
    l->coldata.palcolor.alpha = CLAMP(alpha, 0.0, 1.0f);
}

/**
 * Write a line to the automap render list, color specified by RGBA.
 *
 * @param x         X coordinate of the line origin.
 * @param y         Y coordinate of the line origin.
 * @param x2        X coordinate of the line destination.
 * @param y2        Y coordinate of the line destination.
 * @param r         Red color component of the line.
 * @param g         Green color component of the line.
 * @param b         Blue color component of the line.
 * @param a         Alpha value of the line (opacity).
 */
void AM_AddLine4f(float x, float y, float x2, float y2,
                  float r, float g, float b, float a)
{
    amrline_t *l;

    l = AM_AllocateLine();

    l->a.pos[0] = x;
    l->a.pos[1] = y;
    l->b.pos[0] = x2;
    l->b.pos[1] = y2;

    l->type = AMLT_RGBA;
    l->coldata.f4color.rgba[0] = r;
    l->coldata.f4color.rgba[1] = g;
    l->coldata.f4color.rgba[2] = b;
    l->coldata.f4color.rgba[3] = CLAMP(a, 0.0, 1.0f);
}

/**
 * Write a quad to the automap render list.
 *
 * @param x1        X coordinate of the top left vertex.
 * @param y1        Y coordinate of the top left vertex.
 * @param x2        X coordinate of the top right vertex.
 * @param y2        Y coordinate of the top right vertex.
 * @param x3        X coordinate of the bottom left vertex.
 * @param y3        Y coordinate of the bottom left vertex.
 * @param x4        X coordinate of the bottom right vertex.
 * @param y4        Y coordinate of the bottom right vertex.
 * @param tc1st1    X coordinate of the top left vertex texture offset.
 * @param tc1st2    Y coordinate of the top left vertex texture offset.
 * @param tc2st1    X coordinate of the top right vertex texture offset.
 * @param tc2st2    Y coordinate of the top right vertex texture offset.
 * @param tc3st1    X coordinate of the bottom left vertex texture offset.
 * @param tc3st2    Y coordinate of the bottom left vertex texture offset.
 * @param tc4st1    X coordinate of the bottom right vertex texture offset.
 * @param tc4st2    Y coordinate of the bottom right vertex texture offset.
 * @param r         Red color component of the line.
 * @param g         Green color component of the line.
 * @param b         Blue color component of the line.
 * @param a         Alpha value of the line (opacity).
 * @param tex       DGLuint texture identifier for quad OR patch lump num.
 * @param texIsPatchLumpNum  If <code>true</code> 'tex' is a patch lump num.
 * @param blend     If <code>true</code> this quad will be added to a list
 *                  suitable for additive blended primitives.       
 */
void AM_AddQuad(float x1, float y1, float x2, float y2,
                float x3, float y3, float x4, float y4,
                float tc1st1, float tc1st2,
                float tc2st1, float tc2st2,
                float tc3st1, float tc3st2,
                float tc4st1, float tc4st2,
                float r, float g, float b, float a,
                uint tex, boolean texIsPatchLumpNum, boolean blend)
{
    // Vertex layout.
    // 4--3
    // | /|
    // |/ |
    // 1--2
    //
    amrquad_t *q;

    q = AM_AllocateQuad(tex, texIsPatchLumpNum, blend);

    q->rgba[0] = r;
    q->rgba[1] = g;
    q->rgba[2] = b;
    q->rgba[3] = a;

    // V1
    q->verts[0].pos[0] = x1;
    q->verts[0].tex[0] = tc1st1;
    q->verts[0].pos[1] = y1;
    q->verts[0].tex[1] = tc1st2;

    // V2
    q->verts[1].pos[0] = x2;
    q->verts[1].tex[0] = tc2st1;
    q->verts[1].pos[1] = y2;
    q->verts[1].tex[1] = tc2st2;

    // V3
    q->verts[2].pos[0] = x3;
    q->verts[2].tex[0] = tc3st1;
    q->verts[2].pos[1] = y3;
    q->verts[2].tex[1] = tc3st2;

    // V4
    q->verts[3].pos[0] = x4;
    q->verts[3].tex[0] = tc4st1;
    q->verts[3].pos[1] = y4;
    q->verts[3].tex[1] = tc4st2;
}

/**
 * Render all primitives in the given list, using the specified texture and
 * blending mode.
 *
 * @param tex       DGLuint texture identifier for quads on the list.
 * @param texIsPatchLumpNum   If <code>true</code> 'tex' is a patch lump num.
 * @param blend     If <code>true</code> all primitives on the list will be
 *                  rendered with additive blending.
 * @param alpha     The alpha of primitives on the list will be rendered
 *                  using: (their alpha * alpha).
 * @param list      Ptr to the automap render list to be rendered.
 */
void AM_RenderList(uint tex, boolean texIsPatchLumpNum, boolean blend,
                   float alpha, amprimlist_t *list)
{
    amprim_t *p;

    // Change render state for this list?
    if(tex)
    {
        if(texIsPatchLumpNum)
            GL_SetPatch(tex);
        else
            gl.Bind(tex);

        gl.Enable(DGL_TEXTURING);
    }
    else
    {
        gl.Disable(DGL_TEXTURING);
    }

    if(blend)
    {
        gl.Func(DGL_BLENDING_OP, DGL_ADD, 0);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
    }

    // Write commands.
    p = list->head;
    gl.Begin(list->type);
    switch(list->type)
    {
    case DGL_QUADS:
        while(p)
        {
            gl.Color4f(p->data.quad.rgba[0],
                       p->data.quad.rgba[1],
                       p->data.quad.rgba[2],
                       p->data.quad.rgba[3] * alpha);

            // V1
            gl.TexCoord2f(p->data.quad.verts[0].tex[0],
                          p->data.quad.verts[0].tex[1]);
            gl.Vertex2f(p->data.quad.verts[0].pos[0],
                        p->data.quad.verts[0].pos[1]);
            // V2
            gl.TexCoord2f(p->data.quad.verts[1].tex[0],
                          p->data.quad.verts[1].tex[1]);
            gl.Vertex2f(p->data.quad.verts[1].pos[0],
                        p->data.quad.verts[1].pos[1]);
            // V3
            gl.TexCoord2f(p->data.quad.verts[2].tex[0],
                          p->data.quad.verts[2].tex[1]);
            gl.Vertex2f(p->data.quad.verts[2].pos[0],
                        p->data.quad.verts[2].pos[1]);
            // V4
            gl.TexCoord2f(p->data.quad.verts[3].tex[0],
                          p->data.quad.verts[3].tex[1]);
            gl.Vertex2f(p->data.quad.verts[3].pos[0],
                        p->data.quad.verts[3].pos[1]);

            p = p->next;
        }
        break;

    case DGL_LINES:
        while(p)
        {
            if(p->data.line.type == AMLT_PALCOL)
            {
                GL_SetColor2(p->data.line.coldata.palcolor.color,
                             p->data.line.coldata.palcolor.alpha * alpha);
            }
            else
            {
                gl.Color4f(p->data.line.coldata.f4color.rgba[0],
                           p->data.line.coldata.f4color.rgba[1],
                           p->data.line.coldata.f4color.rgba[2],
                           p->data.line.coldata.f4color.rgba[3] * alpha);
            }

            gl.Vertex2f(p->data.line.a.pos[0], p->data.line.a.pos[1]);
            gl.Vertex2f(p->data.line.b.pos[0], p->data.line.b.pos[1]);

            p = p->next;
        }
        break;

    default:
        break;
    }
    gl.End();

    // Restore previous state.
    if(!tex)
        gl.Enable(DGL_TEXTURING);
    if(blend)
    {
        gl.Func(DGL_BLENDING_OP, DGL_ADD, 0);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA_SATURATE, DGL_DST_ALPHA);
    }
}

/**
 * Render all primitives in all automap render lists.
 */
void AM_RenderAllLists(float alpha)
{
    amquadlist_t *list;

    // Quads.
    list = amQuadListHead;
    while(list)
    {
        AM_RenderList(list->tex, list->texIsPatchLumpNum, list->blend,
                      alpha,
                      &list->primlist);
        list = list->next;
    }

    // Lines.
    AM_RenderList(0, 0, false, alpha, &amLineList);

    gl.Enable(DGL_BLENDING);
    gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
}
