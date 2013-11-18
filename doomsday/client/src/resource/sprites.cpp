/** @file sprites.cpp  Sprite resource management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#include "resource/sprites.h"

#include "dd_main.h"
#include "sys_system.h" // novideo
#include "def_main.h"
#include <de/Error>
#include <de/memory.h>
#include <de/memoryblockset.h>
#include <QHash>
#include <QtAlgorithms>

using namespace de;

struct SpriteGroup
{
    SpriteSet sprites;

    ~SpriteGroup()
    {
        qDeleteAll(sprites);
    }
};

typedef QHash<spritenum_t, SpriteGroup> Groups;
static Groups groups;

static SpriteGroup *toGroup(int spriteId)
{
    Groups::iterator found = groups.find(spritenum_t(spriteId));
    if(found != groups.end())
    {
        return &found.value();
    }
    return 0;
}

void R_ClearAllSprites()
{
    groups.clear();
}

void R_ShutdownSprites()
{
    R_ClearAllSprites();
}

int R_SpriteCount()
{
    return groups.count();
}

Sprite *R_SpritePtr(int spriteId, int frame)
{
    if(SpriteGroup *group = toGroup(spriteId))
    {
        if(frame >= 0 && frame < group->sprites.count())
        {
            return group->sprites.at(frame);
        }
    }
    return 0;
}

Sprite &R_Sprite(int spriteId, int frame)
{
    if(SpriteGroup *group = toGroup(spriteId))
    {
        if(frame >= 0 && frame < group->sprites.count())
        {
            return *group->sprites.at(frame);
        }
        throw Error("R_Sprite", QString("Invalid sprite frame %1").arg(frame));
    }
    throw Error("R_Sprite", QString("Invalid sprite id %1").arg(spriteId));
}

SpriteSet &R_SpriteSet(int spriteId)
{
    if(SpriteGroup *group = toGroup(spriteId))
    {
        return group->sprites;
    }
    throw Error("R_SpriteSet", QString("Invalid sprite id %1").arg(spriteId));
}

struct SpriteFrameDef
{
    byte frame[2];
    byte rotation[2];
    Material *mat;

    SpriteFrameDef *next;
};

struct SpriteDef
{
    static int const max_frames = 128;

    char name[5];
    int numFrames;
    SpriteFrameDef *frames;

    SpriteDef *next;
};

// Tempory storage, used when reading sprite definitions.
static int spriteDefCount;
static SpriteDef *spriteDefs;
static blockset_t *spriteDefBlockSet;
static blockset_t *spriteFrameDefBlockSet;

/**
 * In DOOM, a sprite frame is a patch texture contained in a lump
 * existing between the S_START and S_END marker lumps (in WAD) whose
 * lump name matches the following pattern:
 *
 *      NAME|A|R(A|R) (for example: "TROOA0" or "TROOA2A8")
 *
 * NAME: Four character name of the sprite.
 * A: Animation frame ordinal 'A'... (ASCII).
 * R: Rotation angle 0...8
 *    0 : Use this frame for ALL angles.
 *    1...8 : Angle of rotation in 45 degree increments.
 *
 * The second set of (optional) frame and rotation characters instruct
 * that the same sprite frame is to be used for an additional frame
 * but that the sprite patch should be flipped horizontally (right to
 * left) during the loading phase.
 *
 * Sprite rotation 0 is facing the viewer, rotation 1 is one angle
 * turn CLOCKWISE around the axis. This is not the same as the angle,
 * which increases counter clockwise (protractor).
 */
static SpriteDef *generateSpriteDefs(int &count)
{
    Time begunAt;

    spriteDefCount         = 0;
    spriteDefs             = 0;
    spriteDefBlockSet      = BlockSet_New(sizeof(SpriteDef), 64),
    spriteFrameDefBlockSet = BlockSet_New(sizeof(SpriteFrameDef), 256);

    PathTreeIterator<TextureScheme::Index> iter(App_Textures().scheme("Sprites").index().leafNodes());
    while(iter.hasNext())
    {
        TextureManifest &manifest = iter.next();

        // Have we already encountered this name?
        String spriteName = QString(QByteArray::fromPercentEncoding(manifest.path().toUtf8()));
        QByteArray spriteNameUtf8 = spriteName.toUtf8();

        SpriteDef *spriteDef = spriteDefs;
        if(spriteDefs)
        {
            while(strnicmp(spriteDef->name, spriteNameUtf8.constData(), 4)
                  && (spriteDef = spriteDef->next))
            {}
        }

        if(!spriteDef)
        {
            // An entirely new sprite.
            spriteDef = (SpriteDef *) BlockSet_Allocate(spriteDefBlockSet);

            strncpy(spriteDef->name, spriteNameUtf8.constData(), 4);
            spriteDef->name[4] = '\0';
            spriteDef->numFrames = 0;
            spriteDef->frames = 0;

            spriteDef->next = spriteDefs;
            spriteDefs = spriteDef;
            ++spriteDefCount;
        }

        // Add the frame(s).
        int const frameNumber    = spriteName.at(4).toUpper().unicode() - QChar('A').unicode() + 1;
        int const rotationNumber = spriteName.at(5).digitValue();

        SpriteFrameDef *frame = spriteDef->frames;
        if(spriteDef->frames)
        {
            while(!(frame->frame[0]    == frameNumber &&
                    frame->rotation[0] == rotationNumber) &&
                  (frame = frame->next))
            {}
        }

        if(!frame)
        {
            // A new frame.
            frame = (SpriteFrameDef *) BlockSet_Allocate(spriteFrameDefBlockSet);

            frame->next = spriteDef->frames;
            spriteDef->frames = frame;
            spriteDef->numFrames += 1;
        }

        frame->mat         = &App_Materials().find(de::Uri("Sprites", manifest.path())).material();
        frame->frame[0]    = frameNumber;
        frame->rotation[0] = rotationNumber;

        // Not mirrored?
        if(spriteName.length() >= 8)
        {
            frame->frame[1]    = spriteName.at(6).toUpper().unicode() - QChar('A').unicode() + 1;
            frame->rotation[1] = spriteName.at(7).digitValue();
        }
        else
        {
            frame->frame[1]    = 0;
            frame->rotation[1] = 0;
        }
    }

    LOG_INFO(String("generateSpriteDefs: Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));

    count = spriteDefCount;
    return spriteDefs;
}

static void setupSpriteViewAngle(Sprite *sprTemp, int *maxFrame,
    Material *mat, uint frame, uint rotation, bool flipped)
{
    if(frame >= 30) return;
    if(rotation > 8) return;

    if((signed) frame > *maxFrame)
    {
        *maxFrame = frame;
    }

    if(rotation == 0)
    {
        // This frame should be used for all rotations.
        sprTemp[frame]._rotate = false;
        for(int r = 0; r < 8; ++r)
        {
            sprTemp[frame]._mats[r] = mat;
            sprTemp[frame]._flip[r] = byte( flipped );
        }
        return;
    }

    rotation--; // Make 0 based.

    sprTemp[frame]._rotate = true;
    sprTemp[frame]._mats[rotation] = mat;
    sprTemp[frame]._flip[rotation] = byte( flipped );
}

/**
 * Builds the sprite rotation matrixes to account for horizontally flipped
 * sprites.  Will report an error if the lumps are inconsistant.
 *
 * Sprite lump names are 4 characters for the actor, a letter for the frame,
 * and a number for the rotation, A sprite that is flippable will have an
 * additional letter/number appended.  The rotation character can be 0 to
 * signify no rotations.
 */
void R_InitSprites()
{
    Time begunAt;

    LOG_MSG("Building sprites...");

    R_ClearAllSprites();

    int defCount;
    SpriteDef *defs = generateSpriteDefs(defCount);

    if(defCount)
    {
        int spriteNameCount = de::max(defCount, countSprNames.num);
        groups.reserve(spriteNameCount);

        // Build the final sprites.
        Sprite sprTemp[SpriteDef::max_frames];
        int customIdx = 0;
        for(SpriteDef const *def = defs; def; def = def->next)
        {
            spritenum_t spriteId = Def_GetSpriteNum(def->name);
            if(spriteId == -1)
                spriteId = countSprNames.num + customIdx++;

            DENG2_ASSERT(!groups.contains(spriteId)); // sanity check.
            SpriteGroup &group = groups.insert(spriteId, SpriteGroup()).value();

            std::memset(sprTemp, -1, sizeof(sprTemp));

            int maxSprite = -1;
            for(SpriteFrameDef const *frameDef = def->frames; frameDef;
                frameDef = frameDef->next)
            {
                setupSpriteViewAngle(sprTemp, &maxSprite, frameDef->mat,
                                     frameDef->frame[0] - 1, frameDef->rotation[0],
                                     false);

                if(frameDef->frame[1])
                {
                    setupSpriteViewAngle(sprTemp, &maxSprite, frameDef->mat,
                                         frameDef->frame[1] - 1, frameDef->rotation[1],
                                         true);
                }
            }
            ++maxSprite;

            for(int k = 0; k < maxSprite; ++k)
            {
                if(int(sprTemp[k]._rotate) == -1) // Ahem!
                {
                    // No rotations were found for that frame at all.
                    Error("buildSprites", QString("No patches found for %1 frame %2")
                                              .arg(def->name).arg(char(k + 'A')));
                }

                if(sprTemp[k]._rotate)
                {
                    // Must have all 8 frames.
                    for(int rotation = 0; rotation < 8; ++rotation)
                    {
                        if(!sprTemp[k]._mats[rotation])
                        {
                            Error("buildSprites", QString("Sprite %1 frame %2 is missing rotations")
                                                      .arg(def->name).arg(char(k + 'A')));
                        }
                    }
                }
            }

            for(int k = 0; k < maxSprite; ++k)
            {
                Sprite const &tmpSprite = sprTemp[k];

                Sprite *sprite = new Sprite;

                sprite->_rotate = tmpSprite._rotate;
                std::memcpy(sprite->_mats, tmpSprite._mats, sizeof(sprite->_mats));
                std::memcpy(sprite->_flip, tmpSprite._flip, sizeof(sprite->_flip));

                group.sprites.append(sprite);
            }
        }
    }

    // We are now done with the sprite records.
    BlockSet_Delete(spriteDefBlockSet); spriteDefBlockSet = 0;
    BlockSet_Delete(spriteFrameDefBlockSet); spriteFrameDefBlockSet = 0;
    spriteDefCount = 0;
    spriteDefs = 0;

    LOG_INFO(String("R_InitSprites: Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}
