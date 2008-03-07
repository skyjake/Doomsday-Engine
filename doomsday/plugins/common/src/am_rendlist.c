/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
 * am_rendlist.c : Automap, rendering lists.
 */

// HEADER FILES ------------------------------------------------------------

#if __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "am_map.h"
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
    glprimtype_t    type; // DGL_QUAD or DGL_LINES
    amprim_t *head, *tail, *unused;
} amprimlist_t;

typedef struct amlist_s {
    amprimlist_t    primlist;
    uint            tex;
    boolean         texIsPatchLumpNum;
    blendmode_t     blend;
    float           arg1; // List-type specific arguments.
    struct amlist_s *next;
} amlist_t;

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

int numTexUnits;
boolean envModAdd; // TexEnv: modulate and add is available.

DGLuint amMaskTexture = 0; // Used to mask the map primitives.

//
// Rendering lists.
//
static amlist_t *amListsHead;

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
    // Does the graphics library support multitexturing?
    numTexUnits = DGL_GetInteger(DGL_MAX_TEXTURE_UNITS);
    envModAdd = (DGL_GetInteger(DGL_MODULATE_ADD_COMBINE)? true : false);
}

/**
 * Called once during final shutdown.
 */
void AM_ListShutdown(void)
{
    amlist_t *list, *listp;
    amprim_t   *n, *np;

    AM_ClearAllLists(true);

    // Empty the free node lists too.
    list = amListsHead;
    while(list)
    {
        listp = list->next;

        n = list->primlist.unused;
        while(n)
        {
            np = n->next;
            free(n);
            n = np;
        }
        list->primlist.unused = NULL;
        free(list);

        list = listp;
    }
}

void AM_BindTo(int unit, DGLuint texture)
{
    DGL_SetInteger(DGL_ACTIVE_TEXTURE, unit);
    DGL_Bind(texture);
}

/**
 * The first selected unit is active after this call.
 */
void AM_SelectTexUnits(int count)
{
    int     i;

    // Disable extra units.
    for(i = numTexUnits - 1; i >= count; i--)
        DGL_DisableTexUnit(0 + i);

    // Enable the selected units.
    for(i = count - 1; i >= 0; i--)
    {
        if(i >= numTexUnits)
            continue;

        DGL_EnableTexUnit(0 + i);
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
 * Create a new automap primitive and link it into the appropriate list.
 *
 * @param type      Type of primitive required (DGL_QUADS or DGL_LINES).
 * @param tex       DGLuint texture identifier/patch lump number for quad.
 * @param texIsPatchLumpNum @c true = 'tex' is treated as a patch lump
 *                  number.
 * @param blend     The blendmode required for this quad.
 *
 * @return          Ptr to the new automap render primitive.
 */
static void *AM_AllocatePrimitive(glprimtype_t type, uint tex, boolean texIsPatchLumpNum,
                                  blendmode_t blend, float arg1)
{
    amlist_t *list;
    amprim_t   *p;
    boolean     found;

    if(!(type == DGL_QUADS || type == DGL_LINES))
        Con_Error("AM_AllocatePrimitive: Unsupported primitive type %i.", type);

    // Find a suitable primitive list (matching texture & blend & args).
    list = amListsHead;
    found = false;
    while(list && !found)
    {
        if(list->primlist.type == type &&
           list->tex == tex && list->texIsPatchLumpNum == texIsPatchLumpNum &&
           list->blend == blend && list->arg1 == arg1)
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
        list->arg1 = arg1;
        list->primlist.type = type;
        list->primlist.head = list->primlist.tail =
            list->primlist.unused = NULL;

        // Link it in.
        list->next = amListsHead;
        amListsHead = list;
    }

    p = AM_NewPrimitive(&list->primlist);
    AM_LinkPrimitiveToList(&list->primlist, p);

    if(type == DGL_LINES)
        return &(p->data.line);
    else
        return &(p->data.quad);
}

/**
 * Empties or destroys all primitives in the given automap render list.
 *
 * @param list      Ptr to the list to be emptied/destroyed.
 * @param destroy   If @c true, all primitives in the list will be
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
 * @param destroy   If @c true, all primitives in each list will be
 *                  free'd, ELSE they'll be moved to each list's unused
 *                  store, ready for later re-use.
 */
void AM_ClearAllLists(boolean destroy)
{
    amlist_t *list;

    list = amListsHead;
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
void AM_AddLine(float x, float y, float x2, float y2, float width,
                int color, float alpha, blendmode_t blend)
{
    amrline_t *l;

    l = AM_AllocatePrimitive(DGL_LINES, 0, false, blend, width);

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
void AM_AddLine4f(float x, float y, float x2, float y2, float width,
                  float r, float g, float b, float a, blendmode_t blend)
{
    amrline_t *l;

    l = AM_AllocatePrimitive(DGL_LINES, 0, false, blend, width);

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
 * @param texIsPatchLumpNum  If @c true, 'tex' is a patch lump num.
 * @param blend     Blendmode used for this primitive.
 */
void AM_AddQuad(float x1, float y1, float x2, float y2,
                float x3, float y3, float x4, float y4,
                float tc1st1, float tc1st2,
                float tc2st1, float tc2st2,
                float tc3st1, float tc3st2,
                float tc4st1, float tc4st2,
                float r, float g, float b, float a,
                uint tex, boolean texIsPatchLumpNum, blendmode_t blend)
{
    // Vertex layout.
    // 4--3
    // | /|
    // |/ |
    // 1--2
    //
    amrquad_t *q;

    q = AM_AllocatePrimitive(DGL_QUADS, tex, texIsPatchLumpNum, blend, 0);

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
 * @param texIsPatchLumpNum   If @c true 'tex' is a patch lump num.
 * @param blend     All primitives on the list will be rendered with this
 *                  blending mode.
 * @param alpha     The alpha of primitives on the list will be rendered
 *                  using: (their alpha * alpha).
 * @param arg1      List-specific argment.
 * @param list      Ptr to the automap render list to be rendered.
 */
void AM_RenderList(uint tex, boolean texIsPatchLumpNum, blendmode_t blend,
                   float alpha, float arg1, amprimlist_t *list)
{
    amprim_t   *p;
    int         normal = 0, mask = 1;
    int         maskID = 1;
    float       oldLineWidth = 0;
    boolean     withMask = false, texMatrix = false;

    // Change render state for this list?

    // If multitexturing is available, all primitives will be rendered using
    // a modulation pass with the automap mask texture.
    if(numTexUnits > 1)
    {
        if(tex)
        {
            if(texIsPatchLumpNum)
            {   /**
                 * \fixme Can not modulate these primitives as we don't know
                 * the GL texture name (DGLuint).
                 * \fixme Need to store the wrap mode in the list.
                 */
                GL_SetPatch(tex, DGL_CLAMP, DGL_CLAMP);
            }
            else
            {
                AM_SelectTexUnits(2);
                DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);

                AM_BindTo(0, tex);
                AM_BindTo(1, amMaskTexture);
                withMask = true;
                texMatrix = true;
            }
        }
        else
        {
            DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);
            DGL_Bind(amMaskTexture);

            tex = amMaskTexture;
            maskID = 0;
            withMask = false;
            texMatrix = true;
        }

        DGL_Enable(DGL_TEXTURING);
    }
    else
    {
        if(tex)
        {
            if(texIsPatchLumpNum)
                //// \fixme Need to store the wrap mode in the list.
                GL_SetPatch(tex, DGL_CLAMP, DGL_CLAMP);
            else
                DGL_Bind(tex);

            DGL_Enable(DGL_TEXTURING);
        }
        else
        {
            DGL_Disable(DGL_TEXTURING);
        }
    }

    GL_BlendMode(blend);

    if(texMatrix)
    {
        float       x, y, w, h, angle;

        AM_GetWindow(mapviewplayer, &x, &y, &w, &h);
        angle = AM_ViewAngle(mapviewplayer);

        DGL_SetInteger(DGL_ACTIVE_TEXTURE, maskID);
        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PushMatrix();
        DGL_LoadIdentity();

        // Scale from texture to window space.
        DGL_Scalef(1.0f / w, 1.0f / h, 1);

        // Translate to the view origin, apply map rotation.
        DGL_Translatef(w / 2, h / 2, 0);
        DGL_Rotatef(angle, 0, 0, 1);
        DGL_Translatef(-(w / 2), -(h / 2), 0);

        // Undo the texture to window space translation.
        DGL_Translatef(-x, -y, 0);
    }

    // Write commands.
    p = list->head;

    // Need to adjust the line width?
    if(list->type == DGL_LINES)
    {
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, arg1);
    }

    if(withMask)
    {
        DGL_Begin(list->type);
        switch(list->type)
        {
        case DGL_QUADS:
            while(p)
            {
                DGL_Color4f(p->data.quad.rgba[0],
                           p->data.quad.rgba[1],
                           p->data.quad.rgba[2],
                           p->data.quad.rgba[3] * alpha);

                // V1
                if(tex)
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[0].tex[0],
                                   p->data.quad.verts[0].tex[1]);
                DGL_MultiTexCoord2f(mask,
                                   p->data.quad.verts[0].pos[0],
                                   p->data.quad.verts[0].pos[1]);

                DGL_Vertex2f(p->data.quad.verts[0].pos[0],
                            p->data.quad.verts[0].pos[1]);
                // V2
                if(tex)
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[1].tex[0],
                                   p->data.quad.verts[1].tex[1]);
                DGL_MultiTexCoord2f(mask,
                                   p->data.quad.verts[1].pos[0],
                                   p->data.quad.verts[1].pos[1]);

                DGL_Vertex2f(p->data.quad.verts[1].pos[0],
                            p->data.quad.verts[1].pos[1]);
                // V3
                if(tex)
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[2].tex[0],
                                   p->data.quad.verts[2].tex[1]);
                DGL_MultiTexCoord2f(mask,
                                   p->data.quad.verts[2].pos[0],
                                   p->data.quad.verts[2].pos[1]);

                DGL_Vertex2f(p->data.quad.verts[2].pos[0],
                            p->data.quad.verts[2].pos[1]);
                // V4
                if(tex)
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[3].tex[0],
                                   p->data.quad.verts[3].tex[1]);
                DGL_MultiTexCoord2f(mask,
                                   p->data.quad.verts[3].pos[0],
                                   p->data.quad.verts[3].pos[1]);

                DGL_Vertex2f(p->data.quad.verts[3].pos[0],
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
                    DGL_Color4f(p->data.line.coldata.f4color.rgba[0],
                               p->data.line.coldata.f4color.rgba[1],
                               p->data.line.coldata.f4color.rgba[2],
                               p->data.line.coldata.f4color.rgba[3] * alpha);
                }

                if(tex)
                    DGL_MultiTexCoord2f(normal, 0, 0);
                DGL_MultiTexCoord2f(mask, p->data.line.a.pos[0],
                                   p->data.line.a.pos[1]);
                DGL_Vertex2f(p->data.line.a.pos[0], p->data.line.a.pos[1]);

                if(tex)
                    DGL_MultiTexCoord2f(normal, 1, 1);
                DGL_MultiTexCoord2f(mask, p->data.line.b.pos[0],
                                   p->data.line.b.pos[1]);
                DGL_Vertex2f(p->data.line.b.pos[0], p->data.line.b.pos[1]);

                p = p->next;
            }
            break;

        default:
            break;
        }
        DGL_End();
    }
    else
    {
        DGL_Begin(list->type);
        switch(list->type)
        {
        case DGL_QUADS:
            while(p)
            {
                DGL_Color4f(p->data.quad.rgba[0],
                           p->data.quad.rgba[1],
                           p->data.quad.rgba[2],
                           p->data.quad.rgba[3] * alpha);
                // V1
                if(tex && maskID == 0)
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[0].pos[0],
                                   p->data.quad.verts[0].pos[1]);
                else
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[0].tex[0],
                                   p->data.quad.verts[0].tex[1]);

                DGL_Vertex2f(p->data.quad.verts[0].pos[0],
                            p->data.quad.verts[0].pos[1]);
                // V2
                if(tex && maskID == 0)
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[1].pos[0],
                                   p->data.quad.verts[1].pos[1]);
                else
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[1].tex[0],
                                   p->data.quad.verts[1].tex[1]);

                DGL_Vertex2f(p->data.quad.verts[1].pos[0],
                            p->data.quad.verts[1].pos[1]);
                // V3
                if(tex && maskID == 0)
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[2].pos[0],
                                   p->data.quad.verts[2].pos[1]);
                else
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[2].tex[0],
                                   p->data.quad.verts[2].tex[1]);

                DGL_Vertex2f(p->data.quad.verts[2].pos[0],
                            p->data.quad.verts[2].pos[1]);
                // V4
                if(tex && maskID == 0)
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[3].pos[0],
                                   p->data.quad.verts[3].pos[1]);
                else
                DGL_MultiTexCoord2f(normal,
                                   p->data.quad.verts[3].tex[0],
                                   p->data.quad.verts[3].tex[1]);

                DGL_Vertex2f(p->data.quad.verts[3].pos[0],
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
                    DGL_Color4f(p->data.line.coldata.f4color.rgba[0],
                               p->data.line.coldata.f4color.rgba[1],
                               p->data.line.coldata.f4color.rgba[2],
                               p->data.line.coldata.f4color.rgba[3] * alpha);
                }

                if(tex && maskID == 0)
                    DGL_MultiTexCoord2f(normal, p->data.line.a.pos[0], p->data.line.a.pos[1]);
                DGL_Vertex2f(p->data.line.a.pos[0], p->data.line.a.pos[1]);
                if(tex && maskID == 0)
                    DGL_MultiTexCoord2f(normal, p->data.line.b.pos[0], p->data.line.b.pos[1]);
                DGL_Vertex2f(p->data.line.b.pos[0], p->data.line.b.pos[1]);

                p = p->next;
            }
            break;

        default:
            break;
        }
        DGL_End();
    }

    // Restore previous line width.
    if(list->type == DGL_LINES)
        DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);

    // Restore previous state.
    if(texMatrix)
    {
        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PopMatrix();
    }

    AM_SelectTexUnits(1);
    DGL_SetInteger(DGL_MODULATE_TEXTURE, 1);
    if(!tex)
        DGL_Enable(DGL_TEXTURING);

    GL_BlendMode(BM_NORMAL);
}

/**
 * Render all primitives in all automap render lists.
 */
void AM_RenderAllLists(float alpha)
{
    amlist_t *list;

    list = amListsHead;
    while(list)
    {
        AM_RenderList(list->tex, list->texIsPatchLumpNum, list->blend,
                      alpha, list->arg1,
                      &list->primlist);
        list = list->next;
    }
}
