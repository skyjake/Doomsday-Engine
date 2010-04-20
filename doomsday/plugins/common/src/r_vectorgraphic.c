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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "r_vectorgraphic.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    float pos[3];
} mpoint_t;

typedef struct vgline_s {
    mpoint_t a, b;
} vgline_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#define R (1.0f)

vgline_t keysquare[] = {
    {{0, 0}, {R / 4, -R / 2}},
    {{R / 4, -R / 2}, {R / 2, -R / 2}},
    {{R / 2, -R / 2}, {R / 2, R / 2}},
    {{R / 2, R / 2}, {R / 4, R / 2}},
    {{R / 4, R / 2}, {0, 0}}, // Handle part type thing.
    {{0, 0}, {-R, 0}}, // Stem.
    {{-R, 0}, {-R, -R / 2}}, // End lockpick part.
    {{-3 * R / 4, 0}, {-3 * R / 4, -R / 4}}
};

vgline_t thintriangle_guy[] = {
    {{-R / 2, R - R / 2}, {R, 0}}, // >
    {{R, 0}, {-R / 2, -R + R / 2}},
    {{-R / 2, -R + R / 2}, {-R / 2, R - R / 2}} // |>
};

#if __JDOOM__ || __JDOOM64__
vgline_t player_arrow[] = {
    {{-R + R / 8, 0}, {R, 0}}, // -----
    {{R, 0}, {R - R / 2, R / 4}}, // ----->
    {{R, 0}, {R - R / 2, -R / 4}},
    {{-R + R / 8, 0}, {-R - R / 8, R / 4}}, // >---->
    {{-R + R / 8, 0}, {-R - R / 8, -R / 4}},
    {{-R + 3 * R / 8, 0}, {-R + R / 8, R / 4}}, // >>--->
    {{-R + 3 * R / 8, 0}, {-R + R / 8, -R / 4}}
};

vgline_t cheat_player_arrow[] = {
    {{-R + R / 8, 0}, {R, 0}}, // -----
    {{R, 0}, {R - R / 2, R / 6}}, // ----->
    {{R, 0}, {R - R / 2, -R / 6}},
    {{-R + R / 8, 0}, {-R - R / 8, R / 6}}, // >----->
    {{-R + R / 8, 0}, {-R - R / 8, -R / 6}},
    {{-R + 3 * R / 8, 0}, {-R + R / 8, R / 6}}, // >>----->
    {{-R + 3 * R / 8, 0}, {-R + R / 8, -R / 6}},
    {{-R / 2, 0}, {-R / 2, -R / 6}}, // >>-d--->
    {{-R / 2, -R / 6}, {-R / 2 + R / 6, -R / 6}},
    {{-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6, R / 4}},
    {{-R / 6, 0}, {-R / 6, -R / 6}}, // >>-dd-->
    {{-R / 6, -R / 6}, {0, -R / 6}},
    {{0, -R / 6}, {0, R / 4}},
    {{R / 6, R / 4}, {R / 6, -R / 7}}, // >>-ddt->
    {{R / 6, -R / 7}, {R / 6 + R / 32, -R / 7 - R / 32}},
    {{R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7}}
};

#elif __JHERETIC__
vgline_t player_arrow[] = {
    {{-R + R / 4, 0}, {0, 0}}, // center line.
    {{-R + R / 4, R / 8}, {R, 0}}, // blade
    {{-R + R / 4, -R / 8}, {R, 0}},
    {{-R + R / 4, -R / 4}, {-R + R / 4, R / 4}}, // crosspiece
    {{-R + R / 8, -R / 4}, {-R + R / 8, R / 4}},
    {{-R + R / 8, -R / 4}, {-R + R / 4, -R / 4}}, //crosspiece connectors
    {{-R + R / 8, R / 4}, {-R + R / 4, R / 4}},
    {{-R - R / 4, R / 8}, {-R - R / 4, -R / 8}}, // pommel
    {{-R - R / 4, R / 8}, {-R + R / 8, R / 8}},
    {{-R - R / 4, -R / 8}, {-R + R / 8, -R / 8}}
};

vgline_t cheat_player_arrow[] = {
    {{-R + R / 8, 0}, {R, 0}}, // -----
    {{R, 0}, {R - R / 2, R / 6}}, // ----->
    {{R, 0}, {R - R / 2, -R / 6}},
    {{-R + R / 8, 0}, {-R - R / 8, R / 6}}, // >----->
    {{-R + R / 8, 0}, {-R - R / 8, -R / 6}},
    {{-R + 3 * R / 8, 0}, {-R + R / 8, R / 6}}, // >>----->
    {{-R + 3 * R / 8, 0}, {-R + R / 8, -R / 6}},
    {{-R / 2, 0}, {-R / 2, -R / 6}}, // >>-d--->
    {{-R / 2, -R / 6}, {-R / 2 + R / 6, -R / 6}},
    {{-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6, R / 4}},
    {{-R / 6, 0}, {-R / 6, -R / 6}}, // >>-dd-->
    {{-R / 6, -R / 6}, {0, -R / 6}},
    {{0, -R / 6}, {0, R / 4}},
    {{R / 6, R / 4}, {R / 6, -R / 7}}, // >>-ddt->
    {{R / 6, -R / 7}, {R / 6 + R / 32, -R / 7 - R / 32}},
    {{R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7}}
};

#elif __JHEXEN__
vgline_t player_arrow[] = {
    {{-R + R / 4, 0}, {0, 0}}, // center line.
    {{-R + R / 4, R / 8}, {R, 0}}, // blade
    {{-R + R / 4, -R / 8}, {R, 0}},
    {{-R + R / 4, -R / 4}, {-R + R / 4, R / 4}}, // crosspiece
    {{-R + R / 8, -R / 4}, {-R + R / 8, R / 4}},
    {{-R + R / 8, -R / 4}, {-R + R / 4, -R / 4}}, // crosspiece connectors
    {{-R + R / 8, R / 4}, {-R + R / 4, R / 4}},
    {{-R - R / 4, R / 8}, {-R - R / 4, -R / 8}}, // pommel
    {{-R - R / 4, R / 8}, {-R + R / 8, R / 8}},
    {{-R - R / 4, -R / 8}, {-R + R / 8, -R / 8}}
};
#endif

#undef R

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static vectorgraphic_t* vectorGraphics[NUM_VECTOR_GRAPHICS];

// CODE --------------------------------------------------------------------

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
    memset(vectorGraphics, 0, sizeof(vectorGraphics));
}

void R_ShutdownVectorGraphics(void)
{
    uint i;
    for(i = 0; i < NUM_VECTOR_GRAPHICS; ++i)
    {
        vectorgraphic_t* vg = vectorGraphics[i];
        if(!vg)
            continue;
        if(!(Get(DD_NOVIDEO) || IS_DEDICATED))
        {
            if(vg->dlist)
                DGL_DeleteLists(vg->dlist, 1);
        }
        free(vg->lines);
        free(vg);
    }
}

/**
 * Unload any resources needed for vector graphics.
 * Called during shutdown and before a renderer restart.
 */
void R_UnloadVectorGraphics(void)
{
    if(Get(DD_NOVIDEO) || IS_DEDICATED)
        return; // Nothing to do.

    {uint i;
    for(i = 0; i < NUM_VECTOR_GRAPHICS; ++i)
    {
        vectorgraphic_t* vg = R_PrepareVectorGraphic(i);
        if(!vg)
            continue;
        if(vg->dlist)
            DGL_DeleteLists(vg->dlist, 1);
        vg->dlist = 0;
    }}
}

void R_DrawVectorGraphic(vectorgraphic_t* vg)
{
    assert(vg);

    if(!vg->dlist)
        vg->dlist = constructDisplayList(0, vg);

    if(vg->dlist)
    {   // We have a display list available; call it and get out of here.
        DGL_CallList(vg->dlist);
        return;
    }

    // No display list available. Lets draw it manually.
    draw(vg);
}

vectorgraphic_t* R_PrepareVectorGraphic(vectorgraphicname_t id)
{
    vectorgraphic_t* vg;
    vgline_t* lines;

    if(id > NUM_VECTOR_GRAPHICS - 1)
        return NULL;

    if(vectorGraphics[id])
        return vectorGraphics[id];

    // Not loaded yet.
    vg = vectorGraphics[id] = malloc(sizeof(*vg));
    {
    uint i, linecount;
    switch(id)
    {
    case VG_KEYSQUARE:
        lines = keysquare;
        linecount = sizeof(keysquare) / sizeof(vgline_t);
        break;

    case VG_TRIANGLE:
        lines = thintriangle_guy;
        linecount = sizeof(thintriangle_guy) / sizeof(vgline_t);
        break;

    case VG_ARROW:
        lines = player_arrow;
        linecount = sizeof(player_arrow) / sizeof(vgline_t);
        break;

#if !__JHEXEN__
    case VG_CHEATARROW:
        lines = cheat_player_arrow;
        linecount = sizeof(cheat_player_arrow) / sizeof(vgline_t);
        break;
#endif

    default:
        Con_Error("R_PrepareVectorGraphic: Unknown id %i.", id);
        break;
    }

    vg->lines = malloc(linecount * sizeof(vgline_t));
    vg->count = linecount;
    vg->dlist = 0;
    for(i = 0; i < linecount; ++i)
        memcpy(&vg->lines[i], &lines[i], sizeof(vgline_t));
    }

    return vg;
}
