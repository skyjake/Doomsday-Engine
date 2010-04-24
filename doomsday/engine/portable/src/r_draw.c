/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

static char borderPatchNames[9][9];
static lumpnum_t borderPatchLumps[9];

// CODE --------------------------------------------------------------------

void R_SetBorderGfx(char* gfx[9])
{
    uint i;
    for(i = 0; i < 9; ++i)
    {
        if(gfx[i])
            strcpy(borderPatchNames[i], gfx[i]);
        else
            strcpy(borderPatchNames[i], "-");
    }
    R_InitViewBorder();
}

void R_InitViewBorder(void)
{
    lumppatch_t* patch = NULL;
    uint i;

    for(i = 0; i < 9; ++i)
        borderPatchLumps[i] = W_CheckNumForName(borderPatchNames[i]);

    // Detemine the view border width.
    if(borderPatchLumps[BG_TOP] == -1)
    {
        bwidth = 0;
        return;
    }

    patch = (lumppatch_t*) W_CacheLumpNum(borderPatchLumps[BG_TOP], PU_CACHE);
    bwidth = SHORT(patch->height);
}

static void drawPatch(lumpnum_t lump, int x, int y, int w, int h)
{
    patchtex_t* p = R_GetPatchTex(lump);
    assert(p);
    glBindTexture(GL_TEXTURE_2D, GL_PreparePatch(p));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glmode[texMagMode]);
    GL_DrawRect(x, y, w, h, 1, 1, 1, 1);
}

static void drawPatchTiled(lumpnum_t lump, int x, int y, int w, int h)
{
    patchtex_t* p = R_GetPatchTex(lump);
    assert(p);
    glBindTexture(GL_TEXTURE_2D, GL_PreparePatch(p));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glmode[texMagMode]);
    GL_DrawRectTiled(x, y, w, h, p->width, p->height);
}

/**
 * Draws the border around the view for different size windows.
 */
void R_DrawViewBorder(void)
{
    int viewX, viewY, viewW, viewH, border;
    const viewport_t* port;
    float xScale, yScale;
    material_t* mat;

    if(viewwidth == 320 && viewheight == 200)
        return;

    port = R_CurrentViewPort();
    assert(port);

    xScale = (float) port->width / SCREENWIDTH;
    yScale = (float) port->height / SCREENHEIGHT;

    viewX = viewwindowx * xScale;
    viewY = viewwindowy * yScale;
    viewW = viewwidth * xScale;
    viewH = viewheight * yScale;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    /**
     * Use an orthographic projection in native screenspace. Then
     * translate and scale the projection to produce an aspect
     * corrected coordinate space at 4:3.
     */
    glOrtho(0, port->width, port->height, 0, -1, 1);

    glMatrixMode(GL_TEXTURE);
    glPushMatrix();

    // Correct viewport aspect ratio?
    if(port->width < SCREENWIDTH || port->height < SCREENHEIGHT)
    {
        if(port->width >= port->height)
            glScalef((float)port->height/SCREENHEIGHT, (float)port->height/SCREENHEIGHT, 1);
        else
            glScalef((float)port->width/SCREENWIDTH, (float)port->width/SCREENWIDTH, 1);
    }

    // Scale from viewport space to fixed 320x200 space.
    if(port->width >= port->height)
    {
        glScalef((float)SCREENHEIGHT/port->height, (float)SCREENHEIGHT/port->height, 1);
        border = (float) bwidth / SCREENHEIGHT * port->height;
    }
    else
    {
        glScalef((float)SCREENWIDTH/port->width, (float)SCREENWIDTH/port->width, 1);
        border = (float) bwidth / SCREENWIDTH * port->width;
    }

    glColor4f(1, 1, 1, 1);

    // View background.
    mat = P_ToMaterial(P_MaterialNumForName(borderPatchNames[BG_BACKGROUND], MN_FLATS));
    if(mat)
    {
        GL_SetMaterial(mat);
        GL_DrawCutRectTiled(0, 0, port->width, port->height, mat->width, mat->height, 0, 0,
                            viewX - border, viewY - border,
                            viewW + 2 * border, viewH + 2 * border);
    }

    if(border != 0)
    {
        drawPatchTiled(borderPatchLumps[BG_TOP], viewX, viewY - border, viewW, border);
        drawPatchTiled(borderPatchLumps[BG_BOTTOM], viewX, viewY + viewH , viewW, border);
        drawPatchTiled(borderPatchLumps[BG_LEFT], viewX - border, viewY, border, viewH);
        drawPatchTiled(borderPatchLumps[BG_RIGHT], viewX + viewW, viewY, border, viewH);
    }

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    if(border != 0)
    {
        drawPatch(borderPatchLumps[BG_TOPLEFT], viewX - border, viewY - border, border, border);
        drawPatch(borderPatchLumps[BG_TOPRIGHT], viewX + viewW, viewY - border, border, border);
        drawPatch(borderPatchLumps[BG_BOTTOMRIGHT], viewX + viewW, viewY + viewH, border, border);
        drawPatch(borderPatchLumps[BG_BOTTOMLEFT], viewX - border, viewY + viewH, border, border);
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
