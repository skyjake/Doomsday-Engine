/** @file ui_main.cpp Graphical User Interface
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_ui.h"
#include "de_misc.h"

#include "resource/image.h"
#include "gl/texturecontent.h"
#include "render/rend_main.h"
#include "render/rend_font.h"
#include "MaterialSnapshot"
#include "Material"

#include "ui/ui_main.h"

using namespace de;

D_CMD(UIColor);

fontid_t fontFixed, fontVariable[FONTSTYLE_COUNT];
static int uiFontHgt; /// Height of the UI font.

/// Modify these colors to change the look of the UI.
static ui_color_t ui_colors[NUM_UI_COLORS] = {
    /* UIC_TEXT */      { .85f, .87f, 1 },
    /* UIC_TITLE */     { 1, 1, 1 },
    /* UIC_SHADOW */    { 0, 0, 0 },
    /* UIC_BG_LIGHT */  { .18f, .18f, .22f },
    /* UIC_BG_MEDIUM */ { .4f, .4f, .52f },
    /* UIC_BG_DARK */   { .28f, .28f, .33f },
    /* UIC_BRD_HI */    { 1, 1, 1 },
    /* UIC_BRD_MED */   { 0, 0, 0 },
    /* UIC_BRD_LOW */   { .25f, .25f, .55f },
    /* UIC_HELP */      { .4f, .4f, .52f }
};

void UI_Register(void)
{
    C_CMD_FLAGS("uicolor", "sfff", UIColor, CMDF_NO_DEDICATED);
}

char const *UI_ChooseFixedFont()
{
    if(DENG_GAMEVIEW_WIDTH < 300) return "console11";
    if(DENG_GAMEVIEW_WIDTH > 768) return "console18";
    return "console14";
}

char const *UI_ChooseVariableFont(fontstyle_t style)
{
    int const resY = DENG_GAMEVIEW_HEIGHT;
    int const SMALL_LIMIT = 500;
    int const MED_LIMIT   = 800;

    switch(style)
    {
    default:
        return (resY < SMALL_LIMIT ? "normal12" :
                resY < MED_LIMIT   ? "normal18" :
                                     "normal24");

    case FS_LIGHT:
        return (resY < SMALL_LIMIT ? "normallight12" :
                resY < MED_LIMIT   ? "normallight18" :
                                     "normallight24");

    case FS_BOLD:
        return (resY < SMALL_LIMIT ? "normalbold12" :
                resY < MED_LIMIT   ? "normalbold18" :
                                     "normalbold24");
    }
}

static AbstractFont *loadSystemFont(char const *name)
{
    DENG2_ASSERT(name != 0 && name[0]);

    // Compose the resource name.
    de::Uri uri = de::Uri("System:", RC_NULL).setPath(name);

    // Compose the resource data path.
    ddstring_t resourcePath; Str_InitStd(&resourcePath);
    Str_Appendf(&resourcePath, "}data/Fonts/%s.dfn", name);
#if defined(UNIX) && !defined(MACOSX)
    // Case-sensitive file system.
    /// @todo Unkludge this: handle in a more generic manner.
    strlwr(resourcePath.str);
#endif
    F_ExpandBasePath(&resourcePath, &resourcePath);

    AbstractFont *font = App_ResourceSystem().newFontFromFile(uri, Str_Text(&resourcePath));
    if(!font)
    {
        Con_Error("loadSystemFont: Failed loading font \"%s\".", name);
        exit(1); // Unreachable.
    }

    Str_Free(&resourcePath);
    return font;
}

static void loadFontIfNeeded(char const *uri, fontid_t *fid)
{
    *fid = NOFONTID;
    if(uri && uri[0])
    {
        try
        {
            FontManifest &manifest = App_ResourceSystem().fontManifest(de::Uri(uri, RC_NULL));
            if(manifest.hasResource())
            {
                *fid = fontid_t(manifest.uniqueId());
            }
        }
        catch(ResourceSystem::MissingManifestError const &)
        {}
    }

    if(*fid == NOFONTID)
    {
        *fid = loadSystemFont(uri)->manifest().uniqueId();
    }
}

void UI_LoadFonts()
{
    if(isDedicated) return;

    loadFontIfNeeded(UI_ChooseFixedFont(),             &fontFixed);
    loadFontIfNeeded(UI_ChooseVariableFont(FS_NORMAL), &fontVariable[FS_NORMAL]);
    loadFontIfNeeded(UI_ChooseVariableFont(FS_BOLD),   &fontVariable[FS_BOLD]);
    loadFontIfNeeded(UI_ChooseVariableFont(FS_LIGHT),  &fontVariable[FS_LIGHT]);
}

de::MaterialVariantSpec const &UI_MaterialSpec(int texSpecFlags)
{
    return App_ResourceSystem().materialSpec(UiContext, texSpecFlags | TSF_NO_COMPRESSION,
                                             0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                             1, 1, 0, false, false, false, false);
}

int UI_FontHeight(void)
{
    return uiFontHgt;
}

ui_color_t* UI_Color(uint id)
{
    if(id >= NUM_UI_COLORS)
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
    glColor4f(color->red, color->green, color->blue, alpha);
}

void UI_SetColor(ui_color_t* color)
{
    glColor3f(color->red, color->green, color->blue);
}

void UI_Gradient(const Point2Raw* origin, const Size2Raw* size, ui_color_t* topColor,
    ui_color_t* bottomColor, float topAlpha, float bottomAlpha)
{
    UI_GradientEx(origin, size, 0, topColor, bottomColor, topAlpha, bottomAlpha);
}

void UI_GradientEx(const Point2Raw* origin, const Size2Raw* size, int border, ui_color_t* topColor,
    ui_color_t* bottomColor, float topAlpha, float bottomAlpha)
{
    UI_DrawRectEx(origin, size, border, true, topColor, bottomColor, topAlpha, bottomAlpha);
}

void UI_TextOutEx2(const char* text, const Point2Raw* origin, ui_color_t* color, float alpha,
    int alignFlags, short textFlags)
{
    assert(origin);
    //alpha *= uiAlpha;
    if(alpha <= 0) return;
    FR_SetColorAndAlpha(color->red, color->green, color->blue, alpha);
    FR_DrawText3(text, origin, alignFlags, textFlags);
}

void UI_TextOutEx(const char* text, const Point2Raw* origin, ui_color_t* color, float alpha)
{
    UI_TextOutEx2(text, origin, color, alpha, DEFAULT_ALIGNFLAGS, DEFAULT_DRAWFLAGS);
}

void UI_DrawRectEx(const Point2Raw* origin, const Size2Raw* size, int border, dd_bool filled,
    ui_color_t* topColor, ui_color_t* bottomColor, float alpha, float bottomAlpha)
{
    float s[2] = { 0, 1 }, t[2] = { 0, 1 };
    assert(origin && size);

    //alpha *= uiAlpha;
    //bottomAlpha *= uiAlpha;
    if(alpha <= 0 && bottomAlpha <= 0) return;

    if(border < 0)
    {
        border = -border;
        s[0] = t[0] = 1;
        s[1] = t[1] = 0;
    }
    if(bottomAlpha < 0)
        bottomAlpha = alpha;
    if(!bottomColor)
        bottomColor = topColor;

    // The fill comes first, if there's one.
    if(filled)
    {
        MaterialSnapshot const &ms =
            App_ResourceSystem().material(de::Uri("System", Path("ui/boxfill")))
                .prepare(UI_MaterialSpec());
        GL_BindTexture(&ms.texture(MTU_PRIMARY));

        glBegin(GL_QUADS);
        glTexCoord2f(0.5f, 0.5f);
        UI_SetColorA(topColor, alpha);
        glVertex2f(origin->x + border, origin->y + border);
        glVertex2f(origin->x + size->width - border, origin->y + border);
        UI_SetColorA(bottomColor, bottomAlpha);
        glVertex2f(origin->x + size->width - border, origin->y + size->height - border);
        glVertex2f(origin->x + border, origin->y + size->height - border);
    }
    else
    {
        MaterialSnapshot const &ms =
            App_ResourceSystem().material(de::Uri("System", Path("ui/boxcorner")))
                .prepare(UI_MaterialSpec());
        GL_BindTexture(&ms.texture(MTU_PRIMARY));

        glBegin(GL_QUADS);
    }

    if(!filled || border > 0)
    {
        // Top Left.
        UI_SetColorA(topColor, alpha);
        glTexCoord2f(s[0], t[0]);
        glVertex2f(origin->x, origin->y);
        glTexCoord2f(0.5f, t[0]);
        glVertex2f(origin->x + border, origin->y);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + border, origin->y + border);
        glTexCoord2f(s[0], 0.5f);
        glVertex2f(origin->x, origin->y + border);
        // Top.
        glTexCoord2f(0.5f, t[0]);
        glVertex2f(origin->x + border, origin->y);
        glTexCoord2f(0.5f, t[0]);
        glVertex2f(origin->x + size->width - border, origin->y);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + size->width - border, origin->y + border);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + border, origin->y + border);
        // Top Right.
        glTexCoord2f(0.5f, t[0]);
        glVertex2f(origin->x + size->width - border, origin->y);
        glTexCoord2f(s[1], t[0]);
        glVertex2f(origin->x + size->width, origin->y);
        glTexCoord2f(s[1], 0.5f);
        glVertex2f(origin->x + size->width, origin->y + border);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + size->width - border, origin->y + border);
        // Right.
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + size->width - border, origin->y + border);
        glTexCoord2f(s[1], 0.5f);
        glVertex2f(origin->x + size->width, origin->y + border);
        UI_SetColorA(bottomColor, bottomAlpha);
        glTexCoord2f(s[1], 0.5f);
        glVertex2f(origin->x + size->width, origin->y + size->height - border);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + size->width - border, origin->y + size->height - border);
        // Bottom Right.
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + size->width - border, origin->y + size->height - border);
        glTexCoord2f(s[1], 0.5f);
        glVertex2f(origin->x + size->width, origin->y + size->height - border);
        glTexCoord2f(s[1], t[1]);
        glVertex2f(origin->x + size->width, origin->y + size->height);
        glTexCoord2f(0.5f, t[1]);
        glVertex2f(origin->x + size->width - border, origin->y + size->height);
        // Bottom.
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + border, origin->y + size->height - border);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + size->width - border, origin->y + size->height - border);
        glTexCoord2f(0.5f, t[1]);
        glVertex2f(origin->x + size->width - border, origin->y + size->height);
        glTexCoord2f(0.5f, t[1]);
        glVertex2f(origin->x + border, origin->y + size->height);
        // Bottom Left.
        glTexCoord2f(s[0], 0.5f);
        glVertex2f(origin->x, origin->y + size->height - border);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + border, origin->y + size->height - border);
        glTexCoord2f(0.5f, t[1]);
        glVertex2f(origin->x + border, origin->y + size->height);
        glTexCoord2f(s[0], t[1]);
        glVertex2f(origin->x, origin->y + size->height);
        // Left.
        UI_SetColorA(topColor, alpha);
        glTexCoord2f(s[0], 0.5f);
        glVertex2f(origin->x, origin->y + border);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + border, origin->y + border);
        UI_SetColorA(bottomColor, bottomAlpha);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(origin->x + border, origin->y + size->height - border);
        glTexCoord2f(s[0], 0.5f);
        glVertex2f(origin->x, origin->y + size->height - border);
    }
    glEnd();
}

void UI_DrawDDBackground(Point2Raw const &origin, Size2Raw const &dimensions, float alpha)
{
    ui_color_t const *dark  = UI_Color(UIC_BG_DARK);
    ui_color_t const *light = UI_Color(UIC_BG_LIGHT);
    float const mul = 1.5f;

    // Background gradient picture.
    MaterialSnapshot const &ms =
        App_ResourceSystem().material(de::Uri("System", Path("ui/background")))
            .prepare(UI_MaterialSpec(TSF_MONOCHROME));
    GL_BindTexture(&ms.texture(MTU_PRIMARY));

    glEnable(GL_TEXTURE_2D);
    if(alpha < 1.0)
    {
        GL_BlendMode(BM_NORMAL);
    }
    else
    {
        glDisable(GL_BLEND);
    }

    glBegin(GL_QUADS);
        // Top color.
        glColor4f(dark->red * mul, dark->green * mul, dark->blue * mul, alpha);
        glTexCoord2f(0, 0);
        glVertex2f(origin.x, origin.y);
        glTexCoord2f(1, 0);
        glVertex2f(origin.x + dimensions.width, origin.y);

        // Bottom color.
        glColor4f(light->red * mul, light->green * mul, light->blue * mul, alpha);
        glTexCoord2f(1, 1);
        glVertex2f(origin.x + dimensions.width, origin.y + dimensions.height);
        glTexCoord2f(0, 1);
        glVertex2f(0, origin.y + dimensions.height);
    glEnd();

    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

/**
 * CCmd: Change the UI colors.
 */
D_CMD(UIColor)
{
    DENG2_UNUSED2(argc, src);

    struct colorname_s {
        char const *str;
        uint colorIdx;
    } colors[] =
    {
        { "text",       UIC_TEXT },
        { "title",      UIC_TITLE },
        { "shadow",     UIC_SHADOW },
        { "bglight",    UIC_BG_LIGHT },
        { "bgmed",      UIC_BG_MEDIUM },
        { "bgdark",     UIC_BG_DARK },
        { "borhigh",    UIC_BRD_HI },
        { "bormed",     UIC_BRD_MED },
        { "borlow",     UIC_BRD_LOW },
        { "help",       UIC_HELP },
        { 0, 0 }
    };
    for(int i = 0; colors[i].str; ++i)
    {
        if(stricmp(argv[1], colors[i].str)) continue;

        uint idx = colors[i].colorIdx;
        ui_colors[idx].red   = strtod(argv[2], 0);
        ui_colors[idx].green = strtod(argv[3], 0);
        ui_colors[idx].blue  = strtod(argv[4], 0);
        return true;
    }

    LOG_SCR_ERROR("Unknown UI color '%s'") << argv[1];
    return false;
}
