/** @file r_things.cpp Map Object Management.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright &copy; 1993-1996 by id Software, Inc.
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
#include <cctype>
#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_resource.h"

#include "gl/sys_opengl.h" // TODO: get rid of this
#include "world/map.h"
#include "BspLeaf"

#include "def_main.h"
#ifdef __CLIENT__
#  include "MaterialSnapshot"
#endif
#include <de/memoryblockset.h>

#include "render/r_things.h"

using namespace de;

#define MAX_FRAMES              (128)
#define MAX_OBJECT_RADIUS       (128)

typedef struct spriterecord_frame_s {
    byte frame[2];
    byte rotation[2];
    Material* mat;
    struct spriterecord_frame_s* next;
} spriterecord_frame_t;

typedef struct spriterecord_s {
    char name[5];
    int numFrames;
    spriterecord_frame_t* frames;
    struct spriterecord_s* next;
} spriterecord_t;

float weaponOffsetScale = 0.3183f; // 1/Pi
int weaponOffsetScaleY = 1000;
float weaponFOVShift = 45;
byte weaponScaleMode = SCALEMODE_SMART_STRETCH;
float modelSpinSpeed = 1;
int alwaysAlign = 0;
int noSpriteZWrite = false;
float pspOffset[2] = {0, 0};
float pspLightLevelMultiplier = 1;
// useSRVO: 1 = models only, 2 = sprites + models
int useSRVO = 2, useSRVOAngle = true;

int psp3d;

// Variables used to look up and range check sprites patches.
spritedef_t* sprites = 0;
int numSprites = 0;

vissprite_t visSprites[MAXVISSPRITES], *visSpriteP;
vispsprite_t visPSprites[DDMAXPSPRITES];

int maxModelDistance = 1500;
int levelFullBright = false;

vissprite_t visSprSortedHead;

static vissprite_t overflowVisSprite;

// Tempory storage, used when reading sprite definitions.
static int numSpriteRecords;
static spriterecord_t* spriteRecords;
static blockset_t* spriteRecordBlockSet, *spriteRecordFrameBlockSet;

static void clearSpriteDefs()
{
    int i;
    if(numSprites <= 0) return;

    for(i = 0; i < numSprites; ++i)
    {
        spritedef_t* sprDef = &sprites[i];
        if(sprDef->spriteFrames) free(sprDef->spriteFrames);
    }
    free(sprites), sprites = NULL;
    numSprites = 0;
}

static void installSpriteLump(spriteframe_t* sprTemp, int* maxFrame,
    Material* mat, uint frame, uint rotation, boolean flipped)
{
    if(frame >= 30 || rotation > 8)
    {
        return;
    }

    if((int) frame > *maxFrame)
        *maxFrame = frame;

    if(rotation == 0)
    {
        int r;
        // This frame should be used for all rotations.
        sprTemp[frame].rotate = false;
        for(r = 0; r < 8; ++r)
        {
            sprTemp[frame].mats[r] = mat;
            sprTemp[frame].flip[r] = (byte) flipped;
        }
        return;
    }

    rotation--; // Make 0 based.

    sprTemp[frame].rotate = true;
    sprTemp[frame].mats[rotation] = mat;
    sprTemp[frame].flip[rotation] = (byte) flipped;
}

static spriterecord_t* findSpriteRecordForName(char const *name)
{
    spriterecord_t* rec = NULL;
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
    clearSpriteDefs();

    numSprites = num;
    if(numSprites)
    {
        spriteframe_t sprTemp[MAX_FRAMES];
        int maxFrame, rotation, n, j;

        sprites = (spritedef_t *) M_Malloc(sizeof(*sprites) * numSprites);
        if(!sprites) Con_Error("initSpriteDefs: Failed on allocation of %lu bytes for SpriteDef list.", (unsigned long) sizeof(*sprites) * numSprites);

        for(n = 0; n < num; ++n)
        {
            spritedef_t *sprDef = &sprites[n];
            spriterecord_frame_t const *frame;
            spriterecord_t const *rec;

            if(!sprRecords[n])
            {
                // A record for a sprite we were unable to locate.
                sprDef->numFrames = 0;
                sprDef->spriteFrames = NULL;
                continue;
            }

            rec = sprRecords[n];

            memset(sprTemp, -1, sizeof(sprTemp));
            maxFrame = -1;

            frame = rec->frames;
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

            /**
             * Check the frames that were found for completeness.
             */
            if(-1 == maxFrame)
            {
                // Should NEVER happen. djs - So why is this here then?
                sprDef->numFrames = 0;
            }

            ++maxFrame;
            for(j = 0; j < maxFrame; ++j)
            {
                switch((int) sprTemp[j].rotate)
                {
                case -1: // No rotations were found for that frame at all.
                    Con_Error("R_InitSprites: No patches found for %s frame %c.", rec->name, j + 'A');
                    break;

                case 0: // Only the first rotation is needed.
                    break;

                case 1: // Must have all 8 frames.
                    for(rotation = 0; rotation < 8; ++rotation)
                    {
                        if(!sprTemp[j].mats[rotation])
                            Con_Error("R_InitSprites: Sprite %s frame %c is missing rotations.", rec->name, j + 'A');
                    }
                    break;

                default:
                    Con_Error("R_InitSpriteDefs: Invalid value, sprTemp[frame].rotate = %i.", (int) sprTemp[j].rotate);
                    exit(1); // Unreachable.
                }
            }

            // Allocate space for the frames present and copy sprTemp to it.
            strncpy(sprDef->name, rec->name, 4);
            sprDef->name[4] = '\0';
            sprDef->numFrames = maxFrame;
            sprDef->spriteFrames = (spriteframe_t*) M_Malloc(sizeof(*sprDef->spriteFrames) * maxFrame);
            if(!sprDef->spriteFrames) Con_Error("R_InitSpriteDefs: Failed on allocation of %lu bytes for sprite frame list.", (unsigned long) sizeof(*sprDef->spriteFrames) * maxFrame);

            memcpy(sprDef->spriteFrames, sprTemp, sizeof *sprDef->spriteFrames * maxFrame);
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

static inline spriteframe_t *spriteFrame(int sprite, int frame)
{
    if(spritedef_t *sprDef = R_SpriteDef(sprite))
    {
        return SpriteDef_Frame(*sprDef, frame);
    }
    return 0;
}

Material *R_MaterialForSprite(int sprite, int frame)
{
    if(spriteframe_t *sprFrame = spriteFrame(sprite, frame))
    {
        bool flipX, flipY;
        return SpriteFrame_Material(*sprFrame, 0, 0, true/*no rotation*/,
                                    flipX, flipY);
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

#ifdef __CLIENT__
static modeldef_t *currentModelDefForMobj(mobj_t *mo)
{
    // If models are being used, use the model's radius.
    if(useModels)
    {
        modeldef_t *mf = 0, *nextmf = 0;
        Models_ModelForMobj(mo, &mf, &nextmf);
        return mf;
    }
    return 0;
}

coord_t R_VisualRadius(mobj_t *mo)
{
    // If models are being used, use the model's radius.
    if(modeldef_t *mf = currentModelDefForMobj(mo))
    {
        return mf->visualRadius;
    }

    // Use the sprite frame's width?
    if(Material *material = R_MaterialForSprite(mo->sprite, mo->frame))
    {
        MaterialSnapshot const &ms = material->prepare(Rend_SpriteMaterialSpec());
        return ms.width() / 2;
    }

    // Use the physical radius.
    return mo->radius;
}

float R_ShadowStrength(mobj_t *mo)
{
    DENG_ASSERT(mo);

    float const minSpriteAlphaLimit = .1f;
    float ambientLightLevel, strength = .65f; ///< Default strength factor.

    // Is this mobj in a valid state for shadow casting?
    if(!mo->state || !mo->bspLeaf) return 0;

    // Should this mobj even have a shadow?
    if((mo->state->flags & STF_FULLBRIGHT) ||
       (mo->ddFlags & DDMF_DONTDRAW) || (mo->ddFlags & DDMF_ALWAYSLIT))
        return 0;

    // Sample the ambient light level at the mobj's position.
    if(useBias && App_World().map().hasLightGrid())
    {
        // Evaluate in the light grid.
        ambientLightLevel = App_World().map().lightGrid().evaluateLightLevel(mo->origin);
    }
    else
    {
        ambientLightLevel = mo->bspLeaf->sector().lightLevel();
        Rend_ApplyLightAdaptation(ambientLightLevel);
    }

    // Sprites have their own shadow strength factor.
    if(!currentModelDefForMobj(mo))
    {
        Material *mat = R_MaterialForSprite(mo->sprite, mo->frame);
        if(mat)
        {
            // Ensure we've prepared this.
            MaterialSnapshot const &ms = mat->prepare(Rend_SpriteMaterialSpec());

            averagealpha_analysis_t const *aa = (averagealpha_analysis_t const *) ms.texture(MTU_PRIMARY).generalCase().analysisDataPointer(Texture::AverageAlphaAnalysis);
            float weightedSpriteAlpha;
            if(!aa)
            {
                QByteArray uri = ms.texture(MTU_PRIMARY).generalCase().manifest().composeUri().asText().toUtf8();
                Con_Error("R_ShadowStrength: Texture \"%s\" has no AverageAlphaAnalysis", uri.constData());
            }

            // We use an average which factors in the coverage ratio
            // of alpha:non-alpha pixels.
            /// @todo Constant weights could stand some tweaking...
            weightedSpriteAlpha = aa->alpha * (0.4f + (1 - aa->coverage) * 0.6f);

            // Almost entirely translucent sprite? => no shadow.
            if(weightedSpriteAlpha < minSpriteAlphaLimit) return 0;

            // Apply this factor.
            strength *= MIN_OF(1, 0.2f + weightedSpriteAlpha);
        }
    }

    // Factor in Mobj alpha.
    strength *= R_Alpha(mo);

    /// @note This equation is the same as that used for fakeradio.
    return (0.6f - ambientLightLevel * 0.4f) * strength;
}
#endif // __CLIENT__

float R_Alpha(mobj_t *mo)
{
    DENG_ASSERT(mo);

    float alpha = (mo->ddFlags & DDMF_BRIGHTSHADOW)? .80f :
                  (mo->ddFlags & DDMF_SHADOW      )? .33f :
                  (mo->ddFlags & DDMF_ALTSHADOW   )? .66f : 1;
    /**
     * The three highest bits of the selector are used for alpha.
     * 0 = opaque (alpha -1)
     * 1 = 1/8 transparent
     * 4 = 1/2 transparent
     * 7 = 7/8 transparent
     */
    int selAlpha = mo->selector >> DDMOBJ_SELECTOR_SHIFT;
    if(selAlpha & 0xe0)
    {
        alpha *= 1 - ((selAlpha & 0xe0) >> 5) / 8.0f;
    }
    else if(mo->translucency)
    {
        alpha *= 1 - mo->translucency * reciprocal255;
    }
    return alpha;
}

void R_ClearVisSprites()
{
    visSpriteP = visSprites;
}

vissprite_t *R_NewVisSprite()
{
    vissprite_t *spr;

    if(visSpriteP == &visSprites[MAXVISSPRITES])
    {
        spr = &overflowVisSprite;
    }
    else
    {
        visSpriteP++;
        spr = visSpriteP - 1;
    }

    memset(spr, 0, sizeof(*spr));

    return spr;
}

#ifdef __CLIENT__

void R_ProjectPlayerSprites()
{
    int i;
    float inter;
    modeldef_t *mf, *nextmf;
    ddpsprite_t *psp;
    boolean isFullBright = (levelFullBright != 0);
    boolean isModel;
    ddplayer_t *ddpl = &viewPlayer->shared;
    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

    psp3d = false;

    // Cameramen have no psprites.
    if((ddpl->flags & DDPF_CAMERA) || (ddpl->flags & DDPF_CHASECAM))
        return;

    // Determine if we should be drawing all the psprites full bright?
    if(!isFullBright)
    {
        for(i = 0, psp = ddpl->pSprites; i < DDMAXPSPRITES; ++i, psp++)
        {
            if(!psp->statePtr) continue;

            // If one of the psprites is fullbright, both are.
            if(psp->statePtr->flags & STF_FULLBRIGHT)
                isFullBright = true;
        }
    }

    for(i = 0, psp = ddpl->pSprites; i < DDMAXPSPRITES; ++i, psp++)
    {
        vispsprite_t* spr = &visPSprites[i];

        spr->type = VPSPR_SPRITE;
        spr->psp = psp;

        if(!psp->statePtr) continue;

        // First, determine whether this is a model or a sprite.
        isModel = false;
        if(useModels)
        {
            // Is there a model for this frame?
            mobj_t dummy;

            // Setup a dummy for the call to R_CheckModelFor.
            dummy.state = psp->statePtr;
            dummy.tics = psp->tics;

            inter = Models_ModelForMobj(&dummy, &mf, &nextmf);
            if(mf) isModel = true;
        }

        if(isModel)
        {
            // Yes, draw a 3D model (in Rend_Draw3DPlayerSprites).
            // There are 3D psprites.
            psp3d = true;

            spr->type = VPSPR_MODEL;

            spr->data.model.bspLeaf = ddpl->mo->bspLeaf;
            spr->data.model.flags = 0;
            // 32 is the raised weapon height.
            spr->data.model.gzt = viewData->current.origin[VZ];
            spr->data.model.secFloor = ddpl->mo->bspLeaf->visFloorHeight();
            spr->data.model.secCeil  = ddpl->mo->bspLeaf->visCeilingHeight();
            spr->data.model.pClass = 0;
            spr->data.model.floorClip = 0;

            spr->data.model.mf = mf;
            spr->data.model.nextMF = nextmf;
            spr->data.model.inter = inter;
            spr->data.model.viewAligned = true;
            spr->origin[VX] = viewData->current.origin[VX];
            spr->origin[VY] = viewData->current.origin[VY];
            spr->origin[VZ] = viewData->current.origin[VZ];

            // Offsets to rotation angles.
            spr->data.model.yawAngleOffset = psp->pos[VX] * weaponOffsetScale - 90;
            spr->data.model.pitchAngleOffset =
                (32 - psp->pos[VY]) * weaponOffsetScale * weaponOffsetScaleY / 1000.0f;
            // Is the FOV shift in effect?
            if(weaponFOVShift > 0 && fieldOfView > 90)
                spr->data.model.pitchAngleOffset -= weaponFOVShift * (fieldOfView - 90) / 90;
            // Real rotation angles.
            spr->data.model.yaw =
                viewData->current.angle / (float) ANGLE_MAX *-360 + spr->data.model.yawAngleOffset + 90;
            spr->data.model.pitch = viewData->current.pitch * 85 / 110 + spr->data.model.yawAngleOffset;
            memset(spr->data.model.visOff, 0, sizeof(spr->data.model.visOff));

            spr->data.model.alpha = psp->alpha;
            spr->data.model.stateFullBright = (psp->flags & DDPSPF_FULLBRIGHT)!=0;
        }
        else
        {
            // No, draw a 2D sprite (in Rend_DrawPlayerSprites).
            spr->type = VPSPR_SPRITE;

            // Adjust the center slightly so an angle can be calculated.
            spr->origin[VX] = viewData->current.origin[VX];
            spr->origin[VY] = viewData->current.origin[VY];
            spr->origin[VZ] = viewData->current.origin[VZ];

            spr->data.sprite.bspLeaf = ddpl->mo->bspLeaf;
            spr->data.sprite.alpha = psp->alpha;
            spr->data.sprite.isFullBright = (psp->flags & DDPSPF_FULLBRIGHT)!=0;
        }
    }
}

#endif // __CLIENT__

float R_MovementYaw(float const mom[])
{
    // Multiply by 100 to get some artificial accuracy in bamsAtan2.
    return BANG2DEG(bamsAtan2(-100 * mom[MY], 100 * mom[MX]));
}

float R_MovementXYYaw(float momx, float momy)
{
    float mom[2] = { momx, momy };
    return R_MovementYaw(mom);
}

float R_MovementPitch(float const mom[])
{
    return BANG2DEG(bamsAtan2 (100 * mom[MZ], 100 * V2f_Length(mom)));
}

float R_MovementXYZPitch(float momx, float momy, float momz)
{
    float mom[3] = { momx, momy, momz };
    return R_MovementPitch(mom);
}

#ifdef __CLIENT__

typedef struct {
    vissprite_t *vis;
    mobj_t const *mo;
    boolean floorAdjust;
} vismobjzparams_t;

/**
 * Determine the correct Z coordinate for the mobj. The visible Z coordinate
 * may be slightly different than the actual Z coordinate due to smoothed
 * plane movement.
 *
 * @todo fixme: Should use the visual plane heights of sector clusters.
 */
int RIT_VisMobjZ(Sector *sector, void *parameters)
{
    DENG_ASSERT(sector);
    DENG_ASSERT(parameters);
    vismobjzparams_t *p = (vismobjzparams_t *) parameters;

    if(p->floorAdjust && p->mo->origin[VZ] == sector->floor().height())
    {
        p->vis->origin[VZ] = sector->floor().visHeight();
    }

    if(p->mo->origin[VZ] + p->mo->height == sector->ceiling().height())
    {
        p->vis->origin[VZ] = sector->ceiling().visHeight() - p->mo->height;
    }

    return false; // Continue iteration.
}

static void setupSpriteParamsForVisSprite(rendspriteparams_t &p,
    Vector3d const &center, coord_t distToEye, Vector3d const &visOffset,
    float /*secFloor*/, float /*secCeil*/, float /*floorClip*/, float /*top*/,
    Material &material, bool matFlipS, bool matFlipT, blendmode_t blendMode,
    Vector4f const &ambientColor,
    uint vLightListIdx, int tClass, int tMap, BspLeaf *bspLeafAtOrigin,
    bool /*floorAdjust*/, bool /*fitTop*/, bool /*fitBottom*/, bool viewAligned)
{
    MaterialVariantSpec const &spec = Rend_SpriteMaterialSpec(tClass, tMap);
    MaterialVariant *variant = material.chooseVariant(spec, true);

    DENG_ASSERT((tClass == 0 && tMap == 0) || spec.primarySpec->data.variant.translated);

    p.center[VX]       = center.x;
    p.center[VY]       = center.y;
    p.center[VZ]       = center.z;
    p.srvo[VX]         = visOffset.x;
    p.srvo[VY]         = visOffset.y;
    p.srvo[VZ]         = visOffset.z;
    p.distance         = distToEye;
    p.bspLeaf          = bspLeafAtOrigin;
    p.viewAligned      = viewAligned;
    p.noZWrite         = noSpriteZWrite;

    p.material         = variant;
    p.matFlip[0]       = matFlipS;
    p.matFlip[1]       = matFlipT;
    p.blendMode        = (useSpriteBlend? blendMode : BM_NORMAL);

    p.ambientColor[CR] = ambientColor.x;
    p.ambientColor[CG] = ambientColor.y;
    p.ambientColor[CB] = ambientColor.z;
    p.ambientColor[CA] = (useSpriteAlpha? ambientColor.w : 1);

    p.vLightListIdx    = vLightListIdx;
}

void setupModelParamsForVisSprite(rendmodelparams_t &p,
    Vector3d const &origin, coord_t distToEye, Vector3d const &visOffset,
    float gzt, float yaw, float yawAngleOffset, float pitch, float pitchAngleOffset,
    ModelDef *mf, ModelDef *nextMF, float inter,
    Vector4f const &ambientColor,
    uint vLightListIdx,
    int id, int selector, BspLeaf * /*bspLeafAtOrigin*/, int mobjDDFlags, int tmap,
    bool viewAlign, bool /*fullBright*/, bool alwaysInterpolate)
{
    p.mf                = mf;
    p.nextMF            = nextMF;
    p.inter             = inter;
    p.alwaysInterpolate = alwaysInterpolate;
    p.id                = id;
    p.selector          = selector;
    p.flags             = mobjDDFlags;
    p.tmap              = tmap;
    p.origin[VX]        = origin.x;
    p.origin[VY]        = origin.y;
    p.origin[VZ]        = origin.z;
    p.srvo[VX]          = visOffset.x;
    p.srvo[VY]          = visOffset.y;
    p.srvo[VZ]          = visOffset.z;
    p.gzt               = gzt;
    p.distance          = distToEye;
    p.yaw               = yaw;
    p.extraYawAngle     = 0;
    p.yawAngleOffset    = yawAngleOffset;
    p.pitch             = pitch;
    p.extraPitchAngle   = 0;
    p.pitchAngleOffset  = pitchAngleOffset;
    p.extraScale        = 0;
    p.viewAlign         = viewAlign;
    p.mirror            = 0;
    p.shineYawOffset    = 0;
    p.shinePitchOffset  = 0;

    p.shineTranslateWithViewerPos = p.shinepspriteCoordSpace = false;

    p.ambientColor[CR]  = ambientColor.x;
    p.ambientColor[CG]  = ambientColor.y;
    p.ambientColor[CB]  = ambientColor.z;
    p.ambientColor[CA]  = ambientColor.w;

    p.vLightListIdx     = vLightListIdx;
}

static void evaluateLighting(Vector3d const &origin, BspLeaf *bspLeafAtOrigin,
    coord_t distToEye, bool fullbright, Vector4f &ambientColor, uint *vLightListIdx)
{
    DENG_ASSERT(bspLeafAtOrigin != 0);

    if(fullbright)
    {
        ambientColor = Vector3f(1, 1, 1);
        *vLightListIdx = 0;
    }
    else
    {
        if(useBias && bspLeafAtOrigin->map().hasLightGrid())
        {
            // Evaluate the position in the light grid.
            ambientColor = bspLeafAtOrigin->map().lightGrid().evaluate(origin);
        }
        else
        {
            Sector const &sec        = bspLeafAtOrigin->sector();
            Vector3f const &secColor = Rend_SectorLightColor(sec);

            float lightLevel = sec.lightLevel();
            /* if(spr->type == VSPR_DECORATION)
            {
                // Wall decorations receive an additional light delta.
                lightLevel += R_WallAngleLightLevelDelta(line, side);
            } */

            // Apply distance attenuation.
            lightLevel = Rend_AttenuateLightLevel(distToEye, lightLevel);

            // Add extra light.
            lightLevel += Rend_ExtraLightDelta();

            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final color.
            ambientColor = secColor * lightLevel;
        }

        Rend_ApplyTorchLight(ambientColor, distToEye);

        collectaffectinglights_params_t parm; zap(parm);
        parm.origin[VX]       = origin.x;
        parm.origin[VY]       = origin.y;
        parm.origin[VZ]       = origin.z;
        parm.bspLeaf          = bspLeafAtOrigin;
        parm.ambientColor[CR] = ambientColor.x;
        parm.ambientColor[CG] = ambientColor.y;
        parm.ambientColor[CB] = ambientColor.z;

        *vLightListIdx = R_CollectAffectingLights(&parm);
    }
}

static DGLuint prepareFlaremap(de::Uri const &resourceUri)
{
    if(resourceUri.path().length() == 1)
    {
        // Select a system flare by numeric identifier?
        int number = resourceUri.path().toStringRef().first().digitValue();
        if(number == 0) return 0; // automatic
        if(number >= 1 && number <= 4)
        {
            return GL_PrepareSysFlaremap(flaretexid_t(number - 1));
        }
    }
    if(Texture *tex = R_FindTextureByResourceUri("Flaremaps", &resourceUri))
    {
        if(TextureVariant const *variant = tex->prepareVariant(Rend_HaloTextureSpec()))
        {
            return variant->glName();
        }
        // Dang...
    }
    return 0;
}

/// @todo use Mobj_OriginSmoothed
static Vector3d mobjOriginSmoothed(mobj_t *mo)
{
    DENG_ASSERT(mo != 0);
    coord_t moPos[] = { mo->origin[VX], mo->origin[VY], mo->origin[VZ] };

    // The client may have a Smoother for this object.
    if(isClient && mo->dPlayer && P_GetDDPlayerIdx(mo->dPlayer) != consolePlayer)
    {
        Smoother_Evaluate(clients[P_GetDDPlayerIdx(mo->dPlayer)].smoother, moPos);
    }

    return moPos;
}

void R_ProjectSprite(mobj_t *mo)
{
    if(!mo) return;

    // Not all objects can/will be visualized. Skip this object if:
    // ...hidden?
    if((mo->ddFlags & DDMF_DONTDRAW)) return;
    // ...not linked into the map?
    if(!mo->bspLeaf) return;
    // ...in an invalid state?
    if(!mo->state || mo->state == states) return;
    // ...no sprite frame is defined?
    spriteframe_t *sprFrame = spriteFrame(mo->sprite, mo->frame);
    if(!sprFrame) return;
    // ...fully transparent?
    float const alpha = R_Alpha(mo);
    if(alpha <= 0) return;
    // ...origin lies in a sector with no volume?
    if(!mo->bspLeaf->hasWorldVolume()) return;

    // Determine distance to object.
    Vector3d const moPos = mobjOriginSmoothed(mo);
    coord_t const distFromEye = Rend_PointDist2D(moPos);

    // Should we use a 3D model?
    modeldef_t *mf = 0, *nextmf = 0;
    float interp = 0;
    if(useModels)
    {
        interp = Models_ModelForMobj(mo, &mf, &nextmf);
        if(mf)
        {
            // Use a sprite if the object is beyond the maximum model distance.
            if(maxModelDistance && !(mf->flags & MFF_NO_DISTANCE_CHECK)
               && distFromEye > maxModelDistance)
            {
                mf = nextmf = 0;
                interp = -1;
            }
        }
    }

    // Decide which material to use according to the sprite's angle and position
    // relative to that of the viewer.
    bool matFlipS, matFlipT;
    Material *mat = SpriteFrame_Material(*sprFrame, mo->angle,
                        R_ViewPointToAngle(mo->origin),
                        mf != 0, matFlipS, matFlipT);
    if(!mat) return;

    // A valid sprite texture in the "Sprites" scheme is required.
    MaterialSnapshot const &ms = mat->prepare(Rend_SpriteMaterialSpec(mo->tclass, mo->tmap));
    if(!ms.hasTexture(MTU_PRIMARY))
        return;
    Texture &tex = ms.texture(MTU_PRIMARY).generalCase();
    if(tex.manifest().schemeName().compareWithoutCase("Sprites"))
        return;

    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
    float thangle = 0;

    // Align to the view plane? (Means scaling down Z with models)
    bool align = (mo->ddFlags & DDMF_VIEWALIGN) || alwaysAlign == 1;
    if(mf)
    {
        // Transform the origin point.
        Vector2d delta(moPos.y - viewData->current.origin[VY],
                       moPos.x - viewData->current.origin[VX]);

        thangle = BANG2RAD(bamsAtan2(delta.x * 10, delta.y * 10)) - PI / 2;
        align = false;
    }
    bool const viewAlign  = (align || alwaysAlign == 3);
    bool const fullbright = ((mo->state->flags & STF_FULLBRIGHT) != 0 || levelFullBright);

    /*
     * Perform visibility checking.
     */
    Plane &floor           = mo->bspLeaf->visFloor();
    Plane &ceiling         = mo->bspLeaf->visCeiling();
    bool const floorAdjust = (fabs(floor.visHeight() - mo->bspLeaf->floor().height()) < 8);

    // Project a line segment relative to the view in 2D, then check if not
    // entirely clipped away in the 360 degree angle clipper.
    coord_t const width = R_VisualRadius(mo) * 2; /// @todo ignorant of rotation...
    Vector2d v1, v2;
    R_ProjectViewRelativeLine2D(moPos, mf || (align || alwaysAlign == 3), width,
                                (mf? 0 : coord_t(-tex.origin().x) - (width / 2.0f)),
                                v1, v2);

    // Not visible?
    if(!C_CheckRangeFromViewRelPoints(v1, v2))
    {
        // Sprite visibility is absolute.
        if(!mf) return;

        // If the model is close to the viewpoint we should still to draw it,
        // otherwise large models are likely to disappear too early.
        Vector2d delta(distFromEye, moPos.z + (mo->height / 2) - viewData->current.origin[VZ]);
        if(M_ApproxDistance(delta.x, delta.y) > MAX_OBJECT_RADIUS)
            return;
    }

    // Store information in a vissprite.
    vissprite_t *vis = R_NewVisSprite();
    vis->type       = mf? VSPR_MODEL : VSPR_SPRITE;
    vis->origin[VX] = moPos.x;
    vis->origin[VY] = moPos.y;
    vis->origin[VZ] = moPos.z;
    vis->distance   = distFromEye;

    /**
     * The Z origin of the visual should match that of the mobj. When smoothing
     * is enabled this requires examining all touched sector planes in the vicinity.
     *
     * @todo fixme: Should use the visual planes of sector clusters.
     */
    vismobjzparams_t params;
    params.vis         = vis;
    params.mo          = mo;
    params.floorAdjust = floorAdjust;

    validCount++;
    P_MobjSectorsIterator(mo, RIT_VisMobjZ, &params);

    coord_t gzt = vis->origin[VZ] + -tex.origin().y;

    // Determine floor clipping.
    coord_t floorClip = mo->floorClip;
    if(mo->ddFlags & DDMF_BOB)
    {
        // Bobbing is applied using floorclip.
        floorClip += R_GetBobOffset(mo);
    }

    float yaw = 0, pitch = 0;
    if(mf)
    {
        // Determine the rotation angles (in degrees).
        if(mf->testSubFlag(0, MFF_ALIGN_YAW))
        {
            yaw = 90 - thangle / PI * 180;
        }
        else if(mf->testSubFlag(0, MFF_SPIN))
        {
            yaw = modelSpinSpeed * 70 * App_World().time() + MOBJ_TO_ID(mo) % 360;
        }
        else if(mf->testSubFlag(0, MFF_MOVEMENT_YAW))
        {
            yaw = R_MovementXYYaw(mo->mom[MX], mo->mom[MY]);
        }
        else
        {
            yaw = Mobj_AngleSmoothed(mo) / (float) ANGLE_MAX * -360;
        }

        // How about a unique offset?
        if(mf->testSubFlag(0, MFF_IDANGLE))
        {
            // Add an arbitrary offset.
            yaw += MOBJ_TO_ID(mo) % 360;
        }

        if(mf->testSubFlag(0, MFF_ALIGN_PITCH))
        {
            Vector2d delta((vis->origin[VZ] + gzt) / 2 - viewData->current.origin[VZ], distFromEye);
            pitch = -BANG2DEG(bamsAtan2(delta.x * 10, delta.y * 10));
        }
        else if(mf->testSubFlag(0, MFF_MOVEMENT_PITCH))
        {
            pitch = R_MovementXYZPitch(mo->mom[MX], mo->mom[MY], mo->mom[MZ]);
        }
        else
        {
            pitch = 0;
        }
    }

    // Determine possible short-range visual offset.
    Vector3d visOff;
    if((mf && useSRVO > 0) || (!mf && useSRVO > 1))
    {
        if(mo->tics >= 0)
        {
            visOff = Vector3d(mo->srvo) * (mo->tics - frameTimePos) / (float) mo->state->tics;
        }

        if(!INRANGE_OF(mo->mom[MX], 0, NOMOMENTUM_THRESHOLD) ||
           !INRANGE_OF(mo->mom[MY], 0, NOMOMENTUM_THRESHOLD) ||
           !INRANGE_OF(mo->mom[MZ], 0, NOMOMENTUM_THRESHOLD))
        {
            // Use the object's speed to calculate a short-range offset.
            visOff += Vector3d(mo->mom) * frameTimePos;
        }
    }

    if(!mf && mat)
    {
        bool const brightShadow = (mo->ddFlags & DDMF_BRIGHTSHADOW) != 0;
        bool const fitTop       = (mo->ddFlags & DDMF_FITTOP)       != 0;
        bool const fitBottom    = (mo->ddFlags & DDMF_NOFITBOTTOM)  == 0;

        // Additive blending?
        blendmode_t blendMode;
        if(brightShadow)
        {
            blendMode = BM_ADD;
        }
        // Use the "no translucency" blending mode?
        else if(noSpriteTrans && alpha >= .98f)
        {
            blendMode = BM_ZEROALPHA;
        }
        else
        {
            blendMode = BM_NORMAL;
        }

        // We must find the correct positioning using the sector floor
        // and ceiling heights as an aid.
        if(ms.height() < ceiling.visHeight() - floor.visHeight())
        {
            // Sprite fits in, adjustment possible?
            if(fitTop && gzt > ceiling.visHeight())
                gzt = ceiling.visHeight();

            if(floorAdjust && fitBottom && gzt - ms.height() < floor.visHeight())
                gzt = floor.visHeight() + ms.height();
        }
        // Adjust by the floor clip.
        gzt -= floorClip;

        Vector3d const origin(vis->origin[VX], vis->origin[VY], gzt - ms.height() / 2.0f);
        Vector4f ambientColor;
        uint vLightListIdx = 0;
        evaluateLighting(origin, mo->bspLeaf, vis->distance, fullbright,
                         ambientColor, &vLightListIdx);

        ambientColor.w = alpha;

        setupSpriteParamsForVisSprite(vis->data.sprite, origin, vis->distance, visOff,
                                      floor.visHeight(), ceiling.visHeight(),
                                      floorClip, gzt, *mat, matFlipS, matFlipT, blendMode,
                                      ambientColor,
                                      vLightListIdx,
                                      mo->tclass, mo->tmap,
                                      mo->bspLeaf,
                                      floorAdjust, fitTop, fitBottom, viewAlign);
    }
    else
    {
        Vector3d const origin(vis->origin);
        Vector4f ambientColor;
        uint vLightListIdx = 0;
        evaluateLighting(origin, mo->bspLeaf, vis->distance, fullbright,
                         ambientColor, &vLightListIdx);

        ambientColor.w = alpha;

        setupModelParamsForVisSprite(vis->data.model, origin, vis->distance,
                                     Vector3d(visOff.x, visOff.y, visOff.z - floorClip),
                                     gzt, yaw, 0, pitch, 0,
                                     mf, nextmf, interp,
                                     ambientColor,
                                     vLightListIdx, mo->thinker.id, mo->selector,
                                     mo->bspLeaf, mo->ddFlags,
                                     mo->tmap,
                                     viewAlign,
                                     fullbright && !(mf && mf->testSubFlag(0, MFF_DIM)),
                                     false);
    }

    // Do we need to project a flare source too?
    if(mo->lumIdx)
    {
        Material *mat = SpriteFrame_Material(*sprFrame, mo->angle,
                            R_ViewPointToAngle(mo->origin), false /*use rotations*/,
                            matFlipS, matFlipT);
        if(!mat) return;

        // A valid sprite texture in the "Sprites" scheme is required.
        MaterialSnapshot const &ms = mat->prepare(Rend_SpriteMaterialSpec(mo->tclass, mo->tmap));
        if(!ms.hasTexture(MTU_PRIMARY))
            return;
        Texture &tex = ms.texture(MTU_PRIMARY).generalCase();
        if(tex.manifest().schemeName().compareWithoutCase("Sprites"))
            return;

        pointlight_analysis_t const *pl = (pointlight_analysis_t const *)
            ms.texture(MTU_PRIMARY).generalCase().analysisDataPointer(Texture::BrightPointAnalysis);
        DENG_ASSERT(pl != 0);

        lumobj_t const *lum = LO_GetLuminous(mo->lumIdx);

        vissprite_t *vis = R_NewVisSprite();
        vis->type       = VSPR_FLARE;
        vis->distance   = distFromEye;

        // Determine the exact center of the flare.
        Vector3d center = moPos + visOff;
        vis->origin[VX] = center.x;
        vis->origin[VY] = center.y;
        vis->origin[VZ] = center.z + LUM_OMNI(lum)->zOff;

        float flareSize = pl->brightMul;
        // X offset to the flare position.
        float xOffset = ms.width() * pl->originX - -tex.origin().x;

        // Does the mobj have an active light definition?
        ded_light_t const *def = (mo->state? stateLights[mo->state - states] : 0);
        if(def)
        {
            if(def->size)
                flareSize = def->size;
            if(def->haloRadius)
                flareSize = def->haloRadius;
            if(def->offset[VX])
                xOffset = def->offset[VX];

            vis->data.flare.flags = def->flags;
        }

        vis->data.flare.size = flareSize * 60 * (50 + haloSize) / 100.0f;
        if(vis->data.flare.size < 8)
            vis->data.flare.size = 8;

        // Color is taken from the associated lumobj.
        V3f_Copy(vis->data.flare.color, LUM_OMNI(lum)->color);

        vis->data.flare.factor = mo->haloFactors[viewPlayer - ddPlayers];
        vis->data.flare.xOff = xOffset;
        vis->data.flare.mul = 1;
        vis->data.flare.tex = 0;

        if(def && def->flare)
        {
            de::Uri const &flaremapResourceUri = *reinterpret_cast<de::Uri const *>(def->flare);
            if(flaremapResourceUri.path().toStringRef().compareWithoutCase("-"))
            {
                vis->data.flare.tex = prepareFlaremap(flaremapResourceUri);
            }
            else
            {
                vis->data.flare.flags |= RFF_NO_PRIMARY;
            }
        }
    }
}

void R_SortVisSprites()
{
    int i, count;
    vissprite_t* ds, *best = 0;
    vissprite_t unsorted;
    coord_t bestdist;

    count = visSpriteP - visSprites;

    unsorted.next = unsorted.prev = &unsorted;
    if(!count) return;

    for(ds = visSprites; ds < visSpriteP; ds++)
    {
        ds->next = ds + 1;
        ds->prev = ds - 1;
    }
    visSprites[0].prev = &unsorted;
    unsorted.next = &visSprites[0];
    (visSpriteP - 1)->next = &unsorted;
    unsorted.prev = visSpriteP - 1;

    // Pull the vissprites out by distance.
    visSprSortedHead.next = visSprSortedHead.prev = &visSprSortedHead;

    /**
     * \todo
     * Oprofile results from nuts.wad show over 25% of total execution time
     * was spent sorting vissprites (nuts.wad map01 is a perfect pathological
     * test case).
     *
     * Rather than try to speed up the sort, it would make more sense to
     * actually construct the vissprites in z order if it can be done in
     * linear time.
     */

    for(i = 0; i < count; ++i)
    {
        bestdist = 0;
        for(ds = unsorted.next; ds != &unsorted; ds = ds->next)
        {
            if(ds->distance >= bestdist)
            {
                bestdist = ds->distance;
                best = ds;
            }
        }

        best->next->prev = best->prev;
        best->prev->next = best->next;
        best->next = &visSprSortedHead;
        best->prev = visSprSortedHead.prev;
        visSprSortedHead.prev->next = best;
        visSprSortedHead.prev = best;
    }
}

#endif // __CLIENT__

coord_t R_GetBobOffset(mobj_t* mo)
{
    if(mo->ddFlags & DDMF_BOB)
    {
        return (sin(MOBJ_TO_ID(mo) + App_World().time() / 1.8286 * 2 * PI) * 8);
    }
    return 0;
}
