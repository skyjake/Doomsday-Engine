/**\file r_draw.c
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
 * Misc Drawing Routines.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_play.h"

#include "sys_opengl.h"
#include "texture.h"
#include "materialvariant.h"

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

// View border width.
int bwidth;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;
static dduri_t* borderGraphicsNames[9];
static patchid_t borderPatches[9];

// CODE --------------------------------------------------------------------

static void loadViewBorderPatches(void)
{
    patchinfo_t info;
    uint i;

    borderPatches[0] = 0;
    for(i = 1; i < 9; ++i)
    {
        R_PrecachePatch(Str_Text(Uri_Path(borderGraphicsNames[i])), &info);
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

void R_SetBorderGfx(const dduri_t* paths[9])
{
    assert(inited);
    {
    if(!paths)
        Con_Error("R_SetBorderGfx: Missing argument.");

    {uint i;
    for(i = 0; i < 9; ++i)
    {
        if(paths[i])
        {
            if(!borderGraphicsNames[i])
                borderGraphicsNames[i] = Uri_ConstructCopy(paths[i]);
            else
                Uri_Copy(borderGraphicsNames[i], paths[i]);
        }
        else
        {
            if(borderGraphicsNames[i])
                Uri_Destruct(borderGraphicsNames[i]);
            borderGraphicsNames[i] = 0;
        }
    }}

    loadViewBorderPatches();
    }
}

void R_InitViewWindow(void)
{
    int i;
    /// \fixme Do not assume native game resolution.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        R_SetViewWindow(i, 0, 0, SCREENWIDTH, SCREENHEIGHT);
    }

    if(inited)
    {
        for(i = 0; i < 9; ++i)
            if(borderGraphicsNames[i])
                Uri_Destruct(borderGraphicsNames[i]);
    }
    memset(borderGraphicsNames, 0, sizeof(borderGraphicsNames));
    memset(borderPatches, 0, sizeof(borderPatches));
    bwidth = 0;
    inited = true;
}

void R_ShutdownViewWindow(void)
{
    if(!inited) return;
    { uint i;
    for(i = 0; i < 9; ++i)
        if(borderGraphicsNames[i])
            Uri_Destruct(borderGraphicsNames[i]);
    }
    memset(borderGraphicsNames, 0, sizeof(borderGraphicsNames));
    inited = false;
}

void R_DrawPatch3(patchtex_t* p, int x, int y, int w, int h, boolean useOffsets)
{
    assert(p);

    glBindTexture(GL_TEXTURE_2D, GL_PreparePatch(p));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (filterUI ? GL_LINEAR : GL_NEAREST));

    if(useOffsets)
    {
        x += p->offX;
        y += p->offY;
    }

    GL_DrawRectColor(x, y, w, h, 1, 1, 1, 1);
}

void R_DrawPatch2(patchtex_t* p, int x, int y, int w, int h)
{
    R_DrawPatch3(p, x, y, w, h, true);
}

void R_DrawPatch(patchtex_t* p, int x, int y)
{
    texture_t* tex = GL_ToTexture(p->texId);
    if(NULL == tex) return;
    R_DrawPatch2(p, x, y, Texture_Width(tex), Texture_Height(tex));
}

void R_DrawPatchTiled(patchtex_t* p, int x, int y, int w, int h, DGLint wrapS, DGLint wrapT)
{
    texture_t* tex = GL_ToTexture(p->texId);
    if(NULL == tex) return;

    glBindTexture(GL_TEXTURE_2D, GL_PreparePatch(p));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (filterUI ? GL_LINEAR : GL_NEAREST));

    GL_DrawRectTiled(x, y, w, h, Texture_Width(tex), Texture_Height(tex));
}

/**
 * Draws the border around the view for different size windows.
 */
void R_DrawViewBorder(void)
{
    assert(inited);
    {
    const viewport_t* port = R_CurrentViewPort();
    const viewdata_t* vd = R_ViewData(displayPlayer);
    material_t* mat;
    int border;

    assert(NULL != port);
    assert(NULL != vd);

    if(0 == vd->windowWidth || 0 == vd->windowHeight) return;
    if(vd->windowWidth == port->width && vd->windowHeight == port->height) return;

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
    mat = Materials_ToMaterial(Materials_IndexForUri(borderGraphicsNames[BG_BACKGROUND]));
    if(mat)
    {
        material_snapshot_t ms;
        Materials_Prepare(&ms, mat, true,
            Materials_VariantSpecificationForContext(MC_UI, 0, 0, 0, 0,
                GL_REPEAT, GL_REPEAT, 0, 1, 0, false, false, false, false));
        GL_BindTexture(MSU(&ms, MTU_PRIMARY).tex.glName, (filterUI ? GL_LINEAR : GL_NEAREST));
        GL_DrawCutRectTiled(0, 0, port->width, port->height, ms.width, ms.height, 0, 0,
                            vd->windowX - border, vd->windowY - border,
                            vd->windowWidth + 2 * border, vd->windowHeight + 2 * border);
    }

    if(border != 0)
    {
        R_DrawPatchTiled(R_PatchTextureByIndex(borderPatches[BG_TOP]), vd->windowX, vd->windowY - border, vd->windowWidth, border, GL_REPEAT, GL_CLAMP_TO_EDGE);
        R_DrawPatchTiled(R_PatchTextureByIndex(borderPatches[BG_BOTTOM]), vd->windowX, vd->windowY + vd->windowHeight , vd->windowWidth, border, GL_REPEAT, GL_CLAMP_TO_EDGE);
        R_DrawPatchTiled(R_PatchTextureByIndex(borderPatches[BG_LEFT]), vd->windowX - border, vd->windowY, border, vd->windowHeight, GL_CLAMP_TO_EDGE, GL_REPEAT);
        R_DrawPatchTiled(R_PatchTextureByIndex(borderPatches[BG_RIGHT]), vd->windowX + vd->windowWidth, vd->windowY, border, vd->windowHeight, GL_CLAMP_TO_EDGE, GL_REPEAT);
    }

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    if(border != 0)
    {
        R_DrawPatch3(R_PatchTextureByIndex(borderPatches[BG_TOPLEFT]), vd->windowX - border, vd->windowY - border, border, border, false);
        R_DrawPatch3(R_PatchTextureByIndex(borderPatches[BG_TOPRIGHT]), vd->windowX + vd->windowWidth, vd->windowY - border, border, border, false);
        R_DrawPatch3(R_PatchTextureByIndex(borderPatches[BG_BOTTOMRIGHT]), vd->windowX + vd->windowWidth, vd->windowY + vd->windowHeight, border, border, false);
        R_DrawPatch3(R_PatchTextureByIndex(borderPatches[BG_BOTTOMLEFT]), vd->windowX - border, vd->windowY + vd->windowHeight, border, border, false);
    }

    glDisable(GL_TEXTURE_2D);
    }
}
