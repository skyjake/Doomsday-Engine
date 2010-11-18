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
static patchid_t borderPatches[9];

// CODE --------------------------------------------------------------------

static void loadViewBorderPatches(void)
{
    patchinfo_t info;
    uint i;

    borderPatches[0] = 0;
    for(i = 1; i < 9; ++i)
    {
        R_PrecachePatch(borderPatchNames[i], &info);
        borderPatches[i] = info.id;
    }

    // Detemine the view border width.
    if(borderPatches[BG_TOP] == 0)
    {
        bwidth = 0;
        return;
    }

    R_GetPatchInfo(borderPatches[BG_TOP], &info);
    bwidth = info.height;
}

void R_SetBorderGfx(char* const* lumpNames)
{
    if(!lumpNames)
        Con_Error("R_SetBorderGfx: Missing argument.");

    {uint i;
    for(i = 0; i < 9; ++i)
    {
        if(lumpNames[i] && lumpNames[i][0])
        {
            dd_snprintf(borderPatchNames[i], 9, "%s", lumpNames[i]);
        }
        else
        {
            borderPatchNames[i][0] = 0;
        }
    }}

    loadViewBorderPatches();
}

void R_InitViewBorder(void)
{
    memset(borderPatchNames, 0, sizeof(borderPatchNames));
    memset(borderPatches, 0, sizeof(borderPatches));
    bwidth = 0;
}

void R_DrawPatch3(patchtex_t* p, int x, int y, int w, int h, boolean useOffsets)
{
    assert(p);

    glBindTexture(GL_TEXTURE_2D, GL_PreparePatch(p));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glmode[texMagMode]);

    if(useOffsets)
    {
        x += p->offX;
        y += p->offY;
    }

    GL_DrawRect(x, y, w, h, 1, 1, 1, 1);
}

void R_DrawPatch2(patchtex_t* p, int x, int y, int w, int h)
{
    R_DrawPatch3(p, x, y, w, h, true);
}

void R_DrawPatch(patchtex_t* p, int x, int y)
{
    R_DrawPatch2(p, x, y, p->width, p->height);
}

void R_DrawPatchTiled(patchtex_t* p, int x, int y, int w, int h, GLint wrapS, GLint wrapT)
{
    assert(p);

    glBindTexture(GL_TEXTURE_2D, GL_PreparePatch(p));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glmode[texMagMode]);

    GL_DrawRectTiled(x, y, w, h, p->width, p->height);
}

/**
 * Draws the border around the view for different size windows.
 */
void R_DrawViewBorder(void)
{
    int border;
    const viewport_t* port;
    material_t* mat;

    port = R_CurrentViewPort();
    assert(port);

    if(viewwidth == port->width && viewheight == port->height)
        return;

    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_TEXTURE);
    glPushMatrix();

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
    mat = Materials_ToMaterial(Materials_NumForName(borderPatchNames[BG_BACKGROUND], MN_FLATS));
    if(mat)
    {
        GL_SetMaterial(mat);
        GL_DrawCutRectTiled(0, 0, port->width, port->height, mat->width, mat->height, 0, 0,
                            viewwindowx - border, viewwindowy - border,
                            viewwidth + 2 * border, viewheight + 2 * border);
    }

    if(border != 0)
    {
        R_DrawPatchTiled(R_FindPatchTex(borderPatches[BG_TOP]), viewwindowx, viewwindowy - border, viewwidth, border, GL_REPEAT, GL_CLAMP_TO_EDGE);
        R_DrawPatchTiled(R_FindPatchTex(borderPatches[BG_BOTTOM]), viewwindowx, viewwindowy + viewheight , viewwidth, border, GL_REPEAT, GL_CLAMP_TO_EDGE);
        R_DrawPatchTiled(R_FindPatchTex(borderPatches[BG_LEFT]), viewwindowx - border, viewwindowy, border, viewheight, GL_CLAMP_TO_EDGE, GL_REPEAT);
        R_DrawPatchTiled(R_FindPatchTex(borderPatches[BG_RIGHT]), viewwindowx + viewwidth, viewwindowy, border, viewheight, GL_CLAMP_TO_EDGE, GL_REPEAT);
    }

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    if(border != 0)
    {
        R_DrawPatch3(R_FindPatchTex(borderPatches[BG_TOPLEFT]), viewwindowx - border, viewwindowy - border, border, border, false);
        R_DrawPatch3(R_FindPatchTex(borderPatches[BG_TOPRIGHT]), viewwindowx + viewwidth, viewwindowy - border, border, border, false);
        R_DrawPatch3(R_FindPatchTex(borderPatches[BG_BOTTOMRIGHT]), viewwindowx + viewwidth, viewwindowy + viewheight, border, border, false);
        R_DrawPatch3(R_FindPatchTex(borderPatches[BG_BOTTOMLEFT]), viewwindowx - border, viewwindowy + viewheight, border, border, false);
    }

    glDisable(GL_TEXTURE_2D);
}
