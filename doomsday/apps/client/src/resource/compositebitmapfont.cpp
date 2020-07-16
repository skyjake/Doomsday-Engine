/** @file compositebitmapfont.cpp Composite bitmap font.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <doomsday/res/textures.h>

#include "api_resource.h" // R_GetPatchInfo
#include "dd_main.h" // App_Resources(), isDedicated
#include "sys_system.h" // novideo
#include "gl/gl_texmanager.h" // GL_TextureVariantSpec

using namespace de;

DE_PIMPL(CompositeBitmapFont)
{
    ded_compositefont_t *def; /// Definition on which "this" font is derived (if any).
    bool needGLInit;

    /// Font metrics.
    int leading;
    int ascent;
    int descent;

    Glyph glyphs[MAX_CHARS];
    Glyph missingGlyph;

    Impl(Public *i)
        : Base(i)
        , def(0)
        , needGLInit(true)
        , leading(0)
        , ascent(0)
        , descent(0)
    {
        zap(glyphs);
        zap(missingGlyph);
        self()._flags |= AbstractFont::Colorize;
    }

    ~Impl()
    {
        self().glDeinit();
    }

    Glyph &glyph(dbyte ch)
    {
        //if(ch >= MAX_CHARS) return missingGlyph;
        if(!glyphs[ch].haveSourceImage) return missingGlyph;
        return glyphs[ch];
    }
};

CompositeBitmapFont::CompositeBitmapFont(FontManifest &manifest)
    : AbstractFont(manifest), d(new Impl(this))
{}

int CompositeBitmapFont::ascent() const
{
    glInit();
    return d->ascent;
}

int CompositeBitmapFont::descent() const
{
    glInit();
    return d->descent;
}

int CompositeBitmapFont::lineSpacing() const
{
    glInit();
    return d->leading;
}

const Rectanglei &CompositeBitmapFont::glyphPosCoords(dbyte ch) const
{
    glInit();
    return d->glyph(ch).geometry;
}

const Rectanglei &CompositeBitmapFont::glyphTexCoords(dbyte /*ch*/) const
{
    static Rectanglei coords(Vec2i(0, 0), Vec2i(1, 1));
    glInit();
    return coords;
}

uint CompositeBitmapFont::glyphTextureBorder(dbyte ch) const
{
    glInit();
    return d->glyph(ch).border;
}

TextureVariant *CompositeBitmapFont::glyphTexture(dbyte ch) const
{
    glInit();
    return d->glyph(ch).tex;
}

patchid_t CompositeBitmapFont::glyphPatch(dbyte ch) const
{
    glInit();
    return d->glyph(ch).patch;
}

void CompositeBitmapFont::glyphSetPatch(dbyte ch, String encodedPatchName)
{
    //if(ch >= MAX_CHARS) return;
    d->glyphs[ch].patch = res::Textures::get().declarePatch(encodedPatchName);

    // We'll need to rebuild the prepared GL resources.
    d->needGLInit = true;
}

/// @todo fixme: Do not assume the texture-usage context is @c TC_UI.
static const TextureVariantSpec &glyphTextureSpec()
{
    return ClientApp::resources().textureSpec(TC_UI,
        TSF_MONOCHROME | TSF_UPSCALE_AND_SHARPEN, 0, 0, 0, GL_CLAMP_TO_EDGE,
        GL_CLAMP_TO_EDGE, 0, -3, 0, false, false, false, false);
}

void CompositeBitmapFont::glInit() const
{
    if(!d->needGLInit) return;
    if(novideo || BusyMode_Active()) return;

    LOG_AS("resource/compositebitmapfont.h");

    glDeinit();

    int foundGlyphs = 0;
    Vec2ui avgSize;
    for(int i = 0; i < MAX_CHARS; ++i)
    {
        Glyph *ch = &d->glyphs[i];
        patchid_t patch = ch->patch;

        ch->haveSourceImage = patch != 0;
        if(!ch->haveSourceImage) continue;

        try
        {
            res::Texture &tex = res::Textures::get().textureScheme("Patches").findByUniqueId(patch).texture();

            ch->tex      = static_cast<ClientTexture &>(tex).prepareVariant(glyphTextureSpec());
            ch->geometry = Rectanglei::fromSize(tex.origin(), tex.dimensions().toVec2ui());

            ch->border   = 0;
            if(ch->tex && ch->tex->source() == res::Original)
            {
                // Upscale & Sharpen will have been applied.
                ch->border = 1;
            }

            avgSize += ch->geometry.size();
            ++foundGlyphs;
        }
        catch(const res::TextureManifest::MissingTextureError &er)
        {
            // Log but otherwise ignore this error.
            LOG_RES_WARNING(er.asText() + ", ignoring.");
        }
        catch(const res::TextureScheme::NotFoundError &er)
        {
            // Log but otherwise ignore this error.
            LOG_RES_WARNING(er.asText() + ", ignoring.");
        }
    }

    d->missingGlyph.geometry.setSize(avgSize / (foundGlyphs? foundGlyphs : 1));

    // We have prepared all patches.
    d->needGLInit = false;
}

void CompositeBitmapFont::glDeinit() const
{
    if(novideo) return;

    d->needGLInit = true;
    if(BusyMode_Active()) return;

    for(int i = 0; i < 256; ++i)
    {
        Glyph *ch = &d->glyphs[i];
        if(!ch->tex) continue;
        ch->tex->release();
        ch->tex = 0;
    }
}

CompositeBitmapFont *CompositeBitmapFont::fromDef(FontManifest &manifest,
    const ded_compositefont_t &def) // static
{
    LOG_AS("CompositeBitmapFont::fromDef");

    CompositeBitmapFont *font = new CompositeBitmapFont(manifest);
    font->setDefinition(const_cast<ded_compositefont_t *>(&def));

    for(int i = 0; i < def.charMap.size(); ++i)
    {
        if(!def.charMap[i].path) continue;
        try
        {
            String glyphPatchPath = def.charMap[i].path->resolved();
            font->glyphSetPatch(def.charMap[i].ch, glyphPatchPath);
        }
        catch(const res::Uri::ResolveError &er)
        {
            LOG_RES_WARNING(er.asText());
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

void CompositeBitmapFont::rebuildFromDef(const ded_compositefont_t &newDef)
{
    LOG_AS("CompositeBitmapFont::rebuildFromDef");

    setDefinition(const_cast<ded_compositefont_t *>(&newDef));
    if(!d->def) return;

    for(int i = 0; i < d->def->charMap.size(); ++i)
    {
        if(!d->def->charMap[i].path) continue;

        try
        {
            String glyphPatchPath = d->def->charMap[i].path->resolved();
            glyphSetPatch(d->def->charMap[i].ch, glyphPatchPath);
        }
        catch(res::Uri::ResolveError const& er)
        {
            LOG_RES_WARNING(er.asText());
        }
    }
}
