/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * r_draw.c: Drawing Routines
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_graphics.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

enum                            // A logical ordering (twice around).
{
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
int     viewwidth, viewheight, viewwindowx, viewwindowy;

// View border width.
int     bwidth;

// The view border graphics.
char    borderGfx[9][9];

boolean BorderNeedRefresh;
boolean BorderTopRefresh;

byte   *dc_translation;
byte   *translationtables;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void R_SetBorderGfx(char *gfx[9])
{
    uint        i;

    for(i = 0; i < 9; i++)
        if(gfx[i])
            strcpy(borderGfx[i], gfx[i]);
        else
            strcpy(borderGfx[i], "-");

    R_InitViewBorder();
}

void R_InitViewBorder(void)
{
    lumppatch_t    *patch = NULL;

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
    texinfo_t      *texinfo;

    if(viewwidth == 320 && viewheight == 200)
        return;

    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, 320, 200, -1, 1);

    // View background.
    GL_SetColorAndAlpha(1, 1, 1, 1);
    GL_SetFlat(R_FlatNumForName(borderGfx[BG_BACKGROUND]));
    GL_DrawCutRectTiled(0, 0, 320, 200, 64, 64, 0, 0, viewwindowx - bwidth,
                        viewwindowy - bwidth, viewwidth + 2 * bwidth,
                        viewheight + 2 * bwidth);

    // The border top.
    gl.Bind(curtex = GL_PreparePatch(W_GetNumForName(borderGfx[BG_TOP]), &texinfo));
    GL_DrawRectTiled(viewwindowx, viewwindowy - bwidth, viewwidth,
                     texinfo->height, 16, texinfo->height);
    // Border bottom.
    gl.Bind(curtex = GL_PreparePatch(W_GetNumForName(borderGfx[BG_BOTTOM]), &texinfo));
    GL_DrawRectTiled(viewwindowx, viewwindowy + viewheight , viewwidth,
                     texinfo->height, 16, texinfo->height);

    // Left view border.
    gl.Bind(curtex = GL_PreparePatch(W_GetNumForName(borderGfx[BG_LEFT]), &texinfo));
    GL_DrawRectTiled(viewwindowx - bwidth, viewwindowy,
                     texinfo->width, viewheight,
                     texinfo->width, 16);
    // Right view border.
    gl.Bind(curtex = GL_PreparePatch(W_GetNumForName(borderGfx[BG_RIGHT]), &texinfo));
    GL_DrawRectTiled(viewwindowx + viewwidth , viewwindowy,
                     texinfo->width, viewheight,
                     texinfo->width, 16);

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

    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
}

/**
 * Draws the top border around the view for different size windows.
 */
void R_DrawTopBorder(void)
{
    if(viewwidth == 320 && viewheight == 200)
        return;

    GL_SetColorAndAlpha(1, 1, 1, 1);
    GL_SetFlat(R_FlatNumForName(borderGfx[BG_BACKGROUND]));

    GL_DrawRectTiled(0, 0, 320, 64, 64, 64);
    if(viewwindowy < 65)
    {
        texinfo_t      *texinfo;

        gl.Bind(curtex = GL_PreparePatch(W_GetNumForName(borderGfx[BG_TOP]), &texinfo));
        GL_DrawRectTiled(viewwindowx, viewwindowy - bwidth, viewwidth,
                         texinfo->height, 16,
                         texinfo->height);

        GL_UsePatchOffset(false);
        GL_DrawPatch(viewwindowx - bwidth, viewwindowy,
                     W_GetNumForName(borderGfx[BG_LEFT]));
        GL_DrawPatch(viewwindowx + viewwidth, viewwindowy,
                     W_GetNumForName(borderGfx[BG_RIGHT]));
        GL_DrawPatch(viewwindowx - bwidth, viewwindowy + 16,
                     W_GetNumForName(borderGfx[BG_LEFT]));
        GL_DrawPatch(viewwindowx + viewwidth, viewwindowy + 16,
                     W_GetNumForName(borderGfx[BG_RIGHT]));

        GL_DrawPatch(viewwindowx - bwidth, viewwindowy - bwidth,
                     W_GetNumForName(borderGfx[BG_TOPLEFT]));
        GL_DrawPatch(viewwindowx + viewwidth, viewwindowy - bwidth,
                     W_GetNumForName(borderGfx[BG_TOPRIGHT]));
        GL_UsePatchOffset(true);
    }
}
