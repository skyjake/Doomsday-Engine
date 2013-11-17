/** @file sprites.cpp Logical Sprite Management.
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

#include <cstdio>
#include <cstdlib>

#include <de/memory.h>
#include <de/memoryblockset.h>
#include <de/Error>

#include "de_platform.h"
#include "de_resource.h"

#include "dd_main.h"
#include "sys_system.h" // novideo
#include "def_main.h"

#ifdef __CLIENT__
#  include "MaterialSnapshot"

#  include "gl/gl_tex.h"
#  include "gl/gl_texmanager.h"

#  include "Lumobj"
#  include "render/sprite.h"
#endif

#include "resource/sprites.h"

#include <QList>
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

typedef QList<SpriteGroup> Groups;
static Groups groups;

struct spriterecord_frame_t
{
    byte frame[2];
    byte rotation[2];
    Material *mat;
    spriterecord_frame_t *next;
};

#define SPRITERECORD_MAX_FRAMES  128

struct spriterecord_t
{
    char name[5];
    int numFrames;
    spriterecord_frame_t *sprites;
    spriterecord_t *next;
};

// Tempory storage, used when reading sprite definitions.
static int numSpriteRecords;
static spriterecord_t *spriteRecords;
static blockset_t *spriteRecordBlockSet, *spriteRecordFrameBlockSet;

static void installSpriteLump(Sprite *sprTemp, int *maxFrame,
    Material *mat, uint frame, uint rotation, bool flipped)
{
    if(frame >= 30 || rotation > 8)
    {
        return;
    }

    if((signed) frame > *maxFrame)
        *maxFrame = frame;

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

static spriterecord_t *findSpriteRecordForName(char const *name)
{
    spriterecord_t *rec = 0;
    if(name && name[0] && spriteRecords)
    {
        rec = spriteRecords;
        while(strnicmp(rec->name, name, 4) && (rec = rec->next)) {}
    }
    return rec;
}

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
static void buildSprite(TextureManifest &manifest)
{
    // Have we already encountered this name?
    String decodedPath = QString(QByteArray::fromPercentEncoding(manifest.path().toUtf8()));
    QByteArray decodedPathUtf8 = decodedPath.toUtf8();

    spriterecord_t *rec = findSpriteRecordForName(decodedPathUtf8.constData());
    if(!rec)
    {
        // An entirely new sprite.
        rec = (spriterecord_t *) BlockSet_Allocate(spriteRecordBlockSet);
        strncpy(rec->name, decodedPathUtf8.constData(), 4);
        rec->name[4] = '\0';
        rec->numFrames = 0;
        rec->sprites = NULL;

        rec->next = spriteRecords;
        spriteRecords = rec;
        ++numSpriteRecords;
    }

    // Add the frame(s).
    int const frameNumber    = decodedPath.at(4).toUpper().unicode() - QChar('A').unicode() + 1;
    int const rotationNumber = decodedPath.at(5).digitValue();

    bool link = false;
    spriterecord_frame_t *frame = rec->sprites;
    if(rec->sprites)
    {
        while(!(frame->frame[0]    == frameNumber &&
                frame->rotation[0] == rotationNumber) &&
              (frame = frame->next)) {}
    }

    if(!frame)
    {
        // A new frame.
        frame = (spriterecord_frame_t *) BlockSet_Allocate(spriteRecordFrameBlockSet);
        link = true;
    }

    frame->mat         = &App_Materials().find(de::Uri("Sprites", manifest.path())).material();
    frame->frame[0]    = frameNumber;
    frame->rotation[0] = rotationNumber;

    // Not mirrored?
    if(decodedPath.length() >= 8)
    {
        frame->frame[1]    = decodedPath.at(6).toUpper().unicode() - QChar('A').unicode() + 1;
        frame->rotation[1] = decodedPath.at(7).digitValue();
    }
    else
    {
        frame->frame[1]    = 0;
        frame->rotation[1] = 0;
    }

    if(link)
    {
        frame->next = rec->sprites;
        rec->sprites = frame;
    }
}

/**
 * Sprites are patches with a special naming convention so they can be
 * recognized by R_InitSprites.  The sprite and frame specified by a
 * mobj is range checked at run time.
 *
 * A sprite is a patch_t that is assumed to represent a three dimensional
 * object and may have multiple rotations pre drawn.  Horizontal flipping
 * is used to save space. Some sprites will only have one picture used
 * for all views.
 */
static void buildSprites()
{
    Time begunAt;

    numSpriteRecords = 0;
    spriteRecords = 0;
    spriteRecordBlockSet = BlockSet_New(sizeof(spriterecord_t), 64),
    spriteRecordFrameBlockSet = BlockSet_New(sizeof(spriterecord_frame_t), 256);

    PathTreeIterator<TextureScheme::Index> iter(App_Textures().scheme("Sprites").index().leafNodes());
    while(iter.hasNext())
    {
        buildSprite(iter.next());
    }

    LOG_INFO(String("buildSprites: Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

void R_ClearAllSprites()
{
    groups.clear();
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
static void initSpriteDefs(spriterecord_t *const *sprRecords, int numSprites)
{
    Sprite sprTemp[SPRITERECORD_MAX_FRAMES];

    R_ClearAllSprites();

    for(int i = 0; i < numSprites; ++i)
    {
        spriterecord_t const *rec = sprRecords[i];

        groups.append(SpriteGroup());
        SpriteGroup &group = groups.last();

        // A record for a sprite we were unable to locate?
        if(!rec) continue;

        std::memset(sprTemp, -1, sizeof(sprTemp));
        int maxSprite = -1;

        spriterecord_frame_t const *sprite = rec->sprites;
        do
        {
            installSpriteLump(sprTemp, &maxSprite, sprite->mat, sprite->frame[0] - 1,
                              sprite->rotation[0], false);
            if(sprite->frame[1])
            {
                installSpriteLump(sprTemp, &maxSprite, sprite->mat, sprite->frame[1] - 1,
                                  sprite->rotation[1], true);
            }
        } while((sprite = sprite->next));

        /*
         * Check the frames that were found for completeness.
         */
        if(-1 == maxSprite)
        {
            // Should NEVER happen.
            //sprite->_numFrames = 0;
        }

        ++maxSprite;
        for(int j = 0; j < maxSprite; ++j)
        {
            if(int( sprTemp[j]._rotate) == -1) // Ahem!
            {
                // No rotations were found for that frame at all.
                Error("R_InitSprites", QString("No patches found for %1 frame %2")
                                           .arg(rec->name).arg(char(j + 'A')));
            }

            if(sprTemp[j]._rotate)
            {
                // Must have all 8 frames.
                for(int rotation = 0; rotation < 8; ++rotation)
                {
                    if(!sprTemp[j]._mats[rotation])
                        Error("R_InitSprites", QString("Sprite %1 frame %2 is missing rotations")
                                                   .arg(rec->name).arg(char(j + 'A')));
                }
            }
        }

        for(int i = 0; i < maxSprite; ++i)
        {
            Sprite const &tmpSprite = sprTemp[i];

            Sprite *sprite = new Sprite;

            sprite->_rotate = tmpSprite._rotate;
            std::memcpy(sprite->_mats, tmpSprite._mats, sizeof(sprite->_mats));
            std::memcpy(sprite->_flip, tmpSprite._flip, sizeof(sprite->_flip));

            group.sprites.append(sprite);
        }
    }
}

void R_InitSprites()
{
    Time begunAt;

    LOG_VERBOSE("Building Sprites...");
    buildSprites();

    /**
     * @attention Kludge:
     * As the games still rely upon the sprite definition indices matching
     * those of the sprite name table, use the latter to re-index the sprite
     * record database.
     * New sprites added in mods that we do not have sprite name defs for
     * are pushed to the end of the list (this is fine as the game will not
     * attempt to reference them by either name or indice as they are not
     * present in their game-side sprite index tables. Similarly, DeHackED
     * does not allow for new sprite frames to be added anyway).
     *
     * @todo
     * This unobvious requirement should be broken somehow and perhaps even
     * get rid of the sprite name definitions entirely.
     */
    if(numSpriteRecords)
    {
        int max = de::max(numSpriteRecords, countSprNames.num);
        if(max > 0)
        {
            spriterecord_t *rec, **list = (spriterecord_t **) M_Calloc(sizeof(spriterecord_t *) * max);
            int n = max - 1;

            rec = spriteRecords;
            do
            {
                int idx = Def_GetSpriteNum(rec->name);
                list[idx == -1? n-- : idx] = rec;
            } while((rec = rec->next));

            // Create sprite definitions from the located sprite patch lumps.
            initSpriteDefs(list, max);
            M_Free(list);
        }
    }
    // Kludge end

    // We are now done with the sprite records.
    BlockSet_Delete(spriteRecordBlockSet); spriteRecordBlockSet = 0;
    BlockSet_Delete(spriteRecordFrameBlockSet); spriteRecordFrameBlockSet = 0;
    numSpriteRecords = 0;

    LOG_INFO(String("R_InitSprites: Completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

void R_ShutdownSprites()
{
    R_ClearAllSprites();
}

Sprite::Sprite()
    : _rotate(0)
{
    zap(_mats);
    zap(_flip);
}

Material *Sprite::material(int rotation, bool *flipX, bool *flipY) const
{
    if(flipX) *flipX = false;
    if(flipY) *flipY = false;

    if(rotation < 0 || rotation >= max_angles)
    {
        return 0;
    }

    if(flipX) *flipX = CPP_BOOL(_flip[rotation]);

    return _mats[rotation];
}

Material *Sprite::material(angle_t mobjAngle, angle_t angleToEye,
    bool noRotation, bool *flipX, bool *flipY) const
{
    int rotation = 0; // Use single rotation for all viewing angles (default).

    if(!noRotation && _rotate)
    {
        // Rotation is determined by the relative angle to the viewer.
        rotation = (angleToEye - mobjAngle + (unsigned) (ANG45 / 2) * 9) >> 29;
    }

    return material(rotation, flipX, flipY);
}

#ifdef __CLIENT__
Lumobj *Sprite::generateLumobj() const
{
    // Always use rotation zero.
    /// @todo We could do better here...
    Material *mat = material();
    if(!mat) return 0;

    // Ensure we have up-to-date information about the material.
    MaterialSnapshot const &ms = mat->prepare(Rend_SpriteMaterialSpec());
    if(!ms.hasTexture(MTU_PRIMARY)) return 0; // Unloadable texture?
    Texture &tex = ms.texture(MTU_PRIMARY).generalCase();

    pointlight_analysis_t const *pl = reinterpret_cast<pointlight_analysis_t const *>(ms.texture(MTU_PRIMARY).generalCase().analysisDataPointer(Texture::BrightPointAnalysis));
    if(!pl) throw Error("Sprite::generateLumobj", QString("Texture \"%1\" has no BrightPointAnalysis").arg(ms.texture(MTU_PRIMARY).generalCase().manifest().composeUri()));

    // Apply the auto-calculated color.
    return &(new Lumobj(Vector3d(), pl->brightMul, pl->color.rgb))
                    ->setZOffset(-tex.origin().y - pl->originY * ms.height());
}
#endif // __CLIENT__

int R_SpriteCount()
{
    return groups.count();
}

Sprite *R_SpritePtr(int spriteId, int frame)
{
    if(spriteId >= 0 && spriteId < groups.count())
    {
        SpriteGroup &group = groups[spriteId];
        if(frame >= 0 && frame < group.sprites.count())
        {
            return group.sprites.at(frame);
        }
    }
    return 0;
}

Sprite &R_Sprite(int spriteId, int frame)
{
    if(spriteId >= 0 && spriteId < groups.count())
    {
        SpriteGroup &group = groups[spriteId];
        if(frame >= 0 && frame < group.sprites.count())
        {
            return *group.sprites.at(frame);
        }
        throw Error("R_Sprite", QString("Invalid sprite frame %1").arg(frame));
    }
    throw Error("R_Sprite", QString("Invalid sprite id %1").arg(spriteId));
}

SpriteSet &R_SpriteSet(int spriteId)
{
    if(spriteId >= 0 && spriteId < groups.count())
    {
        return groups[spriteId].sprites;
    }
    throw Error("R_SpriteSet", QString("Invalid sprite id %1").arg(spriteId));
}

#undef R_GetSpriteInfo
DENG_EXTERN_C boolean R_GetSpriteInfo(int spriteId, int frame, spriteinfo_t *info)
{
    LOG_AS("R_GetSpriteInfo");

    if(!info) return false;

    zapPtr(info);

    Sprite *sprite = R_SpritePtr(spriteId, frame);
    if(!sprite)
    {
        LOG_WARNING("Invalid sprite id (%i) and/or frame index (%i).")
            << spriteId << frame;
        return false;
    }

    zapPtr(info);
    info->flip = sprite->_flip[0];

    if(novideo)
    {
        // We can't prepare the material.
        return true;
    }

    info->material = sprite->_mats[0];

#ifdef __CLIENT__
    /// @todo fixme: We should not be using the PSprite spec here. -ds
    MaterialVariantSpec const &spec =
            App_Materials().variantSpec(PSpriteContext, 0, 1, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                        0, 1, -1, false, true, true, false);
    MaterialSnapshot const &ms = info->material->prepare(spec);

    Texture &tex = ms.texture(MTU_PRIMARY).generalCase();
    variantspecification_t const &texSpec = TS_GENERAL(ms.texture(MTU_PRIMARY).spec());

    info->geometry.origin.x    = -tex.origin().x + -texSpec.border;
    info->geometry.origin.y    = -tex.origin().y +  texSpec.border;
    info->geometry.size.width  = ms.width()  + texSpec.border * 2;
    info->geometry.size.height = ms.height() + texSpec.border * 2;

    ms.texture(MTU_PRIMARY).glCoords(&info->texCoord[0], &info->texCoord[1]);
#else
    Texture &tex = *info->material->layers()[0]->stages()[0]->texture;

    info->geometry.origin.x    = -tex.origin().x;
    info->geometry.origin.y    = -tex.origin().y;
    info->geometry.size.width  = info->material->width();
    info->geometry.size.height = info->material->height();
#endif

    return true;
}
