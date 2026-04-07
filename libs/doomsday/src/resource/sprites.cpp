/** @file sprites.cpp  Sprites.
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

#include "doomsday/res/sprites.h"
#include "doomsday/res/resources.h"
#include "doomsday/res/textures.h"
#include "doomsday/defs/ded.h"
#include "doomsday/defs/sprite.h"

#include <de/legacy/types.h>

namespace res {

using namespace de;

DE_PIMPL_NOREF(Sprites)
{
    Hash<spritenum_t, SpriteSet> sprites;

    ~Impl()
    {
        sprites.clear();
    }

    inline bool hasSpriteSet(spritenum_t id) const
    {
        return sprites.contains(id);
    }

    SpriteSet *tryFindSpriteSet(spritenum_t id) const
    {
        auto found = sprites.find(id);
        return (found != sprites.end()? const_cast<SpriteSet *>(&found->second) : nullptr);
    }

    SpriteSet &findSpriteSet(spritenum_t id)
    {
        if (SpriteSet *frames = tryFindSpriteSet(id)) return *frames;
        /// @throw MissingResourceError An unknown/invalid id was specified.
        throw Resources::MissingResourceError("Sprites::findSpriteSet",
                                              "Unknown sprite id " + String::asText(id));
    }

    SpriteSet &addSpriteSet(spritenum_t id, const SpriteSet &frames)
    {
        DE_ASSERT(!tryFindSpriteSet(id));  // sanity check.
        sprites.insert(id, frames);
        return sprites[id];
    }
};

Sprites::Sprites()
    : d(new Impl)
{}

void Sprites::clear()
{
    d->sprites.clear();
}

Sprites::SpriteSet &Sprites::addSpriteSet(spritenum_t id, const SpriteSet &frames)
{
    return d->addSpriteSet(id, frames);
}

dint Sprites::spriteCount() const
{
    return d->sprites.size();
}

bool Sprites::hasSprite(spritenum_t id, dint frame) const
{
    if (const SpriteSet *frames = d.getConst()->tryFindSpriteSet(id))
    {
        return frames->contains(frame);
    }
    return false;
}

defn::CompiledSpriteRecord &Sprites::sprite(spritenum_t id, dint frame)
{
    return d->findSpriteSet(id).find(frame)->second;
}

const defn::CompiledSpriteRecord *Sprites::spritePtr(spritenum_t id, int frame) const
{
    if (const Sprites::SpriteSet *sprSet = tryFindSpriteSet(id))
    {
        auto found = sprSet->find(frame);
        if (found != sprSet->end()) return &found->second;
    }
    return nullptr;
}

const Sprites::SpriteSet *Sprites::tryFindSpriteSet(spritenum_t id) const
{
    return d->tryFindSpriteSet(id);
}

const Sprites::SpriteSet &Sprites::spriteSet(spritenum_t id) const
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
typedef std::multimap<dint, SpriteFrameDef> SpriteFrameDefs;  ///< frame => frame angle def.
typedef Hash<String, SpriteFrameDefs> SpriteDefs;        ///< sprite name => frame set.

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
static SpriteDefs buildSpriteFramesFromTextures(const res::TextureScheme::Index &texIndex)
{
    static const dint NAME_LENGTH = 4;

    SpriteDefs frameSets;
//    frameSets.reserve(texIndex.leafNodes().count() / 8);  // overestimate.

    PathTreeIterator<res::TextureScheme::Index> iter(texIndex.leafNodes());
    while (iter.hasNext())
    {
        const res::TextureManifest &texManifest = iter.next();

        const String material   = res::Uri("Sprites", texManifest.path()).compose();
        // Decode the sprite frame descriptor.
        const String desc       = String::fromPercentEncoding(texManifest.path().toString());

        // Find/create a new sprite frame set.
        const String spriteName = desc.left(CharPos(NAME_LENGTH)).lower();
        SpriteFrameDefs *frames = &frameSets[spriteName];

        // The descriptor may define either one or two frames.
        const bool haveMirror = desc.length() >= 8;
        for (dint i = 0; i < (haveMirror ? 2 : 1); ++i)
        {
            const String nums = desc.mid(CharPos(NAME_LENGTH + i * 2), 2);
            const dint frameNumber = nums.first().upper().delta('A');
            const dint angleNumber = Sprites::toSpriteAngle(nums.last());

            if (frameNumber < 0) continue;

            // Find/create a new frame.
            SpriteFrameDef *frame = nullptr;
            auto found = frames->find(frameNumber);
            if (found != frames->end() && found->second.angle == angleNumber)
            {
                frame = &found->second;
            }
            else
            {
                // Create a new frame.
                frame = &frames->insert(std::make_pair(frameNumber, SpriteFrameDef()))->second;
            }

            // (Re)Configure the frame.
            DE_ASSERT(frame);
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
static Sprites::SpriteSet buildSprites(const std::multimap<dint, SpriteFrameDef> &frameDefs)
{
    static const int MAX_ANGLES = 16;

    Sprites::SpriteSet frames;

    // Build initial Sprites and add views.
    for (auto it = frameDefs.begin(); it != frameDefs.end(); ++it)
    {
        defn::CompiledSpriteRecord *rec = nullptr;
        auto found = frames.find(it->first);
        if (found != frames.end())
        {
            rec = &found->second;
        }
        else
        {
            rec = &frames[it->first];
            defn::Sprite(*rec).resetToDefaults();
        }

        const SpriteFrameDef &def = it->second;
        defn::Sprite(*rec).addView(def.material, def.angle, def.mirrored);
    }

    // Duplicate views to complete angle sets (if defined).
    for (const auto &fit : frames)
    {
        defn::Sprite sprite(fit.second);

        if (sprite.viewCount() < 2)
            continue;

        for (dint angle = 0; angle < MAX_ANGLES / 2; ++angle)
        {
            if (!sprite.hasView(angle * 2 + 1) && sprite.hasView(angle * 2))
            {
                auto src = sprite.view(angle * 2);
                sprite.addView(src.material->asText(), angle * 2 + 2, src.mirrorX);
            }
            if (!sprite.hasView(angle * 2) && sprite.hasView(angle * 2 + 1))
            {
                auto src = sprite.view(angle * 2 + 1);
                sprite.addView(src.material->asText(), angle * 2 + 1, src.mirrorX);
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
    for (auto it = spriteDefs.begin(); it != spriteDefs.end(); ++it)
    {
        // Lookup the id for the named sprite.
        spritenum_t id = DED_Definitions()->getSpriteNum(it->first);
        if (id == -1)
        {
            // Assign a new id from the end of the range.
            id = (DED_Definitions()->sprites.size() + customIdx++);
        }

        // Build a Sprite (frame) set from these definitions.
        addSpriteSet(id, buildSprites(it->second));
    }

    // We're done with the definitions.
    spriteDefs.clear();

    LOG_RES_VERBOSE("Sprites built in %.2f seconds") << begunAt.since();
}

dint Sprites::toSpriteAngle(Char angleCode) // static
{
    static const dint MAX_ANGLES = 16;

    dint angle = -1; // Unknown.
    
    if (angleCode.isNumeric())
    {
        angle = angleCode - '0';
    }
    else if (angleCode.isAlpha())
    {
        Char charCodeLatin1 = angleCode.upper();
        if (charCodeLatin1 >= 'A')
        {
            angle = charCodeLatin1.delta('A') + 10;
        }
    }
    
    if (angle < 0 || angle > MAX_ANGLES)
    {
        return -1;
    }
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

bool Sprites::isValidSpriteName(const String& name) // static
{
    const auto len = name.length();
    
    if (len < 6) return false;

    // Character at position 5 is a view (angle) index.
    if (toSpriteAngle(name.at(CharPos(5))) < 0) return false;

    // If defined, the character at position 7 is also a rotation number.
    return (len <= 7 || toSpriteAngle(name.at(CharPos(7))) >= 0);
}

Sprites &Sprites::get() // static
{
    return Resources::get().sprites();
}

} // namespace res
