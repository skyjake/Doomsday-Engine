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
#include "doomsday/resource/textures.h"
#include "doomsday/defs/ded.h"
#include "doomsday/defs/sprite.h"

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

struct SpriteFrameDef
{
    bool mirrored = false;
    dint angle = 0;
    String material;
};

// Tempory storage, used when reading sprite definitions.
typedef QMultiMap<dint, SpriteFrameDef> SpriteFrameDefs;  ///< frame => frame angle def.
typedef QHash<String, SpriteFrameDefs> SpriteDefs;        ///< sprite name => frame set.

/**
 * In DOOM, a sprite frame is a patch texture contained in a lump existing between
 * the S_START and S_END marker lumps (in WAD) whose lump name matches the following
 * pattern:
 *
 *    NAME|A|R(A|R) (for example: "TROOA0" or "TROOA2A8")
 *
 * NAME: Four character name of the sprite.
 * A: Animation frame ordinal 'A'... (ASCII).
 * R: Rotation angle 0...G
 *    0 : Use this frame for ALL angles.
 *    1...8: Angle of rotation in 45 degree increments.
 *    A...G: Angle of rotation in 22.5 degree increments.
 *
 * The second set of (optional) frame and rotation characters instruct that the
 * same sprite frame is to be used for an additional frame but that the sprite
 * patch should be flipped horizontally (right to left) during the loading phase.
 *
 * Sprite view 0 is facing the viewer, rotation 1 is one half-angle turn CLOCKWISE
 * around the axis. This is not the same as the angle, which increases
 * counter clockwise (protractor).
 */
static SpriteDefs buildSpriteFramesFromTextures(res::TextureScheme::Index const &texIndex)
{
    static dint const NAME_LENGTH = 4;

    SpriteDefs frameSets;
    frameSets.reserve(texIndex.leafNodes().count() / 8);  // overestimate.

    PathTreeIterator<res::TextureScheme::Index> iter(texIndex.leafNodes());
    while (iter.hasNext())
    {
        res::TextureManifest const &texManifest = iter.next();

        String const material   = de::Uri("Sprites", texManifest.path()).compose();
        // Decode the sprite frame descriptor.
        String const desc       = QString(QByteArray::fromPercentEncoding(texManifest.path().toUtf8()));

        // Find/create a new sprite frame set.
        String const spriteName = desc.left(NAME_LENGTH).toLower();
        SpriteFrameDefs *frames = nullptr;
        if (frameSets.contains(spriteName))
        {
            frames = &frameSets.find(spriteName).value();
        }
        else
        {
            frames = &frameSets.insert(spriteName, SpriteFrameDefs()).value();
        }

        // The descriptor may define either one or two frames.
        bool const haveMirror = desc.length() >= 8;
        for (dint i = 0; i < (haveMirror ? 2 : 1); ++i)
        {
            dint const frameNumber = desc.at(NAME_LENGTH + i * 2).toUpper().unicode() - QChar('A').unicode();
            dint const angleNumber = Sprites::toSpriteAngle(desc.at(NAME_LENGTH + i * 2 + 1));

            if (frameNumber < 0) continue;

            // Find/create a new frame.
            SpriteFrameDef *frame = nullptr;
            auto found = frames->find(frameNumber);
            if (found != frames->end() && found.value().angle == angleNumber)
            {
                frame = &found.value();
            }
            else
            {
                // Create a new frame.
                frame = &frames->insert(frameNumber, SpriteFrameDef()).value();
            }

            // (Re)Configure the frame.
            DENG2_ASSERT(frame);
            frame->material = material;
            frame->angle    = angleNumber;
            frame->mirrored = i == 1;
        }
    }

    return frameSets;
}

/**
 * Generates a set of Sprites from the given @a frameSet.
 *
 * @note Gaps in the frame number range will be filled with dummy Sprite instances
 * (no view angles added).
 *
 * @param frameDefs  SpriteFrameDefs to process.
 *
 * @return  Newly built sprite frame-number => definition map.
 *
 * @todo Don't do this here (no need for two-stage construction). -ds
 */
static Sprites::SpriteSet buildSprites(QMultiMap<dint, SpriteFrameDef> const &frameDefs)
{
    static de::dint const MAX_ANGLES = 16;

    // Build initial Sprites and add views.
    QMap<dint, Record> frames;
    for (auto it = frameDefs.constBegin(); it != frameDefs.constEnd(); ++it)
    {
        Record *rec = nullptr;
        if (frames.contains(it.key()))
        {
            rec = &frames.find(it.key()).value();
        }
        else
        {
            Record temp;
            defn::Sprite sprite(temp);
            sprite.resetToDefaults();
            rec = &frames.insert(it.key(), sprite.def()) // A copy is made.
                  .value();
        }

        SpriteFrameDef const &def = it.value();
        defn::Sprite(*rec).addView(def.material, def.angle, def.mirrored);
    }

    // Duplicate views to complete angle sets (if defined).
    for (Record &rec : frames)
    {
        defn::Sprite sprite(rec);

        if (sprite.viewCount() < 2)
            continue;

        for (dint angle = 0; angle < MAX_ANGLES / 2; ++angle)
        {
            if (!sprite.hasView(angle * 2 + 1) && sprite.hasView(angle * 2))
            {
                auto const &src = sprite.view(angle * 2);
                sprite.addView(src.gets("material"), angle * 2 + 2, src.getb("mirrorX"));
            }
            if (!sprite.hasView(angle * 2) && sprite.hasView(angle * 2 + 1))
            {
                auto const &src = sprite.view(angle * 2 + 1);
                sprite.addView(src.gets("material"), angle * 2 + 1, src.getb("mirrorX"));
            }
        }
    }

    return frames;
}

void Sprites::initSprites()
{
    LOG_AS("Sprites");
    LOG_RES_VERBOSE("Building sprites...");

    Time begunAt;

    clear();

    // Build Sprite sets from their definitions.
    /// @todo It should no longer be necessary to split this into two phases -ds
    dint customIdx = 0;
    SpriteDefs spriteDefs = buildSpriteFramesFromTextures(res::Textures::get().textureScheme("Sprites").index());
    for (auto it = spriteDefs.constBegin(); it != spriteDefs.constEnd(); ++it)
    {
        // Lookup the id for the named sprite.
        spritenum_t id = DED_Definitions()->getSpriteNum(it.key());
        if (id == -1)
        {
            // Assign a new id from the end of the range.
            id = (DED_Definitions()->sprites.size() + customIdx++);
        }

        // Build a Sprite (frame) set from these definitions.
        addSpriteSet(id, buildSprites(it.value()));
    }

    // We're done with the definitions.
    spriteDefs.clear();

    LOG_RES_VERBOSE("Sprites built in %.2f seconds") << begunAt.since();
}

dint Sprites::toSpriteAngle(QChar angleCode) // static
{
    static dint const MAX_ANGLES = 16;

    dint angle = -1; // Unknown.
    if (angleCode.isDigit())
    {
        angle = angleCode.digitValue();
    }
    else if (angleCode.isLetter())
    {
        char charCodeLatin1 = angleCode.toUpper().toLatin1();
        if (charCodeLatin1 >= 'A')
        {
            angle = charCodeLatin1 - 'A' + 10;
        }
    }

    if (angle < 0 || angle > MAX_ANGLES)
        return -1;

    if (angle == 0) return 0;

    if (angle <= MAX_ANGLES / 2)
    {
        return (angle - 1) * 2 + 1;
    }
    else
    {
        return (angle - 9) * 2 + 2;
    }
}

bool Sprites::isValidSpriteName(String name) // static
{
    if (name.length() < 6) return false;

    // Character at position 5 is a view (angle) index.
    if (toSpriteAngle(name.at(5)) < 0) return false;

    // If defined, the character at position 7 is also a rotation number.
    return (name.length() <= 7 || toSpriteAngle(name.at(7)) >= 0);
}

Sprites &Sprites::get() // static
{
    return Resources::get().sprites();
}

} // namespace res
