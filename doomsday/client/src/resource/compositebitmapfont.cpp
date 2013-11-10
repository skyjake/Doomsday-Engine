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

#include "de_platform.h"
#include "resource/compositebitmapfont.h"

#include "api_resource.h" // R_GetPatchInfo
#include "dd_main.h" // App_ResourceSystem(), isDedicated
#include "sys_system.h" // novideo
#include "gl/gl_texmanager.h"

using namespace de;

DENG2_PIMPL(CompositeBitmapFont)
{
    ded_compositefont_t *def; /// Definition on which "this" font is derived (if any).
    bool needGLInit;

    /// Character map.
    bitmapcompositefont_char_t chars[MAX_CHARS];

    Instance(Public *i)
        : Base(i)
        , def(0)
        , needGLInit(true)
    {
        zap(chars);
        self._flags |= FF_COLORIZE;
    }

    ~Instance()
    {
        self.glDeinit();
    }
};

CompositeBitmapFont::CompositeBitmapFont(fontid_t bindId)
    : AbstractFont(), d(new Instance(this))
{
    setPrimaryBind(bindId);
}

Rectanglei const &CompositeBitmapFont::charGeometry(uchar chr)
{
    glInit();
    return d->chars[chr].geometry;
}

int CompositeBitmapFont::charWidth(uchar ch)
{
    glInit();
    if(d->chars[ch].geometry.width() == 0) return _noCharSize.x;
    return d->chars[ch].geometry.width();
}

int CompositeBitmapFont::charHeight(uchar ch)
{
    glInit();
    if(d->chars[ch].geometry.height() == 0) return _noCharSize.y;
    return d->chars[ch].geometry.height();
}

static texturevariantspecification_t &charTextureSpec()
{
    return GL_TextureVariantSpec(TC_UI, TSF_MONOCHROME | TSF_UPSCALE_AND_SHARPEN,
                                 0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                 0, -3, 0, false, false, false, false);
}

void CompositeBitmapFont::glInit()
{
    if(!d->needGLInit) return;
    if(novideo || isDedicated || BusyMode_Active()) return;

    glDeinit();

    int numPatches = 0;
    Vector2ui avgSize;
    for(int i = 0; i < MAX_CHARS; ++i)
    {
        bitmapcompositefont_char_t *ch = &d->chars[i];
        patchid_t patch = ch->patch;
        patchinfo_t info;

        if(0 == patch) continue;

        R_GetPatchInfo(patch, &info);
        ch->geometry.topLeft = Vector2i(info.geometry.origin.xy) - Vector2i(_margin.x, _margin.y);
        ch->geometry.setSize(_margin * 2 +
                             Vector2ui(info.geometry.size.width, info.geometry.size.height));
        ch->border = 0;
        ch->tex    = App_ResourceSystem().textures()
                        .scheme("Patches").findByUniqueId(patch)
                            .texture().prepareVariant(charTextureSpec());
        if(ch->tex && ch->tex->source() == TEXS_ORIGINAL)
        {
            // Upscale & Sharpen will have been applied.
            ch->border = 1;
        }

        avgSize += ch->geometry.size();
        ++numPatches;
    }

    if(numPatches)
    {
        avgSize /= numPatches;
    }
    _noCharSize = avgSize;

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
        bitmapcompositefont_char_t *ch = &d->chars[i];
        if(!ch->tex) continue;
        GL_ReleaseVariantTexture(*ch->tex);
        ch->tex = 0;
    }
}

CompositeBitmapFont *CompositeBitmapFont::fromDef(fontid_t bindId, ded_compositefont_t *def) // static
{
    DENG2_ASSERT(def != 0);

    LOG_AS("CompositeBitmapFont::fromDef");

    CompositeBitmapFont *font = new CompositeBitmapFont(bindId);
    font->setDefinition(def);

    for(int i = 0; i < def->charMapCount.num; ++i)
    {
        if(!def->charMap[i].path) continue;
        try
        {
            QByteArray path = reinterpret_cast<de::Uri &>(*def->charMap[i].path).resolved().toUtf8();
            font->charSetPatch(def->charMap[i].ch, path.constData());
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

void CompositeBitmapFont::rebuildFromDef(ded_compositefont_t *newDef)
{
    LOG_AS("CompositeBitmapFont::rebuildFromDef");

    setDefinition(newDef);
    if(!newDef) return;

    for(int i = 0; i < newDef->charMapCount.num; ++i)
    {
        if(!newDef->charMap[i].path) continue;

        try
        {
            QByteArray path = reinterpret_cast<de::Uri &>(*newDef->charMap[i].path).resolved().toUtf8();
            charSetPatch(newDef->charMap[i].ch, path.constData());
        }
        catch(de::Uri::ResolveError const& er)
        {
            LOG_WARNING(er.asText());
        }
    }
}

TextureVariant *CompositeBitmapFont::charTexture(uchar ch)
{
    glInit();
    return d->chars[ch].tex;
}

patchid_t CompositeBitmapFont::charPatch(uchar ch)
{
    glInit();
    return d->chars[ch].patch;
}

void CompositeBitmapFont::charSetPatch(uchar chr, char const *encodedPatchName)
{
    bitmapcompositefont_char_t *ch = &d->chars[chr];
    ch->patch = App_ResourceSystem().declarePatch(encodedPatchName);
    d->needGLInit = true;
}

uint8_t CompositeBitmapFont::charBorder(uchar chr)
{
    bitmapcompositefont_char_t *ch = &d->chars[chr];
    glInit();
    return ch->border;
}

void CompositeBitmapFont::charCoords(uchar /*chr*/, Vector2i coords[4])
{
    if(!coords) return;

    glInit();

    coords[0] = Vector2i(0, 0); // Top left.
    coords[2] = Vector2i(1, 1); // Bottom right.
    coords[1] = Vector2i(1, 0); // Top right.
    coords[3] = Vector2i(0, 1); // Bottom left.
}
