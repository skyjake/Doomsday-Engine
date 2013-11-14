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

#include <de/Observers>
#include <de/Rectangle>
#include <de/Vector>
#include <QFlags>

#define MAX_CHARS               256 // Normal 256 ANSI characters.

namespace de {
class FontManifest;
}

/**
 * Abstract font resource.
 *
 * @ingroup resource
 */
class AbstractFont
{
public:
    DENG2_DEFINE_AUDIENCE(Deletion, void fontBeingDeleted(AbstractFont const &font))

    enum Flag {
        Colorize  = 0x1, ///< Can be colored.
        Shadowed  = 0x2  ///< A shaodw is embedded in the font.
    };
    Q_DECLARE_FLAGS(Flags, Flag)

public:
    /// Resource manifest for the font.
    de::FontManifest &_manifest;

    Flags _flags;

    AbstractFont(de::FontManifest &manifest);
    virtual ~AbstractFont();

    DENG2_AS_IS_METHODS()

    /**
     * Returns the resource manifest for the font.
     */
    de::FontManifest &manifest() const;

    /// @return  Returns a copy of the font's flags.
    Flags flags() const;

    virtual int ascent();
    virtual int descent();
    virtual int lineSpacing();

    virtual void glInit();
    virtual void glDeinit();

    virtual de::Rectanglei const &glyphPosCoords(uchar ch) = 0;
    virtual de::Rectanglei const &glyphTexCoords(uchar ch) = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractFont::Flags)

#endif // CLIENT_RESOURCE_ABSTRACTFONT_H
