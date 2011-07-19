/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * r_draw.c: Drawing Routines
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_graphics.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

enum { // A logical ordering (twice around).
    BG_BACKGROUND,
    BG_TOP,
    BG_RIGHT,
    BG_BOTTOM,
    BG_LEFT,
    BG_TOPLEFT,
    BG_TOPRIGHT,
    BG_BOTTOMRIGHT,
    BG_BOTTOMLEFT
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The view window.
int viewwidth, viewheight, viewwindowx, viewwindowy;

// View border width.
int bwidth;

byte* translationTables;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char borderGfx[9][9];

// CODE --------------------------------------------------------------------

void R_SetBorderGfx(char *gfx[9])
{
    uint                    i;

    for(i = 0; i < 9; ++i)
        if(gfx[i])
            strcpy(borderGfx[i], gfx[i]);
        else
            strcpy(borderGfx[i], "-");

    R_InitViewBorder();
}

void R_InitViewBorder(void)
{
    lumppatch_t*            patch = NULL;

    // Detemine the view border width.
    if(W_CheckNumForName(borderGfx[BG_TOP]) == -1)
        return;

    patch = (lumppatch_t*) W_CacheLumpName(borderGfx[BG_TOP], PU_CACHE);
    bwidth = SHORT(patch->height);
}

/**
 * Draws the border around the view for different size windows.
 */
void R_DrawViewBorder(void)
{
    patchtex_t*         p;
    material_t*         mat;

    if(viewwidth == 320 && viewheight == 200)
        return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 320, 200, 0, -1, 1);

    // View background.
    glColor4f(1, 1, 1, 1);
    mat = P_ToMaterial(P_MaterialNumForName(borderGfx[BG_BACKGROUND], MN_FLATS));
    if(mat)
    {
        GL_SetMaterial(mat);
        GL_DrawCutRectTiled(0, 0, 320, 200, mat->width, mat->height, 0, 0,
                            viewwindowx - bwidth,
                            viewwindowy - bwidth, viewwidth + 2 * bwidth,
                            viewheight + 2 * bwidth);
    }

    // The border top.
    p = R_GetPatchTex(W_GetNumForName(borderGfx[BG_TOP]));
    GL_BindTexture(GL_PreparePatch(R_GetPatchTex(p->lump)), glmode[texMagMode]);
    GL_DrawRectTiled(viewwindowx, viewwindowy - bwidth, viewwidth,
                     p->height, 16, p->height);
    // Border bottom.
    p = R_GetPatchTex(W_GetNumForName(borderGfx[BG_BOTTOM]));
    GL_BindTexture(GL_PreparePatch(R_GetPatchTex(p->lump)), glmode[texMagMode]);
    GL_DrawRectTiled(viewwindowx, viewwindowy + viewheight , viewwidth,
                     p->height, 16, p->height);

    // Left view border.
    p = R_GetPatchTex(W_GetNumForName(borderGfx[BG_LEFT]));
    GL_BindTexture(GL_PreparePatch(R_GetPatchTex(p->lump)), glmode[texMagMode]);
    GL_DrawRectTiled(viewwindowx - bwidth, viewwindowy,
                     p->width, viewheight, p->width, 16);
    // Right view border.
    p = R_GetPatchTex(W_GetNumForName(borderGfx[BG_RIGHT]));
    GL_BindTexture(GL_PreparePatch(R_GetPatchTex(p->lump)), glmode[texMagMode]);
    GL_DrawRectTiled(viewwindowx + viewwidth , viewwindowy,
                     p->width, viewheight, p->width, 16);

    GL_UsePatchOffset(false);
    GL_DrawPatch(viewwindowx - bwidth, viewwindowy - bwidth,
                 W_GetNumForName(borderGfx[BG_TOPLEFT]));
    GL_DrawPatch(viewwindowx + viewwidth, viewwindowy - bwidth,
                 W_GetNumForName(borderGfx[BG_TOPRIGHT]));
    GL_DrawPatch(viewwindowx + viewwidth, viewwindowy + viewheight,
                 W_GetNumForName(borderGfx[BG_BOTTOMRIGHT]));
    GL_DrawPatch(viewwindowx - bwidth, viewwindowy + viewheight,
                 W_GetNumForName(borderGfx[BG_BOTTOMLEFT]));
    GL_UsePatchOffset(true);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
