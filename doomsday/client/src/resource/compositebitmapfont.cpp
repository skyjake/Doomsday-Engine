/** @file compositebitmapfont.cpp Composite bitmap font.
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
#include "resource/compositebitmapfont.h"

#include "api_resource.h" // R_GetPatchInfo
#include "dd_main.h" // App_ResourceSystem(), isDedicated
#include "sys_system.h" // novideo
#include "gl/gl_texmanager.h" // GL_TextureVariantSpec

using namespace de;

DENG2_PIMPL(CompositeBitmapFont)
{
    ded_compositefont_t *def; /// Definition on which "this" font is derived (if any).
    bool needGLInit;

    /// Font metrics.
    int leading;
    int ascent;
    int descent;

    Glyph glyphs[MAX_CHARS];
    Glyph missingGlyph;

    Instance(Public *i)
        : Base(i)
        , def(0)
        , needGLInit(true)
        , leading(0)
        , ascent(0)
        , descent(0)
    {
        zap(glyphs);
        zap(missingGlyph);
        self._flags |= AbstractFont::Colorize;
    }

    ~Instance()
    {
        self.glDeinit();
    }

    Glyph &glyph(uchar ch)
    {
        if(ch >= MAX_CHARS) return missingGlyph;
        if(!glyphs[ch].haveSourceImage) return missingGlyph;
        return glyphs[ch];
    }
};

CompositeBitmapFont::CompositeBitmapFont(FontManifest &manifest)
    : AbstractFont(manifest), d(new Instance(this))
{}

int CompositeBitmapFont::ascent()
{
    glInit();
    return d->ascent;
}

int CompositeBitmapFont::descent()
{
    glInit();
    return d->descent;
}

int CompositeBitmapFont::lineSpacing()
{
    glInit();
    return d->leading;
}

Rectanglei const &CompositeBitmapFont::glyphPosCoords(uchar ch)
{
    glInit();
    return d->glyph(ch).geometry;
}

Rectanglei const &CompositeBitmapFont::glyphTexCoords(uchar /*ch*/)
{
    static Rectanglei coords(Vector2i(0, 0), Vector2i(1, 1));
    glInit();
    return coords;
}

uint CompositeBitmapFont::glyphTextureBorder(uchar ch)
{
    glInit();
    return d->glyph(ch).border;
}

TextureVariant *CompositeBitmapFont::glyphTexture(uchar ch)
{
    glInit();
    return d->glyph(ch).tex;
}

patchid_t CompositeBitmapFont::glyphPatch(uchar ch)
{
    glInit();
    return d->glyph(ch).patch;
}

void CompositeBitmapFont::glyphSetPatch(uchar ch, String encodedPatchName)
{
    if(ch >= MAX_CHARS) return;
    d->glyphs[ch].patch = App_ResourceSystem().declarePatch(encodedPatchName);

    // We'll need to rebuild the prepared GL resources.
    d->needGLInit = true;
}

/// @todo fixme: Do not assume the texture-usage context is @c TC_UI.
static TextureVariantSpec &glyphTextureSpec()
{
    return ClientApp::resourceSystem().textureSpec(TC_UI,
        TSF_MONOCHROME | TSF_UPSCALE_AND_SHARPEN, 0, 0, 0, GL_CLAMP_TO_EDGE,
        GL_CLAMP_TO_EDGE, 0, -3, 0, false, false, false, false);
}

void CompositeBitmapFont::glInit()
{
    if(!d->needGLInit) return;
    if(novideo || isDedicated || BusyMode_Active()) return;

    glDeinit();

    int foundGlyphs = 0;
    Vector2ui avgSize;
    for(int i = 0; i < MAX_CHARS; ++i)
    {
        Glyph *ch = &d->glyphs[i];
        patchid_t patch = ch->patch;

        ch->haveSourceImage = patch != 0;
        if(!ch->haveSourceImage) continue;

        patchinfo_t info;
        R_GetPatchInfo(patch, &info);
        ch->tex    = App_ResourceSystem().textures()
                        .scheme("Patches").findByUniqueId(patch)
                            .texture().prepareVariant(glyphTextureSpec());
        ch->border = 0;
        if(ch->tex && ch->tex->source() == res::Original)
        {
            // Upscale & Sharpen will have been applied.
            ch->border = 1;
        }

        ch->geometry = Rectanglei::fromSize(Vector2i(info.geometry.origin.xy),
                                            Vector2ui(info.geometry.size.width, info.geometry.size.height));

        avgSize += ch->geometry.size();
        ++foundGlyphs;
    }

    d->missingGlyph.geometry.setSize(avgSize / (foundGlyphs? foundGlyphs : 1));

    // We have prepared all patches.
    d->needGLInit = false;
}

void CompositeBitmapFont::glDeinit()
{
    if(novideo || isDedicated) return;

    d->needGLInit = true;
    if(BusyMode_Active()) return;

    for(int i = 0; i < 256; ++i)
    {
        Glyph *ch = &d->glyphs[i];
        if(!ch->tex) continue;
        App_ResourceSystem().releaseGLTexturesFor(*ch->tex);
        ch->tex = 0;
    }
}

CompositeBitmapFont *CompositeBitmapFont::fromDef(FontManifest &manifest,
    ded_compositefont_t const &def) // static
{
    LOG_AS("CompositeBitmapFont::fromDef");

    CompositeBitmapFont *font = new CompositeBitmapFont(manifest);
    font->setDefinition(const_cast<ded_compositefont_t *>(&def));

    for(int i = 0; i < def.charMapCount.num; ++i)
    {
        if(!def.charMap[i].path) continue;
        try
        {
            String glyphPatchPath = reinterpret_cast<de::Uri &>(*def.charMap[i].path).resolved();
            font->glyphSetPatch(def.charMap[i].ch, glyphPatchPath);
        }
        catch(de::Uri::ResolveError const &er)
        {
            LOG_WARNING(er.asText());
        }
    }

    // Lets try to prepare it right away.
    font->glInit();
    return font;
}

ded_compositefont_t *CompositeBitmapFont::definition() const
{
    return d->def;
}

void CompositeBitmapFont::setDefinition(ded_compositefont_t *newDef)
{
    d->def = newDef;
}

void CompositeBitmapFont::rebuildFromDef(ded_compositefont_t const &newDef)
{
    LOG_AS("CompositeBitmapFont::rebuildFromDef");

    setDefinition(const_cast<ded_compositefont_t *>(&newDef));
    if(!d->def) return;

    for(int i = 0; i < d->def->charMapCount.num; ++i)
    {
        if(!d->def->charMap[i].path) continue;

        try
        {
            String glyphPatchPath = reinterpret_cast<de::Uri &>(*d->def->charMap[i].path).resolved();
            glyphSetPatch(d->def->charMap[i].ch, glyphPatchPath);
        }
        catch(de::Uri::ResolveError const& er)
        {
            LOG_WARNING(er.asText());
        }
    }
}
