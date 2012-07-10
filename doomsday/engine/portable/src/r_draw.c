/**\file r_draw.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include "texturevariant.h"
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
static Uri* borderGraphicsNames[9];
static patchid_t borderPatches[9];

// CODE --------------------------------------------------------------------

static void loadViewBorderPatches(void)
{
    patchinfo_t info;
    uint i;

    borderPatches[0] = 0;
    for(i = 1; i < 9; ++i)
    {
        borderPatches[i] = R_DeclarePatch(Str_Text(Uri_Path(borderGraphicsNames[i])));
    }

    // Detemine the view border width.
    bwidth = 0;
    if(!R_GetPatchInfo(borderPatches[BG_TOP], &info)) return;
    bwidth = info.geometry.size.height;
}

void R_SetBorderGfx(const Uri* const* paths)
{
    uint i;
    assert(inited);

    if(!paths) Con_Error("R_SetBorderGfx: Missing argument.");

    for(i = 0; i < 9; ++i)
    {
        if(paths[i])
        {
            if(!borderGraphicsNames[i])
                borderGraphicsNames[i] = Uri_NewCopy(paths[i]);
            else
                Uri_Copy(borderGraphicsNames[i], paths[i]);
        }
        else
        {
            if(borderGraphicsNames[i])
                Uri_Delete(borderGraphicsNames[i]);
            borderGraphicsNames[i] = 0;
        }
    }

    loadViewBorderPatches();
}

void R_InitViewWindow(void)
{
    int i;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        R_SetupDefaultViewWindow(i);
    }

    if(inited)
    {
        for(i = 0; i < 9; ++i)
        {
            if(borderGraphicsNames[i])
                Uri_Delete(borderGraphicsNames[i]);
        }
    }
    memset(borderGraphicsNames, 0, sizeof(borderGraphicsNames));
    memset(borderPatches, 0, sizeof(borderPatches));
    bwidth = 0;
    inited = true;
}

void R_ShutdownViewWindow(void)
{
    uint i;
    if(!inited) return;

    for(i = 0; i < 9; ++i)
    {
        if(borderGraphicsNames[i])
            Uri_Delete(borderGraphicsNames[i]);
    }
    memset(borderGraphicsNames, 0, sizeof(borderGraphicsNames));
    inited = false;
}

void R_DrawPatch3(Texture* tex, int x, int y, int w, int h, boolean useOffsets)
{
    if(!tex) return;
    if(Textures_Namespace(Textures_Id(tex)) != TN_PATCHES)
    {
#if _DEBUG
        Con_Message("Warning:R_DrawPatch3: Attempted to draw a non-patch [%p].\n", (void*)tex);
#endif
        return;
    }

    GL_BindTexture(GL_PreparePatchTexture(tex));
    if(useOffsets)
    {
        patchtex_t* pTex = (patchtex_t*)Texture_UserData(tex);
        assert(pTex);

        x += pTex->offX;
        y += pTex->offY;
    }

    GL_DrawRectf2Color(x, y, w, h, 1, 1, 1, 1);
}

void R_DrawPatch2(Texture* tex, int x, int y, int w, int h)
{
    R_DrawPatch3(tex, x, y, w, h, true);
}

void R_DrawPatch(Texture* tex, int x, int y)
{
    R_DrawPatch2(tex, x, y, Texture_Width(tex), Texture_Height(tex));
}

void R_DrawPatchTiled(Texture* tex, int x, int y, int w, int h, int wrapS, int wrapT)
{
    if(!tex) return;

    GL_BindTexture(GL_PreparePatchTexture2(tex, wrapS, wrapT));
    GL_DrawRectf2Tiled(x, y, w, h, Texture_Width(tex), Texture_Height(tex));
}

/**
 * Draws the border around the view for different size windows.
 */
void R_DrawViewBorder(void)
{
    const viewport_t* port = R_CurrentViewPort();
    const viewdata_t* vd = R_ViewData(displayPlayer);
    material_t* mat;
    int border;
    assert(inited);

    assert(port && vd);

    if(0 == vd->window.size.width || 0 == vd->window.size.height) return;
    if(vd->window.size.width == port->geometry.size.width && vd->window.size.height == port->geometry.size.height) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_TEXTURE);
    glPushMatrix();

    // Scale from viewport space to fixed 320x200 space.
    if(port->geometry.size.width >= port->geometry.size.height)
    {
        glScalef((float)SCREENHEIGHT/port->geometry.size.height, (float)SCREENHEIGHT/port->geometry.size.height, 1);
        border = (float) bwidth / SCREENHEIGHT * port->geometry.size.height;
    }
    else
    {
        glScalef((float)SCREENWIDTH/port->geometry.size.width, (float)SCREENWIDTH/port->geometry.size.width, 1);
        border = (float) bwidth / SCREENWIDTH * port->geometry.size.width;
    }

    glColor4f(1, 1, 1, 1);

    // View background.
    mat = Materials_ToMaterial(Materials_ResolveUri2(borderGraphicsNames[BG_BACKGROUND], true/*quiet please*/));
    if(mat)
    {
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_UI, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, 0, -3, 0, false, false, false, false);
        const materialsnapshot_t* ms = Materials_Prepare(mat, spec, true);

        GL_BindTexture(MST(ms, MTU_PRIMARY));
        GL_DrawCutRectf2Tiled(0, 0, port->geometry.size.width, port->geometry.size.height, ms->size.width, ms->size.height, 0, 0,
                            vd->window.origin.x - border, vd->window.origin.y - border,
                            vd->window.size.width + 2 * border, vd->window.size.height + 2 * border);
    }

    if(border != 0)
    {
        R_DrawPatchTiled(Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, borderPatches[BG_TOP])), vd->window.origin.x, vd->window.origin.y - border, vd->window.size.width, border, GL_REPEAT, GL_CLAMP_TO_EDGE);
        R_DrawPatchTiled(Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, borderPatches[BG_BOTTOM])), vd->window.origin.x, vd->window.origin.y + vd->window.size.height , vd->window.size.width, border, GL_REPEAT, GL_CLAMP_TO_EDGE);
        R_DrawPatchTiled(Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, borderPatches[BG_LEFT])), vd->window.origin.x - border, vd->window.origin.y, border, vd->window.size.height, GL_CLAMP_TO_EDGE, GL_REPEAT);
        R_DrawPatchTiled(Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, borderPatches[BG_RIGHT])), vd->window.origin.x + vd->window.size.width, vd->window.origin.y, border, vd->window.size.height, GL_CLAMP_TO_EDGE, GL_REPEAT);
    }

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    if(border != 0)
    {
        R_DrawPatch3(Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, borderPatches[BG_TOPLEFT])), vd->window.origin.x - border, vd->window.origin.y - border, border, border, false);
        R_DrawPatch3(Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, borderPatches[BG_TOPRIGHT])), vd->window.origin.x + vd->window.size.width, vd->window.origin.y - border, border, border, false);
        R_DrawPatch3(Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, borderPatches[BG_BOTTOMRIGHT])), vd->window.origin.x + vd->window.size.width, vd->window.origin.y + vd->window.size.height, border, border, false);
        R_DrawPatch3(Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, borderPatches[BG_BOTTOMLEFT])), vd->window.origin.x - border, vd->window.origin.y + vd->window.size.height, border, border, false);
    }

    glDisable(GL_TEXTURE_2D);
}
