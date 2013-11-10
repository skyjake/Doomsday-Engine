/** @file bitmapfont.cpp Composite bitmap font.
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
#include "resource/bitmapfont.h"

#include "api_resource.h" // R_GetPatchInfo
#include "dd_main.h" // App_ResourceSystem(), isDedicated
#include "sys_system.h" // novideo
#include "gl/gl_texmanager.h"

using namespace de;

CompositeBitmapFont::CompositeBitmapFont(fontid_t bindId)
    : AbstractFont(FT_BITMAPCOMPOSITE, bindId)
{
    _def = 0;
    std::memset(_chars, 0, sizeof(_chars));
    _flags |= FF_COLORIZE;
}

CompositeBitmapFont::~CompositeBitmapFont()
{
    glDeinit();
}

RectRaw const *CompositeBitmapFont::charGeometry(unsigned char chr)
{
    glInit();
    bitmapcompositefont_char_t *ch = &_chars[chr];
    return &ch->geometry;
}

int CompositeBitmapFont::charWidth(unsigned char ch)
{
    glInit();
    if(_chars[ch].geometry.size.width == 0) return _noCharSize.width;
    return _chars[ch].geometry.size.width;
}

int CompositeBitmapFont::charHeight(unsigned char ch)
{
    glInit();
    if(_chars[ch].geometry.size.height == 0) return _noCharSize.height;
    return _chars[ch].geometry.size.height;
}

static inline texturevariantspecification_t &charTextureSpec()
{
    return GL_TextureVariantSpec(TC_UI, TSF_MONOCHROME | TSF_UPSCALE_AND_SHARPEN,
                                 0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                 0, -3, 0, false, false, false, false);
}

void CompositeBitmapFont::glInit()
{
    Size2Raw avgSize;
    int numPatches;
    uint i;

    if(!_isDirty) return;
    if(novideo || isDedicated || BusyMode_Active()) return;

    glDeinit();

    avgSize.width = avgSize.height = 0;
    numPatches = 0;

    for(i = 0; i < MAX_CHARS; ++i)
    {
        bitmapcompositefont_char_t *ch = &_chars[i];
        patchid_t patch = ch->patch;
        patchinfo_t info;

        if(0 == patch) continue;

        R_GetPatchInfo(patch, &info);
        std::memcpy(&ch->geometry, &info.geometry, sizeof ch->geometry);

        ch->geometry.origin.x -= _marginWidth;
        ch->geometry.origin.y -= _marginHeight;
        ch->geometry.size.width  += _marginWidth  * 2;
        ch->geometry.size.height += _marginHeight * 2;
        ch->border = 0;

        ch->tex = App_ResourceSystem().textures()
                      .scheme("Patches").findByUniqueId(patch)
                          .texture().prepareVariant(charTextureSpec());
        if(ch->tex && ch->tex->source() == TEXS_ORIGINAL)
        {
            // Upscale & Sharpen will have been applied.
            ch->border = 1;
        }

        avgSize.width  += ch->geometry.size.width;
        avgSize.height += ch->geometry.size.height;
        ++numPatches;
    }

    if(numPatches)
    {
        avgSize.width  /= numPatches;
        avgSize.height /= numPatches;
    }

    _noCharSize.width  = avgSize.width;
    _noCharSize.height = avgSize.height;

    // We have prepared all patches.
    _isDirty = false;
}

void CompositeBitmapFont::glDeinit()
{
    if(novideo || isDedicated) return;

    _isDirty = true;
    if(BusyMode_Active()) return;

    for(int i = 0; i < 256; ++i)
    {
        bitmapcompositefont_char_t *ch = &_chars[i];
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

struct ded_compositefont_s *CompositeBitmapFont::definition() const
{
    return _def;
}

void CompositeBitmapFont::setDefinition(struct ded_compositefont_s *newDef)
{
    _def = newDef;
}

void CompositeBitmapFont::rebuildFromDef(ded_compositefont_t *def)
{
    LOG_AS("CompositeBitmapFont::rebuildFromDef");

    setDefinition(def);
    if(!def) return;

    for(int i = 0; i < def->charMapCount.num; ++i)
    {
        if(!def->charMap[i].path) continue;

        try
        {
            QByteArray path = reinterpret_cast<de::Uri &>(*def->charMap[i].path).resolved().toUtf8();
            charSetPatch(def->charMap[i].ch, path.constData());
        }
        catch(de::Uri::ResolveError const& er)
        {
            LOG_WARNING(er.asText());
        }
    }
}

Texture::Variant *CompositeBitmapFont::charTexture(unsigned char ch)
{
    glInit();
    return _chars[ch].tex;
}

patchid_t CompositeBitmapFont::charPatch(unsigned char ch)
{
    glInit();
    return _chars[ch].patch;
}

void CompositeBitmapFont::charSetPatch(unsigned char chr, char const *encodedPatchName)
{
    bitmapcompositefont_char_t *ch = &_chars[chr];
    ch->patch = App_ResourceSystem().declarePatch(encodedPatchName);
    _isDirty = true;
}

uint8_t CompositeBitmapFont::charBorder(unsigned char chr)
{
    bitmapcompositefont_char_t *ch = &_chars[chr];
    glInit();
    return ch->border;
}

void CompositeBitmapFont::charCoords(unsigned char /*chr*/, Point2Raw coords[4])
{
    if(!coords) return;

    glInit();

    // Top left.
    coords[0].x = 0;
    coords[0].y = 0;

    // Bottom right.
    coords[2].x = 1;
    coords[2].y = 1;

    // Top right.
    coords[1].x = 1;
    coords[1].y = 0;

    // Bottom left.
    coords[3].x = 0;
    coords[3].y = 1;
}
