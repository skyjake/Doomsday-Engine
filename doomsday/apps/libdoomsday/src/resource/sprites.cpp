/** @file sprites.cpp  Sprites.
 * @ingroup resource
 *
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/resource/sprites.h"
#include "doomsday/resource/resources.h"

#include <de/types.h>
#include <QMap>

namespace res {

using namespace de;

DENG2_PIMPL_NOREF(Sprites)
{
    QMap<spritenum_t, SpriteSet> sprites;

    ~Impl()
    {
        sprites.clear();
    }

    inline bool hasSpriteSet(spritenum_t id) const
    {
        return sprites.contains(id);
    }

    SpriteSet *tryFindSpriteSet(spritenum_t id)
    {
        auto found = sprites.find(id);
        return (found != sprites.end()? &found.value() : nullptr);
    }

    SpriteSet &findSpriteSet(spritenum_t id)
    {
        if (SpriteSet *frames = tryFindSpriteSet(id)) return *frames;
        /// @throw MissingResourceError An unknown/invalid id was specified.
        throw Resources::MissingResourceError("Sprites::findSpriteSet",
                                              "Unknown sprite id " + String::number(id));
    }

    SpriteSet &addSpriteSet(spritenum_t id, SpriteSet const &frames)
    {
        DENG2_ASSERT(!tryFindSpriteSet(id));  // sanity check.
        return sprites.insert(id, frames).value();
    }
};

Sprites::Sprites()
    : d(new Impl)
{}

void Sprites::clear()
{
    d->sprites.clear();
}

Sprites::SpriteSet &Sprites::addSpriteSet(spritenum_t id, SpriteSet const &frames)
{
    return d->addSpriteSet(id, frames);
}

dint Sprites::spriteCount() const
{
    return d->sprites.count();
}

bool Sprites::hasSprite(spritenum_t id, dint frame) const
{
    if (SpriteSet const *frames = d->tryFindSpriteSet(id))
    {
        return frames->contains(frame);
    }
    return false;
}

Record &Sprites::sprite(spritenum_t id, dint frame)
{
    return d->findSpriteSet(id).find(frame).value();
}

Sprites::SpriteSet const *Sprites::tryFindSpriteSet(spritenum_t id) const
{
    return d->tryFindSpriteSet(id);
}

Sprites::SpriteSet const &Sprites::spriteSet(spritenum_t id) const
{
    return d->findSpriteSet(id);
}

Sprites &Sprites::get() // static
{
    return Resources::get().sprites();
}

} // namespace res
