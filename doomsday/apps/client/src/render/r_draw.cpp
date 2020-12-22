/** @file r_draw.cpp  Misc Drawing Routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include <de/legacy/concurrency.h>
#include <doomsday/res/textures.h>
#include <doomsday/world/materials.h>
#include <de/glinfo.h>

#include "clientapp.h"
#include "sys_system.h"
#include "render/r_main.h"
#include "render/r_draw.h"
#include "resource/clientresources.h"
#include "gl/gl_draw.h"
#include "gl/gl_main.h"
#include "gl/sys_opengl.h"

#include "api_resource.h"
#include "resource/materialanimator.h"

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

static bool initedDraw = false;
static int borderSize;
static res::Uri *borderGraphicsNames[9];

/// @todo Declare the patches with URNs to avoid unnecessary duplication here -ds
static patchid_t borderPatches[9];

static void loadViewBorderPatches()
{
    borderPatches[0] = 0;
    for(uint i = 1; i < 9; ++i)
    {
        borderPatches[i] = res::Textures::get().declarePatch(borderGraphicsNames[i]->path());
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

static ClientTexture &borderTexture(int borderComp)
{
    res::TextureScheme &patches = res::Textures::get().textureScheme("Patches");
    DE_ASSERT(borderComp >= 0 && borderComp < 9);
    return static_cast<ClientTexture &>(patches.findByUniqueId(borderPatches[borderComp]).texture());
}

#undef R_SetBorderGfx
DE_EXTERN_C void R_SetBorderGfx(const struct uri_s *const *paths)
{
    DE_ASSERT(initedDraw);
    if(!paths) return;

    for(uint i = 0; i < 9; ++i)
    {
        if(paths[i])
        {
            if(!borderGraphicsNames[i])
            {
                borderGraphicsNames[i] = new res::Uri;
            }
            *(borderGraphicsNames[i]) = *reinterpret_cast<const res::Uri *>(paths[i]);
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

    if(initedDraw)
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
    initedDraw = true;
}

void R_ShutdownViewWindow()
{
    if(!initedDraw) return;

    for(int i = 0; i < 9; ++i)
    {
        if(borderGraphicsNames[i])
        {
            delete borderGraphicsNames[i];
        }
    }

    de::zap(borderGraphicsNames);
    initedDraw = false;
}

const TextureVariantSpec &Rend_PatchTextureSpec(int              flags,
                                                gfx::Wrapping wrapS,
                                                gfx::Wrapping wrapT)
{
    return ClientApp::resources().textureSpec(TC_UI, flags, 0, 0, 0,
        GL_Wrap(wrapS), GL_Wrap(wrapT), 0, -3, 0, false, false, false, false);
}

void R_DrawPatch(ClientTexture &texture, int x, int y, int w, int h, bool useOffsets)
{
    if(texture.manifest().schemeName().compareWithoutCase("Patches"))
    {
        LOG_AS("R_DrawPatch3");
        LOGDEV_GL_WARNING("Cannot draw a non-patch [%p]") << dintptr(&texture);
        return;
    }

    const TextureVariantSpec &texSpec =
        Rend_PatchTextureSpec(0 | (texture.isFlagged(res::Texture::Monochrome)        ? TSF_MONOCHROME : 0)
                                | (texture.isFlagged(res::Texture::UpscaleAndSharpen) ? TSF_UPSCALE_AND_SHARPEN : 0));
    GL_BindTexture(texture.prepareVariant(texSpec));

    if(useOffsets)
    {
        x += texture.origin().x;
        y += texture.origin().y;
    }

    GL_DrawRectf2Color(x, y, w, h, 1, 1, 1, 1);
}

void R_DrawPatch(ClientTexture &tex, int x, int y)
{
    R_DrawPatch(tex, x, y, tex.width(), tex.height());
}

void R_DrawPatchTiled(ClientTexture &  texture,
                      int              x,
                      int              y,
                      int              w,
                      int              h,
                      gfx::Wrapping wrapS,
                      gfx::Wrapping wrapT)
{
    const TextureVariantSpec &spec =
        Rend_PatchTextureSpec(0 | (texture.isFlagged(res::Texture::Monochrome)        ? TSF_MONOCHROME : 0)
                                | (texture.isFlagged(res::Texture::UpscaleAndSharpen) ? TSF_UPSCALE_AND_SHARPEN : 0),
                              wrapS, wrapT);

    GL_BindTexture(texture.prepareVariant(spec));
    GL_DrawRectf2Tiled(x, y, w, h, texture.width(), texture.height());
}

static const MaterialVariantSpec &bgMaterialSpec()
{
    return ClientApp::resources().materialSpec(UiContext, 0, 0, 0, 0,
                                                    GL_REPEAT, GL_REPEAT, 0, -3,
                                                    0, false, false, false, false);
}

/// @todo Optimize: Do not search for resources (materials, textures) each frame.
void R_DrawViewBorder()
{
    DE_ASSERT(initedDraw);

    const viewport_t *port = R_CurrentViewPort();
    const viewdata_t *vd = &DD_Player(displayPlayer)->viewport();
    DE_ASSERT(port != 0 && vd != 0);

    if (!borderGraphicsNames[BG_BACKGROUND]) return;
    if (vd->window.isNull()) return;
    if (vd->window.size() >= port->geometry.size()) return;

    const Vec2i origin = port->geometry.topLeft;

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PushMatrix();

    // Scale from viewport space to fixed 320x200 space.
    int border;
    if(port->geometry.width() >= port->geometry.height())
    {
        DGL_Scalef(float(SCREENHEIGHT) / port->geometry.height(),
                   float(SCREENHEIGHT) / port->geometry.height(), 1);
        border = float(borderSize) / SCREENHEIGHT * port->geometry.height();
    }
    else
    {
        DGL_Scalef(float(SCREENWIDTH) / port->geometry.width(),
                   float(SCREENWIDTH) / port->geometry.width(), 1);
        border = float(borderSize) / SCREENWIDTH * port->geometry.width();
    }

    DGL_Color4f(1, 1, 1, 1);

    // View background.
    try
    {
        MaterialAnimator &matAnimator = ClientMaterial::find(*borderGraphicsNames[BG_BACKGROUND])
                .getAnimator(bgMaterialSpec());

        // Ensure we've up to date info about the material.
        matAnimator.prepare();

        GL_BindTexture(matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture);
        const Vec2ui &matDimensions = matAnimator.dimensions();

        GL_DrawCutRectf2Tiled(origin.x, origin.y, port->geometry.width(), port->geometry.height(),
                              matDimensions.x, matDimensions.y, 0, 0,
                              origin.x + vd->window.topLeft.x - border,
                              origin.y + vd->window.topLeft.y - border,
                              vd->window.width() + 2 * border, vd->window.height() + 2 * border);
    }
    catch(const world::MaterialManifest::MissingMaterialError &)
    {} // Ignore this error.

    if(border)
    {
        R_DrawPatchTiled(borderTexture(BG_TOP),    origin.x + vd->window.topLeft.x,          origin.y + vd->window.topLeft.y - border, vd->window.width(), border, gfx::Repeat, gfx::ClampToEdge);
        R_DrawPatchTiled(borderTexture(BG_BOTTOM), origin.x + vd->window.topLeft.x,          origin.y + vd->window.bottomRight.y, vd->window.width(), border, gfx::Repeat, gfx::ClampToEdge);
        R_DrawPatchTiled(borderTexture(BG_LEFT),   origin.x + vd->window.topLeft.x - border, origin.y + vd->window.topLeft.y, border, vd->window.height(), gfx::ClampToEdge, gfx::Repeat);
        R_DrawPatchTiled(borderTexture(BG_RIGHT),  origin.x + vd->window.topRight().x,       origin.y + vd->window.topRight().y, border, vd->window.height(), gfx::ClampToEdge, gfx::Repeat);
    }

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PopMatrix();

    if(border)
    {
        R_DrawPatch(borderTexture(BG_TOPLEFT),     origin.x + vd->window.topLeft.x - border,      origin.y + vd->window.topLeft.y - border, border, border, false);
        R_DrawPatch(borderTexture(BG_TOPRIGHT),    origin.x + vd->window.topRight().x,            origin.y + vd->window.topLeft.y - border, border, border, false);
        R_DrawPatch(borderTexture(BG_BOTTOMRIGHT), origin.x + vd->window.bottomRight.x,           origin.y + vd->window.bottomRight.y, border, border, false);
        R_DrawPatch(borderTexture(BG_BOTTOMLEFT),  origin.x + vd->window.bottomLeft().x - border, origin.y + vd->window.bottomRight.y, border, border, false);
    }

    DGL_Disable(DGL_TEXTURE_2D);
}
