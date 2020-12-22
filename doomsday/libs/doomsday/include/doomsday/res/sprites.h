/** @file sprites.h  Sprites.
 * @ingroup resource
 *
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_RESOURCE_SPRITES_H
#define LIBDOOMSDAY_RESOURCE_SPRITES_H

#include "../libdoomsday.h"
#include "../defs/sprite.h"
#include <de/legacy/types.h>
#include <de/record.h>
#include <de/hash.h>

namespace res {

class LIBDOOMSDAY_PUBLIC Sprites
{
public:
    typedef de::Hash<int, defn::CompiledSpriteRecord> SpriteSet;  ///< frame => Sprite

    static Sprites &get();

public:
    Sprites();

    void initSprites();

    void clear();

    SpriteSet &addSpriteSet(spritenum_t id, const SpriteSet &frames);

    /**
     * Returns @c true if a Sprite exists with given unique @a id and @a frame number.
     * Consider using spritePtr() if the sprite definition needs to be access as well.
     */
    bool hasSprite(spritenum_t id, int frame) const;

    /**
     * Lookup a Sprite by it's unique @a id and @a frame number.
     *
     * @see hasSprite(), spritePtr()
     */
    defn::CompiledSpriteRecord &sprite(spritenum_t id, int frame);

    /**
     * Returns a pointer to the identified Sprite, or @c nullptr.
     */
    const defn::CompiledSpriteRecord *spritePtr(spritenum_t id, int frame) const;

    const SpriteSet *tryFindSpriteSet(spritenum_t id) const;

    /**
     * Returns the SpriteSet associated with the given unique @a id.
     */
    const SpriteSet &spriteSet(spritenum_t id) const;

    /**
     * Returns the total number of SpriteSets.
     */
    int spriteCount() const;

public:
    /// Returns a value in the range [0..Sprite::MAX_VIEWS] if @a angleCode can be
    /// interpreted as a sprite view (angle) index; otherwise @c -1
    static int toSpriteAngle(de::Char angleCode);

    /// Returns @c true if @a name is a well-formed sprite name.
    static bool isValidSpriteName(const de::String& name);

private:
    DE_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_SPRITES_H
