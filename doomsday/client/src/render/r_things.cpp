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
#include "map/gamemap.h"

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

    LOG_INFO(String("buildSprites: Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
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

    LOG_INFO(String("R_InitSprites: Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

void R_ShutdownSprites()
{
    clearSpriteDefs();
}

Material *R_GetMaterialForSprite(int sprite, int frame)
{
    if((unsigned) sprite < (unsigned) numSprites)
    {
        spritedef_t *sprDef = &sprites[sprite];
        if(frame < sprDef->numFrames)
            return sprDef->spriteFrames[frame].mats[0];
    }
    //LOG_WARNING("R_GetMaterialForSprite: Invalid sprite %i and/or frame %i.") << sprite << frame;
    return NULL;
}

#undef R_GetSpriteInfo
DENG_EXTERN_C boolean R_GetSpriteInfo(int sprite, int frame, spriteinfo_t *info)
{
    LOG_AS("R_GetSpriteInfo");

    if((unsigned) sprite >= (unsigned) numSprites)
    {
        LOG_WARNING("Invalid sprite number %i.") << sprite;
        return false;
    }
    spritedef_t *sprDef = &sprites[sprite];

    if(frame >= sprDef->numFrames)
    {
        // We have no information to return.
        LOG_WARNING("Invalid sprite frame %i.") << frame;
        std::memset(info, 0, sizeof(*info));
        return false;
    }
    spriteframe_t *sprFrame = &sprDef->spriteFrames[frame];

    if(novideo)
    {
        // We can't prepare the material.
        std::memset(info, 0, sizeof(*info));
        info->numFrames = sprDef->numFrames;
        info->flip = sprFrame->flip[0];
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
    if(Material *material = R_GetMaterialForSprite(mo->sprite, mo->frame))
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
    if(useBias)
    {
        // Evaluate in the light grid.
        vec3d_t point; V3d_Set(point, mo->origin[VX], mo->origin[VY], mo->origin[VZ]);
        ambientLightLevel = LG_EvaluateLightLevel(point);
    }
    else
    {
        ambientLightLevel = mo->bspLeaf->sector().lightLevel();
        Rend_ApplyLightAdaptation(&ambientLightLevel);
    }

    // Sprites have their own shadow strength factor.
    if(!currentModelDefForMobj(mo))
    {
        Material *mat = R_GetMaterialForSprite(mo->sprite, mo->frame);
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
            spr->data.model.secFloor = ddpl->mo->bspLeaf->sector().floor().visHeight();
            spr->data.model.secCeil  = ddpl->mo->bspLeaf->sector().ceiling().visHeight();
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

typedef struct {
    vissprite_t *vis;
    mobj_t const *mo;
    boolean floorAdjust;
} vismobjzparams_t;

/**
 * Determine the correct Z coordinate for the mobj. The visible Z coordinate
 * may be slightly different than the actual Z coordinate due to smoothed
 * plane movement.
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

#ifdef __CLIENT__

static void setupSpriteParamsForVisSprite(rendspriteparams_t *params,
    float x, float y, float z, float distance, float visOffX, float visOffY, float visOffZ,
    float /*secFloor*/, float /*secCeil*/, float /*floorClip*/, float /*top*/,
    Material &material, boolean matFlipS, boolean matFlipT, blendmode_t blendMode,
    float ambientColorR, float ambientColorG, float ambientColorB, float alpha,
    uint vLightListIdx, int tClass, int tMap, BspLeaf *bspLeaf,
    boolean /*floorAdjust*/, boolean /*fitTop*/, boolean /*fitBottom*/, boolean viewAligned)
{
    if(!params) return; // Wha?

    MaterialVariantSpec const &spec = Rend_SpriteMaterialSpec(tClass, tMap);
    MaterialVariant *variant = material.chooseVariant(spec, true);

#ifdef DENG_DEBUG
    if(tClass || tMap)
    {
        DENG_ASSERT(spec.primarySpec->data.variant.translated);
    }
#endif

    params->center[VX] = x;
    params->center[VY] = y;
    params->center[VZ] = z;
    params->srvo[VX] = visOffX;
    params->srvo[VY] = visOffY;
    params->srvo[VZ] = visOffZ;
    params->distance = distance;
    params->bspLeaf = bspLeaf;
    params->viewAligned = viewAligned;
    params->noZWrite = noSpriteZWrite;

    params->material = variant;
    params->matFlip[0] = matFlipS;
    params->matFlip[1] = matFlipT;
    params->blendMode = (useSpriteBlend? blendMode : BM_NORMAL);

    params->ambientColor[CR] = ambientColorR;
    params->ambientColor[CG] = ambientColorG;
    params->ambientColor[CB] = ambientColorB;
    params->ambientColor[CA] = (useSpriteAlpha? alpha : 1);

    params->vLightListIdx = vLightListIdx;
}

void setupModelParamsForVisSprite(rendmodelparams_t *params,
    float x, float y, float z, float distance, float visOffX, float visOffY, float visOffZ,
    float gzt, float yaw, float yawAngleOffset, float pitch, float pitchAngleOffset,
    struct modeldef_s *mf, struct modeldef_s *nextMF, float inter,
    float ambientColorR, float ambientColorG, float ambientColorB, float alpha,
    uint vLightListIdx,
    int id, int selector, BspLeaf * /*bspLeaf*/, int mobjDDFlags, int tmap,
    boolean viewAlign, boolean /*fullBright*/, boolean alwaysInterpolate)
{
    if(!params)
        return; // Hmm...

    params->mf = mf;
    params->nextMF = nextMF;
    params->inter = inter;
    params->alwaysInterpolate = alwaysInterpolate;
    params->id = id;
    params->selector = selector;
    params->flags = mobjDDFlags;
    params->tmap = tmap;
    params->origin[VX] = x;
    params->origin[VY] = y;
    params->origin[VZ] = z;
    params->srvo[VX] = visOffX;
    params->srvo[VY] = visOffY;
    params->srvo[VZ] = visOffZ;
    params->gzt = gzt;
    params->distance = distance;
    params->yaw = yaw;
    params->extraYawAngle = 0;
    params->yawAngleOffset = yawAngleOffset;
    params->pitch = pitch;
    params->extraPitchAngle = 0;
    params->pitchAngleOffset = pitchAngleOffset;
    params->extraScale = 0;
    params->viewAlign = viewAlign;
    params->mirror = 0;
    params->shineYawOffset = 0;
    params->shinePitchOffset = 0;
    params->shineTranslateWithViewerPos = false;
    params->shinepspriteCoordSpace = false;

    params->ambientColor[CR] = ambientColorR;
    params->ambientColor[CG] = ambientColorG;
    params->ambientColor[CB] = ambientColorB;
    params->ambientColor[CA] = alpha;

    params->vLightListIdx = vLightListIdx;
}

void getLightingParams(coord_t x, coord_t y, coord_t z, BspLeaf *bspLeaf,
    coord_t distance, boolean fullBright, float ambientColor[3], uint *vLightListIdx)
{
    if(fullBright)
    {
        ambientColor[CR] = ambientColor[CG] = ambientColor[CB] = 1;
        *vLightListIdx = 0;
    }
    else
    {
        collectaffectinglights_params_t lparams;

        if(useBias)
        {
            vec3d_t point;

            // Evaluate the position in the light grid.
            V3d_Set(point, x, y, z);
            LG_Evaluate(point, ambientColor);
        }
        else
        {
            Sector &sec = bspLeaf->sector();
            float lightLevel = sec.lightLevel();
            float const *secColor = R_GetSectorLightColor(&sec);

            /* if(spr->type == VSPR_DECORATION)
            {
                // Wall decorations receive an additional light delta.
                lightLevel += R_WallAngleLightLevelDelta(linedef, side);
            } */

            // Apply distance attenuation.
            lightLevel = R_DistAttenuateLightLevel(distance, lightLevel);

            // Add extra light.
            lightLevel += R_ExtraLightDelta();

            Rend_ApplyLightAdaptation(&lightLevel);

            // Determine the final ambientColor in affect.
            ambientColor[CR] = lightLevel * secColor[CR];
            ambientColor[CG] = lightLevel * secColor[CG];
            ambientColor[CB] = lightLevel * secColor[CB];
        }

        Rend_ApplyTorchLight(ambientColor, distance);

        lparams.starkLight = false;
        lparams.origin[VX] = x;
        lparams.origin[VY] = y;
        lparams.origin[VZ] = z;
        lparams.bspLeaf = bspLeaf;
        lparams.ambientColor = ambientColor;

        *vLightListIdx = R_CollectAffectingLights(&lparams);
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

void R_ProjectSprite(mobj_t *mo)
{
    float thangle = 0, alpha, yaw = 0, pitch = 0;
    coord_t distance, gzt, floorClip;
    vec3d_t visOff;
    spritedef_t *sprDef;
    spriteframe_t *sprFrame = NULL;
    boolean matFlipS, matFlipT;
    vissprite_t *vis;
    boolean align, fullBright, viewAlign;
    modeldef_t *mf = 0, *nextmf = 0;
    float interp = 0;
    vismobjzparams_t params;
    visspritetype_t visType = VSPR_SPRITE;
    float ambientColor[3];
    uint vLightListIdx = 0;
    Material *mat;
    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
    coord_t moPos[3];

    if(!mo) return;

    // Never make a vissprite when DDMF_DONTDRAW is set or when when the mobj
    // is in an invalid state.
    if((mo->ddFlags & DDMF_DONTDRAW) || !mo->state || mo->state == states) return;

    Sector &moSec       = mo->bspLeaf->sector();
    coord_t secFloor    = moSec.floor().visHeight();
    coord_t secCeil     = moSec.ceiling().visHeight();
    boolean floorAdjust = (fabs(moSec.floor().visHeight() - moSec.floor().height()) < 8);

    // Never make a vissprite when the mobj's origin sector is of zero height.
    if(secFloor >= secCeil) return;

    alpha = R_Alpha(mo);
    // Never make a vissprite when the mobj is fully transparent.
    if(alpha <= 0) return;

    moPos[VX] = mo->origin[VX];
    moPos[VY] = mo->origin[VY];
    moPos[VZ] = mo->origin[VZ];

    // The client may have a Smoother for this object.
    if(isClient && mo->dPlayer && P_GetDDPlayerIdx(mo->dPlayer) != consolePlayer)
    {
        Smoother_Evaluate(clients[P_GetDDPlayerIdx(mo->dPlayer)].smoother, moPos);
    }

    // Decide which patch to use according to the sprite's angle and position
    // relative to that of the viewer.

#ifdef RANGECHECK
    if(mo->sprite < 0 || (unsigned) mo->sprite >= (unsigned) numSprites)
    {
        Con_Error("R_ProjectSprite: invalid sprite number %i\n", mo->sprite);
    }
#endif
    sprDef = &sprites[mo->sprite];

    // If the frame is not defined, we can't display this object.
    if(mo->frame >= sprDef->numFrames) return;
    sprFrame = &sprDef->spriteFrames[mo->frame];

    // Determine distance to object.
    distance = Rend_PointDist2D(moPos);

    // Should we use a 3D model?
    if(useModels)
    {
        interp = Models_ModelForMobj(mo, &mf, &nextmf);
        if(mf && !(mf->flags & MFF_NO_DISTANCE_CHECK) && maxModelDistance &&
           distance > maxModelDistance)
        {
            // Don't use a 3D model.
            mf = nextmf = NULL;
            interp = -1;
        }
    }
    visType = !mf? VSPR_SPRITE : VSPR_MODEL;

    if(sprFrame->rotate && !mf)
    {
        // Choose a different rotation according to the relative angle to the viewer.
        angle_t ang = R_ViewPointXYToAngle(mo->origin[VX], mo->origin[VY]);
        uint rot = (ang - mo->angle + (unsigned) (ANG45 / 2) * 9) >> 29;
        mat = sprFrame->mats[rot];
        matFlipS = (boolean) sprFrame->flip[rot];
    }
    else
    {
        // Use single rotation for all views.
        mat = sprFrame->mats[0];
        matFlipS = (boolean) sprFrame->flip[0];
    }
    matFlipT = false;

    MaterialSnapshot const &ms = mat->prepare(Rend_SpriteMaterialSpec(mo->tclass, mo->tmap));

    // An invalid sprite texture?
    Texture &tex = ms.texture(MTU_PRIMARY).generalCase();
    if(tex.manifest().schemeName().compareWithoutCase("Sprites")) return;

    // Align to the view plane?
    align = (mo->ddFlags & DDMF_VIEWALIGN) || alwaysAlign == 1;
    if(mf)
    {
        // Transform the origin point.
        coord_t delta[2] = { moPos[VY] - viewData->current.origin[VY],
                             moPos[VX] - viewData->current.origin[VX] };

        thangle = BANG2RAD(bamsAtan2(delta[0] * 10, delta[1] * 10)) - PI / 2;
        // View-aligning means scaling down Z with models.
        align = false;
    }

    // Perform visibility checking.
    /// @todo R_VisualRadius() does not consider sprite rotation.
    coord_t const width = R_VisualRadius(mo) * 2;
    coord_t const offset = (mf? 0 : coord_t(-tex.origin().x) - (width / 2.0f));
    coord_t v1[2], v2[2];

    // Project a line segment relative to the view in 2D, then check if not
    // entirely clipped away in the 360 degree angle clipper.
    R_ProjectViewRelativeLine2D(moPos, mf || (align || alwaysAlign == 3), width, offset, v1, v2);

    // Check for visibility.
    if(!C_CheckRangeFromViewRelPoints(v1, v2))
    {
        // Not visible.
        coord_t delta[2];

        // Sprite visibility is absolute.
        if(!mf) return;

        // If the model is close to the viewpoint we will need to draw it.
        // Otherwise large models are likely to disappear too early.
        delta[0] = distance;
        delta[1] = moPos[VZ] + (mo->height / 2) - viewData->current.origin[VZ];
        if(M_ApproxDistance(delta[0], delta[1]) > MAX_OBJECT_RADIUS) return;
    }

    // Store information in a vissprite.
    vis = R_NewVisSprite();
    vis->type = visType;
    vis->origin[VX] = moPos[VX];
    vis->origin[VY] = moPos[VY];
    vis->origin[VZ] = moPos[VZ];
    vis->distance = distance;

    /**
     * The mobj's Z coordinate must match the actual visible floor/ceiling
     * height.  When using smoothing, this requires iterating through the
     * sectors (planes) in the vicinity.
     */
    validCount++;
    params.vis = vis;
    params.mo = mo;
    params.floorAdjust = floorAdjust;
    P_MobjSectorsIterator(mo, RIT_VisMobjZ, &params);

    gzt = vis->origin[VZ] + -tex.origin().y;

    viewAlign  = (align || alwaysAlign == 3)? true : false;
    fullBright = ((mo->state->flags & STF_FULLBRIGHT) || levelFullBright)? true : false;

    // Foot clipping.
    floorClip = mo->floorClip;
    if(mo->ddFlags & DDMF_BOB)
    {
        // Bobbing is applied to the floorclip.
        floorClip += R_GetBobOffset(mo);
    }

    if(mf)
    {
        // Determine the rotation angles (in degrees).
        if(mf->sub[0].flags & MFF_ALIGN_YAW)
        {
            yaw = 90 - thangle / PI * 180;
        }
        else if(mf->sub[0].flags & MFF_SPIN)
        {
            yaw = modelSpinSpeed * 70 * ddMapTime + MOBJ_TO_ID(mo) % 360;
        }
        else if(mf->sub[0].flags & MFF_MOVEMENT_YAW)
        {
            yaw = R_MovementXYYaw(mo->mom[MX], mo->mom[MY]);
        }
        else
        {
            yaw = Mobj_AngleSmoothed(mo) / (float) ANGLE_MAX * -360;
        }

        // How about a unique offset?
        if(mf->sub[0].flags & MFF_IDANGLE)
        {
            // Add an arbitrary offset.
            yaw += MOBJ_TO_ID(mo) % 360;
        }

        if(mf->sub[0].flags & MFF_ALIGN_PITCH)
        {
            coord_t delta[2];
            delta[0] = (vis->origin[VZ] + gzt) / 2 - viewData->current.origin[VZ];
            delta[1] = distance;
            pitch = -BANG2DEG(bamsAtan2(delta[0] * 10, delta[1] * 10));
        }
        else if(mf->sub[0].flags & MFF_MOVEMENT_PITCH)
        {
            pitch = R_MovementXYZPitch(mo->mom[MX], mo->mom[MY], mo->mom[MZ]);
        }
        else
        {
            pitch = 0;
        }
    }

    // Determine possible short-range visual offset.
    V3d_Set(visOff, 0, 0, 0);

    if((mf && useSRVO > 0) || (!mf && useSRVO > 1))
    {
        if(mo->state && mo->tics >= 0)
        {
            V3d_Set(visOff, mo->srvo[VX], mo->srvo[VY], mo->srvo[VZ]);
            V3d_Scale(visOff, (mo->tics - frameTimePos) / (float) mo->state->tics);
        }

        if(!INRANGE_OF(mo->mom[MX], 0, NOMOMENTUM_THRESHOLD) ||
           !INRANGE_OF(mo->mom[MY], 0, NOMOMENTUM_THRESHOLD) ||
           !INRANGE_OF(mo->mom[MZ], 0, NOMOMENTUM_THRESHOLD))
        {
            vec3d_t tmp;

            // Use the object's speed to calculate a short-range offset.
            V3d_Set(tmp, mo->mom[MX], mo->mom[MY], mo->mom[MZ]);
            V3d_Scale(tmp, frameTimePos);

            V3d_Sum(visOff, visOff, tmp);
        }
    }

    if(!mf && mat)
    {
        const boolean brightShadow = !!(mo->ddFlags & DDMF_BRIGHTSHADOW);
        const boolean fitTop       = !!(mo->ddFlags & DDMF_FITTOP);
        const boolean fitBottom    = !(mo->ddFlags & DDMF_NOFITBOTTOM);
        blendmode_t blendMode;

        // Additive blending?
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
        if(ms.height() < secCeil - secFloor)
        {
            // Sprite fits in, adjustment possible?
            // Check top.
            if(fitTop && gzt > secCeil)
                gzt = secCeil;
            // Check bottom.
            if(floorAdjust && fitBottom && gzt - ms.height() < secFloor)
                gzt = secFloor + ms.height();
        }
        // Adjust by the floor clip.
        gzt -= floorClip;

        getLightingParams(vis->origin[VX], vis->origin[VY],
                          gzt - ms.height() / 2.0f,
                          mo->bspLeaf, vis->distance, fullBright,
                          ambientColor, &vLightListIdx);

        setupSpriteParamsForVisSprite(&vis->data.sprite,
                                      vis->origin[VX], vis->origin[VY],
                                      gzt - ms.height() / 2.0f,
                                      vis->distance,
                                      visOff[VX], visOff[VY], visOff[VZ],
                                      secFloor, secCeil,
                                      floorClip, gzt, *mat, matFlipS, matFlipT, blendMode,
                                      ambientColor[CR], ambientColor[CG], ambientColor[CB], alpha,
                                      vLightListIdx,
                                      mo->tclass, mo->tmap,
                                      mo->bspLeaf,
                                      floorAdjust, fitTop, fitBottom, viewAlign);
    }
    else
    {
        getLightingParams(vis->origin[VX], vis->origin[VY], vis->origin[VZ],
                          mo->bspLeaf, vis->distance, fullBright,
                          ambientColor, &vLightListIdx);

        setupModelParamsForVisSprite(&vis->data.model,
                                     vis->origin[VX], vis->origin[VY], vis->origin[VZ], vis->distance,
                                     visOff[VX], visOff[VY], visOff[VZ] - floorClip, gzt, yaw, 0, pitch, 0,
                                     mf, nextmf, interp,
                                     ambientColor[CR], ambientColor[CG], ambientColor[CB], alpha,
                                     vLightListIdx, mo->thinker.id, mo->selector,
                                     mo->bspLeaf, mo->ddFlags,
                                     mo->tmap,
                                     viewAlign,
                                     fullBright && !(mf && (mf->sub[0].flags & MFF_DIM)),
                                     false);
    }

    // Do we need to project a flare source too?
    if(mo->lumIdx)
    {
        // Determine the sprite frame lump of the source.
        spritedef_t *sprDef     = &sprites[mo->sprite];
        spriteframe_t *sprFrame = &sprDef->spriteFrames[mo->frame];

        Material *mat;
        if(sprFrame->rotate)
        {
            mat = sprFrame->mats[(R_ViewPointXYToAngle(moPos[VX], moPos[VY]) - mo->angle + (unsigned) (ANG45 / 2) * 9) >> 29];
        }
        else
        {
            mat = sprFrame->mats[0];
        }

#if _DEBUG
        if(!mat) Con_Error("R_ProjectSprite: Sprite '%i' frame '%i' missing material.", (int) mo->sprite, mo->frame);
#endif

        // Ensure we have up-to-date information about the material.
        MaterialSnapshot const &ms = mat->prepare(Rend_SpriteMaterialSpec());

        pointlight_analysis_t const *pl = (pointlight_analysis_t const *) ms.texture(MTU_PRIMARY).generalCase().analysisDataPointer(Texture::BrightPointAnalysis);
        if(!pl)
        {
            QByteArray uri = ms.texture(MTU_PRIMARY).generalCase().manifest().composeUri().asText().toUtf8();
            Con_Error("R_ProjectSprite: Texture \"%s\" has no BrightPointAnalysis", uri.constData());
        }

        lumobj_t const *lum = LO_GetLuminous(mo->lumIdx);
        ded_light_t const *def = (mo->state? stateLights[mo->state - states] : 0);

        vis = R_NewVisSprite();
        vis->type = VSPR_FLARE;
        vis->distance = distance;

        // Determine the exact center of the flare.
        V3d_Sum(vis->origin, moPos, visOff);
        vis->origin[VZ] += LUM_OMNI(lum)->zOff;

        float flareSize = pl->brightMul;
        // X offset to the flare position.
        float xOffset = ms.width() * pl->originX - -tex.origin().x;

        // Does the mobj have an active light definition?
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
        if(vis->data.flare.size < 8) vis->data.flare.size = 8;

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

typedef struct {
    BspLeaf *bspLeaf;
} addspriteparams_t;

int RIT_AddSprite(void *ptr, void *parameters)
{
    GameMap *map = theMap; /// @todo Do not assume mobj is from the CURRENT map.

    mobj_t *mo = (mobj_t *) ptr;
    addspriteparams_t *params = (addspriteparams_t *)parameters;

    if(mo->addFrameCount != frameCount)
    {
        Sector &sec = params->bspLeaf->sector();
        R_ProjectSprite(mo);

        // Hack: Sprites have a tendency to extend into the ceiling in
        // sky sectors. Here we will raise the skyfix dynamically, to make sure
        // that no sprites get clipped by the sky.
        // Only check
        Material *material = R_GetMaterialForSprite(mo->sprite, mo->frame);
        if(material && sec.ceilingSurface().hasSkyMaskedMaterial())
        {
            if(!(mo->dPlayer && mo->dPlayer->flags & DDPF_CAMERA) && // Cameramen don't exist!
               mo->origin[VZ] <= sec.ceiling().height() &&
               mo->origin[VZ] >= sec.floor().height())
            {
                coord_t visibleTop = mo->origin[VZ] + material->height();
                if(visibleTop > map->skyFixCeiling())
                {
                    // Raise skyfix ceiling.
                    map->setSkyFixCeiling(visibleTop + 16/*leeway*/);
                }
            }
        }
        mo->addFrameCount = frameCount;
    }
    return false; // Continue iteration.
}

void R_AddSprites(BspLeaf *bspLeaf)
{
    addspriteparams_t params;

    // Do not use validCount because other parts of the renderer may change it.
    if(bspLeaf->addSpriteCount() == frameCount)
        return; // Already added.

    params.bspLeaf = bspLeaf;
    R_IterateBspLeafContacts2(bspLeaf, OT_MOBJ, RIT_AddSprite, &params);

    bspLeaf->_addSpriteCount = frameCount;
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
        return (sin(MOBJ_TO_ID(mo) + ddMapTime / 1.8286 * 2 * PI) * 8);
    }
    return 0;
}
