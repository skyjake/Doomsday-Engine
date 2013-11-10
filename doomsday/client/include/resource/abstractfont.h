/** @file abstractfont.h  Abstract font.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENT_RESOURCE_ABSTRACTFONT_H
#define CLIENT_RESOURCE_ABSTRACTFONT_H

#include "dd_types.h"
#include "def_main.h"

// Font types.
typedef enum {
    FT_FIRST = 0,
    FT_BITMAP = FT_FIRST,
    FT_BITMAPCOMPOSITE,
    FT_LAST = FT_BITMAPCOMPOSITE
} fonttype_t;

#define VALID_FONTTYPE(v)       ((v) >= FT_FIRST && (v) <= FT_LAST)

/**
 * @defgroup fontFlags  Font Flags
 * @ingroup flags
 */
/*@{*/
#define FF_COLORIZE             0x1 /// Font can be colored.
#define FF_SHADOWED             0x2 /// Font has an embedded shadow.
/*@}*/

#define MAX_CHARS               256 // Normal 256 ANSI characters.

/**
 * Abstract font resource.
 *
 * @ingroup resource
 */
class AbstractFont
{
public:
    fonttype_t _type;

    /// @c true= Font requires a full update.
    bool _isDirty;

    /// @ref fontFlags.
    int _flags;

    /// Unique identifier of the primary binding in the owning collection.
    fontid_t _primaryBind;

    /// Font metrics.
    int _leading;
    int _ascent;
    int _descent;

    Size2Raw _noCharSize;

    /// Do fonts have margins? Is this a pixel border in the composited character
    /// map texture (perhaps per-glyph)?
    int _marginWidth, _marginHeight;

    AbstractFont(fonttype_t type = FT_FIRST, fontid_t bindId = 0);
    virtual ~AbstractFont() {}

    DENG2_AS_IS_METHODS()

    fonttype_t type() const;

    fontid_t primaryBind() const;

    void setPrimaryBind(fontid_t bindId);

    /// @return  @ref fontFlags
    int flags() const;

    int ascent();
    int descent();
    int lineSpacing();

    virtual void glInit();
    virtual void glDeinit();

    virtual RectRaw const *charGeometry(unsigned char ch) = 0;
    virtual int charWidth(unsigned char ch) = 0;
    virtual int charHeight(unsigned char ch) = 0;

    void charSize(Size2Raw *size, unsigned char ch);
};

#endif // CLIENT_RESOURCE_ABSTRACTFONT_H
