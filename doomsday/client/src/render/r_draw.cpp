/** @file r_draw.cpp  Misc Drawing Routines.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "clientapp.h"
#include "render/r_draw.h"

#include "con_main.h"

#include "gl/gl_draw.h"
#include "gl/gl_main.h"
#include "gl/sys_opengl.h"

#include "render/r_main.h"

#include "api_resource.h"
#include "MaterialSnapshot"

#include "world/p_players.h" // displayPlayer

using namespace de;

// A logical ordering (twice around).
enum {
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

static bool inited = false;
static int borderSize;
static de::Uri *borderGraphicsNames[9];

/// @todo Declare the patches with URNs to avoid unnecessary duplication here -ds
static patchid_t borderPatches[9];

static void loadViewBorderPatches()
{
    borderPatches[0] = 0;
    for(uint i = 1; i < 9; ++i)
    {
        borderPatches[i] = ClientApp::resourceSystem().declarePatch(borderGraphicsNames[i]->path());
    }

    // Detemine the view border size.
    borderSize = 0;
    patchinfo_t info;
    if(!R_GetPatchInfo(borderPatches[BG_TOP], &info))
    {
        return;
    }
    borderSize = info.geometry.size.height;
}

static Texture &borderTexture(int borderComp)
{
    TextureScheme &patches = ClientApp::resourceSystem().textureScheme("Patches");
    DENG2_ASSERT(borderComp >= 0 && borderComp < 9);
    return patches.findByUniqueId(borderPatches[borderComp]).texture();
}

#undef R_SetBorderGfx
DENG_EXTERN_C void R_SetBorderGfx(struct uri_s const *const *paths)
{
    DENG2_ASSERT(inited);
    if(!paths) return;

    for(uint i = 0; i < 9; ++i)
    {
        if(paths[i])
        {
            if(!borderGraphicsNames[i])
            {
                borderGraphicsNames[i] = new de::Uri;
            }
            *(borderGraphicsNames[i]) = *reinterpret_cast<de::Uri const *>(paths[i]);
        }
        else
        {
            if(borderGraphicsNames[i])
            {
                delete borderGraphicsNames[i];
            }
            borderGraphicsNames[i] = 0;
        }
    }

    loadViewBorderPatches();
}

void R_InitViewWindow()
{
    if(Sys_IsShuttingDown()) return;

    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        R_SetupDefaultViewWindow(i);
    }

    if(inited)
    {
        for(int i = 0; i < 9; ++i)
        {
            if(borderGraphicsNames[i])
            {
                delete borderGraphicsNames[i];
            }
        }
    }
    de::zap(borderGraphicsNames);
    de::zap(borderPatches);
    borderSize = 0;
    inited = true;
}

void R_ShutdownViewWindow()
{
    if(!inited) return;

    for(int i = 0; i < 9; ++i)
    {
        if(borderGraphicsNames[i])
        {
            delete borderGraphicsNames[i];
        }
    }

    de::zap(borderGraphicsNames);
    inited = false;
}

TextureVariantSpec const &Rend_PatchTextureSpec(int flags, gl::Wrapping wrapS,
    gl::Wrapping wrapT)
{
    return ClientApp::resourceSystem().textureSpec(TC_UI, flags, 0, 0, 0,
        GL_Wrap(wrapS), GL_Wrap(wrapT), 0, -3, 0, false, false, false, false);
}

void R_DrawPatch(Texture &texture, int x, int y, int w, int h, bool useOffsets)
{
    if(texture.manifest().schemeName().compareWithoutCase("Patches"))
    {
        LOG_AS("R_DrawPatch3");
        LOGDEV_GL_WARNING("Cannot draw a non-patch [%p]") << dintptr(&texture);
        return;
    }

    TextureVariantSpec const &texSpec =
        Rend_PatchTextureSpec(0 | (texture.isFlagged(Texture::Monochrome)        ? TSF_MONOCHROME : 0)
                                | (texture.isFlagged(Texture::UpscaleAndSharpen) ? TSF_UPSCALE_AND_SHARPEN : 0));
    GL_BindTexture(texture.prepareVariant(texSpec));

    if(useOffsets)
    {
        x += texture.origin().x;
        y += texture.origin().y;
    }

    GL_DrawRectf2Color(x, y, w, h, 1, 1, 1, 1);
}

void R_DrawPatch(Texture &tex, int x, int y)
{
    R_DrawPatch(tex, x, y, tex.width(), tex.height());
}

void R_DrawPatchTiled(Texture &texture, int x, int y, int w, int h,
    gl::Wrapping wrapS, gl::Wrapping wrapT)
{
    TextureVariantSpec const &spec =
        Rend_PatchTextureSpec(0 | (texture.isFlagged(Texture::Monochrome)        ? TSF_MONOCHROME : 0)
                                | (texture.isFlagged(Texture::UpscaleAndSharpen) ? TSF_UPSCALE_AND_SHARPEN : 0),
                              wrapS, wrapT);

    GL_BindTexture(texture.prepareVariant(spec));
    GL_DrawRectf2Tiled(x, y, w, h, texture.width(), texture.height());
}

static MaterialVariantSpec const &bgMaterialSpec()
{
    return ClientApp::resourceSystem().materialSpec(UiContext, 0, 0, 0, 0,
                                                    GL_REPEAT, GL_REPEAT, 0, -3,
                                                    0, false, false, false, false);
}

/// @todo Optimize: Do not search for resources (materials, textures) each frame.
void R_DrawViewBorder()
{
    DENG2_ASSERT(inited);

    viewport_t const *port = R_CurrentViewPort();
    viewdata_t const *vd = R_ViewData(displayPlayer);
    DENG2_ASSERT(port != 0 && vd != 0);

    if(vd->window.isNull()) return;
    if(vd->window.size() >= port->geometry.size()) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_TEXTURE);
    glPushMatrix();

    // Scale from viewport space to fixed 320x200 space.
    int border;
    if(port->geometry.width() >= port->geometry.height())
    {
        glScalef(float(SCREENHEIGHT) / port->geometry.height(),
                 float(SCREENHEIGHT) / port->geometry.height(), 1);
        border = float(borderSize) / SCREENHEIGHT * port->geometry.height();
    }
    else
    {
        glScalef(float(SCREENWIDTH) / port->geometry.width(),
                 float(SCREENWIDTH) / port->geometry.width(), 1);
        border = float(borderSize) / SCREENWIDTH * port->geometry.width();
    }

    glColor4f(1, 1, 1, 1);

    // View background.
    try
    {
        MaterialSnapshot const &ms =
            ClientApp::resourceSystem().material(*borderGraphicsNames[BG_BACKGROUND])
                      .prepare(bgMaterialSpec());

        GL_BindTexture(&ms.texture(MTU_PRIMARY));
        GL_DrawCutRectf2Tiled(0, 0, port->geometry.width(), port->geometry.height(),
                              ms.width(), ms.height(), 0, 0,
                              vd->window.topLeft.x - border, vd->window.topLeft.y - border,
                              vd->window.width() + 2 * border, vd->window.height() + 2 * border);
    }
    catch(MaterialManifest::MissingMaterialError const &)
    {} // Ignore this error.

    if(border)
    {
        R_DrawPatchTiled(borderTexture(BG_TOP),    vd->window.topLeft.x, vd->window.topLeft.y - border, vd->window.width(), border, gl::Repeat, gl::ClampToEdge);
        R_DrawPatchTiled(borderTexture(BG_BOTTOM), vd->window.topLeft.x, vd->window.bottomRight.y, vd->window.width(), border, gl::Repeat, gl::ClampToEdge);
        R_DrawPatchTiled(borderTexture(BG_LEFT),   vd->window.topLeft.x - border, vd->window.topLeft.y, border, vd->window.height(), gl::ClampToEdge, gl::Repeat);
        R_DrawPatchTiled(borderTexture(BG_RIGHT),  vd->window.topRight().x, vd->window.topRight().y, border, vd->window.height(), gl::ClampToEdge, gl::Repeat);
    }

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    if(border)
    {
        R_DrawPatch(borderTexture(BG_TOPLEFT),     vd->window.topLeft.x - border, vd->window.topLeft.y - border, border, border, false);
        R_DrawPatch(borderTexture(BG_TOPRIGHT),    vd->window.topRight().x, vd->window.topLeft.y - border, border, border, false);
        R_DrawPatch(borderTexture(BG_BOTTOMRIGHT), vd->window.bottomRight.x, vd->window.bottomRight.y, border, border, false);
        R_DrawPatch(borderTexture(BG_BOTTOMLEFT),  vd->window.bottomLeft().x - border, vd->window.bottomRight.y, border, border, false);
    }

    glDisable(GL_TEXTURE_2D);
}
