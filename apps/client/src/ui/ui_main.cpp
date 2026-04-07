/** @file ui_main.cpp  Graphical User Interface.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "ui/ui_main.h"
#include "ui/clientwindow.h"
#include "sys_system.h"

#include <cmath>
#include <de/glstate.h>
#include <de/glinfo.h>
#include <doomsday/console/cmd.h>
#include <doomsday/filesys/fs_util.h>
//#include <doomsday/world/materials.h>

#include "api_fontrender.h"
#include "gl/gl_main.h"
#include "gl/gl_draw.h"
//#include "gl/texturecontent.h"
//#include "resource/image.h"
#include "render/rend_main.h"
#include "render/rend_font.h"
//#include "resource/materialanimator.h"

using namespace de;

fontid_t fontFixed;

/// Modify these colors to change the look of the UI.
static ui_color_t ui_colors[NUM_UI_COLORS] = {
    /* UIC_TEXT */      { .85f, .87f, 1 },
    /* UIC_TITLE */     { 1, 1, 1 },
};

const char *UI_ChooseFixedFont()
{
    if (DE_GAMEVIEW_WIDTH < 300) return "console11";
    if (DE_GAMEVIEW_WIDTH > 768) return "console18";
    return "console14";
}

static AbstractFont *loadSystemFont(const char *name)
{
    DE_ASSERT(name != 0 && name[0]);

    // Compose the resource name.
    res::Uri uri = res::makeUri("System:").setPath(name);

    // Compose the resource data path.
    ddstring_t resourcePath; Str_InitStd(&resourcePath);
    Str_Appendf(&resourcePath, "}data/Fonts/%s.dfn", name);
#if defined(UNIX) && !defined(MACOSX)
    // Case-sensitive file system.
    /// @todo Unkludge this: handle in a more generic manner.
    strlwr(resourcePath.str);
#endif
    F_ExpandBasePath(&resourcePath, &resourcePath);

    AbstractFont *font = ClientResources::get().newFontFromFile(uri, Str_Text(&resourcePath));
    if (!font)
    {
        App_Error("loadSystemFont: Failed loading font \"%s\".", name);
    }

    Str_Free(&resourcePath);
    return font;
}

static void loadFontIfNeeded(const char *uri, fontid_t *fid)
{
    *fid = NOFONTID;
    if (uri && uri[0])
    {
        try
        {
            FontManifest &manifest = ClientResources::get().fontManifest(res::makeUri(uri));
            if (manifest.hasResource())
            {
                *fid = fontid_t(manifest.uniqueId());
            }
        }
        catch (const Resources::MissingResourceManifestError &)
        {}
    }

    if (*fid == NOFONTID)
    {
        *fid = loadSystemFont(uri)->manifest().uniqueId();
    }
}

void UI_LoadFonts()
{
    if (novideo) return;

    loadFontIfNeeded(UI_ChooseFixedFont(), &fontFixed);
}

ui_color_t* UI_Color(uint id)
{
    if (id >= NUM_UI_COLORS)
        return NULL;
    return &ui_colors[id];
}

void UI_MixColors(ui_color_t* a, ui_color_t* b, ui_color_t* dest, float amount)
{
    dest->red   = (1 - amount) * a->red   + amount * b->red;
    dest->green = (1 - amount) * a->green + amount * b->green;
    dest->blue  = (1 - amount) * a->blue  + amount * b->blue;
}

void UI_SetColorA(ui_color_t* color, float alpha)
{
    DGL_Color4f(color->red, color->green, color->blue, alpha);
}

void UI_SetColor(ui_color_t* color)
{
    DGL_Color3f(color->red, color->green, color->blue);
}

void UI_TextOutEx2(const char* text, const Point2Raw* origin, ui_color_t* color, float alpha,
    int alignFlags, short textFlags)
{
    assert(origin);
    //alpha *= uiAlpha;
    if (alpha <= 0) return;
    FR_SetColorAndAlpha(color->red, color->green, color->blue, alpha);
    FR_DrawText3(text, origin, alignFlags, textFlags);
}

void UI_TextOutEx(const char* text, const Point2Raw* origin, ui_color_t* color, float alpha)
{
    UI_TextOutEx2(text, origin, color, alpha, DEFAULT_ALIGNFLAGS, DEFAULT_DRAWFLAGS);
}

void UI_DrawDDBackground(const Point2Raw &origin, const Size2Raw &dimensions, float alpha)
{
    //DGL_Disable(DGL_TEXTURE_2D);
    DGL_PushState();

    if (alpha < 1.0f)
    {
        GL_BlendMode(BM_NORMAL);
    }
    else
    {
        DGL_Disable(DGL_BLEND);
    }

    DGL_Color4f(0, 0, 0, alpha);
    GL_DrawRect2(origin.x, origin.y, dimensions.width, dimensions.height);

    DGL_PopState();
}
