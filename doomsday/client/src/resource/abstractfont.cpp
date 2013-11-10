/** @file abstractfont.cpp Abstract font.
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

#include "resource/abstractfont.h"

using namespace de;

font_t::font_t(fonttype_t type, fontid_t bindId)
{
    DENG_ASSERT(VALID_FONTTYPE(type));

    _type = type;
    _marginWidth  = 0;
    _marginHeight = 0;
    _leading = 0;
    _ascent = 0;
    _descent = 0;
    _noCharSize.width  = 0;
    _noCharSize.height = 0;
    _primaryBind = bindId;
    _isDirty = true;
}

void font_t::glInit()
{}

void font_t::glDeinit()
{}

void font_t::charSize(Size2Raw *size, unsigned char ch)
{
    if(!size) return;
    size->width  = charWidth(ch);
    size->height = charHeight(ch);
}

fonttype_t font_t::type() const
{
    return _type;
}

fontid_t font_t::primaryBind() const
{
    return _primaryBind;
}

void font_t::setPrimaryBind(fontid_t bindId)
{
    _primaryBind = bindId;
}

int font_t::flags() const
{
    return _flags;
}

int font_t::ascent()
{
    glInit();
    return _ascent;
}

int font_t::descent()
{
    glInit();
    return _descent;
}

int font_t::lineSpacing()
{
    glInit();
    return _leading;
}

