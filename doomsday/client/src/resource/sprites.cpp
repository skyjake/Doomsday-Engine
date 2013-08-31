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
#  include "gl/gl_texmanager.h"
#endif

#include "resource/sprites.h"

using namespace de;

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
    spriterecord_frame_t *frames;
    spriterecord_t *next;
};

spritedef_t *sprites;
int numSprites;

// Tempory storage, used when reading sprite definitions.
static int numSpriteRecords;
static spriterecord_t *spriteRecords;
static blockset_t *spriteRecordBlockSet, *spriteRecordFrameBlockSet;

static void clearSpriteDefs()
{
    if(numSprites <= 0) return;

    for(int i = 0; i < numSprites; ++i)
    {
        spritedef_t *sprDef = &sprites[i];
        if(sprDef->spriteFrames)
            M_Free(sprDef->spriteFrames);
    }
    M_Free(sprites); sprites = 0;
    numSprites = 0;
}

static void installSpriteLump(spriteframe_t *sprTemp, int *maxFrame,
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
        sprTemp[frame].rotate = false;
        for(int r = 0; r < 8; ++r)
        {
            sprTemp[frame].mats[r] = mat;
            sprTemp[frame].flip[r] = byte( flipped );
        }
        return;
    }

    rotation--; // Make 0 based.

    sprTemp[frame].rotate = true;
    sprTemp[frame].mats[rotation] = mat;
    sprTemp[frame].flip[rotation] = byte( flipped );
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
        rec->frames = NULL;

        rec->next = spriteRecords;
        spriteRecords = rec;
        ++numSpriteRecords;
    }

    // Add the frame(s).
    int const frameNumber    = decodedPath.at(4).toUpper().unicode() - QChar('A').unicode() + 1;
    int const rotationNumber = decodedPath.at(5).digitValue();

    bool link = false;
    spriterecord_frame_t *frame = rec->frames;
    if(rec->frames)
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
        frame->next = rec->frames;
        rec->frames = frame;
    }
}

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

/**
 * Builds the sprite rotation matrixes to account for horizontally flipped
 * sprites.  Will report an error if the lumps are inconsistant.
 *
 * Sprite lump names are 4 characters for the actor, a letter for the frame,
 * and a number for the rotation, A sprite that is flippable will have an
 * additional letter/number appended.  The rotation character can be 0 to
 * signify no rotations.
 */
static void initSpriteDefs(spriterecord_t *const *sprRecords, int num)
{
    spriteframe_t sprTemp[SPRITERECORD_MAX_FRAMES];

    clearSpriteDefs();

    numSprites = num;
    if(numSprites)
    {
        sprites = (spritedef_t *) M_Malloc(sizeof(*sprites) * numSprites);

        for(int n = 0; n < num; ++n)
        {
            spritedef_t *sprDef = &sprites[n];

            if(!sprRecords[n])
            {
                // A record for a sprite we were unable to locate.
                sprDef->numFrames    = 0;
                sprDef->spriteFrames = 0;
                continue;
            }

            std::memset(sprTemp, -1, sizeof(sprTemp));
            int maxFrame = -1;

            spriterecord_t const *rec = sprRecords[n];
            spriterecord_frame_t const *frame = rec->frames;
            do
            {
                installSpriteLump(sprTemp, &maxFrame, frame->mat, frame->frame[0] - 1,
                                  frame->rotation[0], false);
                if(frame->frame[1])
                {
                    installSpriteLump(sprTemp, &maxFrame, frame->mat, frame->frame[1] - 1,
                                      frame->rotation[1], true);
                }
            } while((frame = frame->next));

            /*
             * Check the frames that were found for completeness.
             */
            if(-1 == maxFrame)
            {
                // Should NEVER happen.
                sprDef->numFrames = 0;
            }

            ++maxFrame;
            for(int j = 0; j < maxFrame; ++j)
            {
                if(int( sprTemp[j].rotate) == -1) // Ahem!
                {
                    // No rotations were found for that frame at all.
                    Error("R_InitSprites", QString("No patches found for %1 frame %2")
                                               .arg(rec->name).arg(char(j + 'A')));
                }

                if(sprTemp[j].rotate)
                {
                    // Must have all 8 frames.
                    for(int rotation = 0; rotation < 8; ++rotation)
                    {
                        if(!sprTemp[j].mats[rotation])
                            Error("R_InitSprites", QString("Sprite %1 frame %2 is missing rotations")
                                                       .arg(rec->name).arg(char(j + 'A')));
                    }
                }
            }

            qstrncpy(sprDef->name, rec->name, 5);

            // Allocate space for the frames present and copy sprTemp to it.
            sprDef->numFrames = maxFrame;
            sprDef->spriteFrames = (spriteframe_t*) M_Malloc(sizeof(*sprDef->spriteFrames) * maxFrame);
            std::memcpy(sprDef->spriteFrames, sprTemp, sizeof(*sprDef->spriteFrames) * maxFrame);
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
        int max = MAX_OF(numSpriteRecords, countSprNames.num);
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
    clearSpriteDefs();
}

spritedef_t *R_SpriteDef(int sprite)
{
    if(sprite >= 0 && sprite < numSprites)
    {
        return &sprites[sprite];
    }
    return 0; // Not found.
}

/**
 * Lookup a sprite frame by unique @a frame index.
 */
spriteframe_t *SpriteDef_Frame(spritedef_t const &sprDef, int frame)
{
    if(frame >= 0 && frame < sprDef.numFrames)
    {
        return &sprDef.spriteFrames[frame];
    }
    return 0; // Invalid frame.
}

/**
 * Select an appropriate material for a mobj's angle and relative position with
 * that of the viewer (the 'eye').
 *
 * @param sprFrame    spriteframe_t instance.
 * @param mobjAngle   Angle of the mobj in the map coordinate space.
 * @param angleToEye  Relative angle of the mobj from the view position.
 * @param noRotation  @c true= Ignore rotations and always use the "front".
 *
 * Return values:
 * @param flipX       @c true= chosen material should be flipped on the X axis.
 * @param flipY       @c true= chosen material should be flipped on the Y axis.
 *
 * @return  The chosen material otherwise @c 0.
 */
Material *SpriteFrame_Material(spriteframe_t &sprFrame, angle_t mobjAngle,
    angle_t angleToEye, bool noRotation, bool &flipX, bool &flipY)
{
    int rotation = 0; // Use single rotation for all views (default).

    if(!noRotation && sprFrame.rotate)
    {
        // Rotation is determined by the relative angle to the viewer.
        rotation = (angleToEye - mobjAngle + (unsigned) (ANG45 / 2) * 9) >> 29;
    }

    DENG_ASSERT(rotation >= 0 && rotation < SPRITEFRAME_MAX_ANGLES);
    Material *mat = sprFrame.mats[rotation];
    flipX = CPP_BOOL(sprFrame.flip[rotation]);
    flipY = false;

    return mat;
}

Material *R_MaterialForSprite(int sprite, int frame)
{
    if(spritedef_t *sprDef = R_SpriteDef(sprite))
    {
        if(spriteframe_t *sprFrame = SpriteDef_Frame(*sprDef, frame))
        {
            bool flipX, flipY;
            return SpriteFrame_Material(*sprFrame, 0, 0, true/*no rotation*/,
                                        flipX, flipY);
        }
    }
    return 0;
}

#undef R_GetSpriteInfo
DENG_EXTERN_C boolean R_GetSpriteInfo(int sprite, int frame, spriteinfo_t *info)
{
    LOG_AS("R_GetSpriteInfo");

    spritedef_t *sprDef = R_SpriteDef(sprite);
    if(!sprDef)
    {
        LOG_WARNING("Invalid sprite number %i.") << sprite;
        zapPtr(info);
        return false;
    }

    spriteframe_t *sprFrame = SpriteDef_Frame(*sprDef, frame);
    if(!sprFrame)
    {
        LOG_WARNING("Invalid sprite frame %i.") << frame;
        zapPtr(info);
        return false;
    }

    if(novideo)
    {
        // We can't prepare the material.
        zapPtr(info);
        info->numFrames = sprDef->numFrames;
        info->flip      = sprFrame->flip[0];
        return true;
    }

    info->material  = sprFrame->mats[0];
    info->numFrames = sprDef->numFrames;
    info->flip      = sprFrame->flip[0];

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
