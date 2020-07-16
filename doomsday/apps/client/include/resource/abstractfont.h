/** @file abstractfont.h  Abstract font.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENT_RESOURCE_ABSTRACTFONT_H
#define CLIENT_RESOURCE_ABSTRACTFONT_H

#include <de/observers.h>
#include <de/rectangle.h>
#include <de/vector.h>

namespace de {
class FontManifest;
}

/// Special value used to signify an invalid font id.
#define NOFONTID  0

/**
 * Abstract font resource.
 *
 * @em Clearing a font means any names bound to it are deleted and any GL textures
 * acquired for it are 'released' at this time). The Font instance record used
 * to represent it is also deleted.
 *
 * @em Releasing a font will release any GL textures acquired for it.
 *
 * @ingroup resource
 */
class AbstractFont
{
public:
    /// Notified when the resource is about to be deleted.
    DE_DEFINE_AUDIENCE(Deletion, void fontBeingDeleted(const AbstractFont &font))

    enum Flag {
        Colorize  = 0x1, ///< Can be colored.
        Shadowed  = 0x2  ///< A shadow is embedded in the font.
    };

    static const int MAX_CHARS = 256; // Normal 256 ANSI characters.

public:
    /// Resource manifest for the font.
    de::FontManifest &_manifest;

    de::Flags _flags;

    AbstractFont(de::FontManifest &manifest);
    virtual ~AbstractFont();

    DE_CAST_METHODS()

    /**
     * Returns the resource manifest for the font.
     */
    de::FontManifest &manifest() const;

    /// @return  Returns a copy of the font's flags.
    de::Flags flags() const;

    virtual int ascent() const;
    virtual int descent() const;
    virtual int lineSpacing() const;
    virtual const de::Rectanglei &glyphPosCoords(de::dbyte ch) const = 0;
    virtual const de::Rectanglei &glyphTexCoords(de::dbyte ch) const = 0;

    virtual void glInit() const;
    virtual void glDeinit() const;
};

#endif // CLIENT_RESOURCE_ABSTRACTFONT_H
