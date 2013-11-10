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

#include "dd_types.h" // fontid_t
#include <de/Rectangle>
#include <de/Vector>

#define MAX_CHARS               256 // Normal 256 ANSI characters.

/**
 * @defgroup fontFlags  Font Flags
 * @ingroup flags
 */
/*@{*/
#define FF_COLORIZE             0x1 /// Font can be colored.
#define FF_SHADOWED             0x2 /// Font has an embedded shadow.
/*@}*/

/**
 * Abstract font resource.
 *
 * @ingroup resource
 */
class AbstractFont
{
public:
    /// @ref fontFlags.
    int _flags;

    /// Unique identifier of the primary binding in the owning collection.
    fontid_t _primaryBind;

    /// Font metrics.
    int _leading;
    int _ascent;
    int _descent;

    de::Vector2ui _noCharSize;

    /// Do fonts have margins? Is this a pixel border in the composited character
    /// map texture (perhaps per-glyph)?
    de::Vector2ui _margin;

    AbstractFont(fontid_t bindId = 0);
    virtual ~AbstractFont() {}

    DENG2_AS_IS_METHODS()

    fontid_t primaryBind() const;

    void setPrimaryBind(fontid_t bindId);

    /// @return  @ref fontFlags
    int flags() const;

    int ascent();
    int descent();
    int lineSpacing();

    virtual void glInit();
    virtual void glDeinit();

    virtual de::Rectanglei const &charGeometry(uchar ch) = 0;
    virtual int charWidth(uchar ch) = 0;
    virtual int charHeight(uchar ch) = 0;

    de::Vector2i charSize(uchar ch);
};

#endif // CLIENT_RESOURCE_ABSTRACTFONT_H
