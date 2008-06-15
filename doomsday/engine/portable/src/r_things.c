/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * r_things.c: Object Management and Refresh
 */

/**
 * Sprite rotation 0 is facing the viewer, rotation 1 is one angle
 * turn CLOCKWISE around the axis. This is not the same as the angle,
 * which increases counter clockwise (protractor).
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"

#include "def_main.h"

// MACROS ------------------------------------------------------------------

#define MAX_FRAMES              128
#define MAX_OBJECT_RADIUS       128

#define MAX_VISSPRITE_LIGHTS    10

// TYPES -------------------------------------------------------------------

typedef struct {
    float           pos[3];
    float*          ambientColor;
    uint*           numLights;
    uint            maxLights;
} vlightiterparams_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float   weaponOffsetScale = 0.3183f;    // 1/Pi
int     weaponOffsetScaleY = 1000;
float   weaponFOVShift = 45;
float   modelSpinSpeed = 1;
int     alwaysAlign = 0;
int     noSpriteZWrite = false;
float   pspOffset[2] = {0, 0};
// useSRVO: 1 = models only, 2 = sprites + models
int     useSRVO = 2, useSRVOAngle = true;
int     psp3d;

// Variables used to look up and range check sprites patches.
spritedef_t *sprites = 0;
int     numSprites;

vissprite_t visSprites[MAXVISSPRITES], *visSpriteP;
vispsprite_t visPSprites[DDMAXPSPRITES];

int     maxModelDistance = 1500;
int     levelFullBright = false;

vissprite_t visSprSortedHead;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static spriteframe_t sprTemp[MAX_FRAMES];
static int maxFrame;
static char *spriteName;

static vissprite_t overflowVisSprite;
static mobj_t *projectedMobj;  // Used during RIT_VisMobjZ

static const float worldLight[3] = {-.400891f, -.200445f, .601336f};

static vlight_t lights[MAX_VISSPRITE_LIGHTS] = {
    // The first light is the world light.
    {false, 0, NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Local function for R_InitSprites.
 */
static void installSpriteLump(lumpnum_t lump, uint frame, uint rotation,
                              boolean flipped)
{
    int                 r;
    material_t         *mat;
    int                 idx = R_NewSpriteTexture(lump, &mat);

    if(frame >= 30 || rotation > 8)
    {
        /*Con_Error("installSpriteLump: Bad frame characters in lump %i",
                    (int) lump);*/
        return;
    }

    if((int) frame > maxFrame)
        maxFrame = frame;

    if(rotation == 0)
    {
        // The lump should be used for all rotations.
        sprTemp[frame].rotate = false;
        for(r = 0; r < 8; ++r)
        {
            sprTemp[frame].mats[r] = mat;
            sprTemp[frame].flip[r] = (byte) flipped;
        }

        return;
    }

    sprTemp[frame].rotate = true;

    rotation--; // Make 0 based.

    sprTemp[frame].mats[rotation] = mat;
    sprTemp[frame].flip[rotation] = (byte) flipped;
}

/**
 * Pass a null terminated list of sprite names (4 chars exactly) to be used
 * Builds the sprite rotation matrixes to account for horizontally flipped
 * sprites.  Will report an error if the lumps are inconsistant.
 *
 * Sprite lump names are 4 characters for the actor, a letter for the frame,
 * and a number for the rotation, A sprite that is flippable will have an
 * additional letter/number appended.  The rotation character can be 0 to
 * signify no rotations.
 */
void R_InitSpriteDefs(void)
{
    int                 i, l, intname, frame, rotation;
    boolean             inSpriteBlock;

    numSpriteTextures = 0;
    numSprites = countSprNames.num;

    // Check that some sprites are defined.
    if(!numSprites)
        return;

    sprites = Z_Malloc(numSprites * sizeof(*sprites), PU_SPRITE, NULL);

    // Scan all the lump names for each of the names, noting the highest
    // frame letter. Just compare 4 characters as ints.
    for(i = 0; i < numSprites; ++i)
    {
        spriteName = sprNames[i].name;
        memset(sprTemp, -1, sizeof(sprTemp));

        maxFrame = -1;
        intname = *(int *) spriteName;

        //
        // scan the lumps, filling in the frames for whatever is found
        //
        inSpriteBlock = false;
        for(l = 0; l < numLumps; l++)
        {
            char               *name = lumpInfo[l].name;

            if(!strnicmp(name, "S_START", 7))
            {
                // We've arrived at *a* sprite block.
                inSpriteBlock = true;
                continue;
            }
            else if(!strnicmp(name, "S_END", 5))
            {
                // The sprite block ends.
                inSpriteBlock = false;
                continue;
            }

            if(!inSpriteBlock)
                continue;

            // Check that the first four letters match the sprite name.
            if(*(int *) name == intname)
            {
                // Check that the name is valid.
                if(!name[4] || !name[5] || (name[6] && !name[7]))
                    continue;   // This is not a sprite frame.

                // Indices 5 and 7 must be numbers (0-8).
                if(name[5] < '0' || name[5] > '8')
                    continue;

                if(name[7] && (name[7] < '0' || name[7] > '8'))
                    continue;

                frame = name[4] - 'A';
                rotation = name[5] - '0';
                installSpriteLump(l, frame, rotation, false);
                if(name[6])
                {
                    frame = name[6] - 'A';
                    rotation = name[7] - '0';
                    installSpriteLump(l, frame, rotation, true);
                }
            }
        }

        /**
         * Check the frames that were found for completeness.
         */
        if(maxFrame == -1)
        {
            sprites[i].numFrames = 0;
            //Con_Error ("R_InitSprites: No lumps found for sprite %s", namelist[i]);
        }

        maxFrame++;
        for(frame = 0; frame < maxFrame; frame++)
        {
            switch((int) sprTemp[frame].rotate)
            {
            case -1: // No rotations were found for that frame at all.
                Con_Error("R_InitSprites: No patches found for %s frame %c",
                          spriteName, frame + 'A');
                break;

            case 0: // Only the first rotation is needed.
                break;

            case 1: // Must have all 8 frames.
                for(rotation = 0; rotation < 8; rotation++)
                    if(!sprTemp[frame].mats[rotation])
                        Con_Error("R_InitSprites: Sprite %s frame %c is "
                                  "missing rotations", spriteName,
                                  frame + 'A');
                break;

            default:
                Con_Error("R_InitSpriteDefs: Invalid value, "
                          "sprTemp[frame].rotate = %i.",
                          (int) sprTemp[frame].rotate);
                break;
            }
        }

        // The model definitions might have a larger max frame for this
        // sprite.
        /*highframe = R_GetMaxMDefSpriteFrame(spriteName) + 1;
        if(highframe > maxFrame)
        {
            maxFrame = highframe;
            for(l = 0; l < maxFrame; ++l)
                for(frame = 0; frame < 8; frame++)
                    if(!sprTemp[l].mats[frame])
                        sprTemp[l].mats[frame] = 0;
        } */

        // Allocate space for the frames present and copy sprTemp to it.
        sprites[i].numFrames = maxFrame;
        sprites[i].spriteFrames =
            Z_Malloc(maxFrame * sizeof(spriteframe_t), PU_SPRITE, NULL);
        memcpy(sprites[i].spriteFrames, sprTemp,
               maxFrame * sizeof(spriteframe_t));
        // The possible model frames are initialized elsewhere.
        //sprites[i].modeldef = -1;
    }
}

void R_GetSpriteInfo(int sprite, int frame, spriteinfo_t *info)
{
    spritedef_t        *sprDef;
    spriteframe_t      *sprFrame;
    spritetex_t        *sprTex;

#ifdef RANGECHECK
    if((unsigned) sprite >= (unsigned) numSprites)
        Con_Error("R_GetSpriteInfo: invalid sprite number %i.\n", sprite);
#endif

    sprDef = &sprites[sprite];

    if(frame >= sprDef->numFrames)
    {
        // We have no information to return.
        memset(info, 0, sizeof(*info));
        return;
    }

    sprFrame = &sprDef->spriteFrames[frame];
    sprTex = spriteTextures[sprFrame->mats[0]->ofTypeID];

    info->numFrames = sprDef->numFrames;
    info->idx = sprFrame->mats[0]->ofTypeID;
    info->realLump = sprTex->lump;
    info->flip = sprFrame->flip[0];
    info->offset = sprTex->info.offsetX;
    info->topOffset = sprTex->info.offsetY;
    info->width = sprTex->info.width;
    info->height = sprTex->info.height;
}

void R_GetPatchInfo(lumpnum_t lump, patchinfo_t *info)
{
    lumppatch_t        *patch =
        (lumppatch_t *) W_CacheLumpNum(lump, PU_CACHE);

    memset(info, 0, sizeof(*info));
    info->lump = info->realLump = lump;
    info->width = SHORT(patch->width);
    info->height = SHORT(patch->height);
    info->topOffset = SHORT(patch->topOffset);
    info->offset = SHORT(patch->leftOffset);
}

/**
 * @return              Radius of the mobj as it would visually appear to be.
 */
float R_VisualRadius(mobj_t *mo)
{
    modeldef_t         *mf, *nextmf;
    spriteinfo_t        sprinfo;

    // If models are being used, use the model's radius.
    if(useModels)
    {
        R_CheckModelFor(mo, &mf, &nextmf);
        if(mf)
        {
            // Returns the model's radius!
            return mf->visualRadius;
        }
    }

    // Use the sprite frame's width.
    R_GetSpriteInfo(mo->sprite, mo->frame, &sprinfo);
    return sprinfo.width / 2;
}

void R_InitSprites(void)
{
    // Free all previous sprite memory.
    Z_FreeTags(PU_SPRITE, PU_SPRITE);
    R_InitSpriteDefs();
    R_InitSpriteTextures();
}

/**
 * Called at frame start.
 */
void R_ClearSprites(void)
{
    visSpriteP = visSprites;
}

vissprite_t *R_NewVisSprite(void)
{
    if(visSpriteP == &visSprites[MAXVISSPRITES])
        return &overflowVisSprite;

    visSpriteP++;
    return visSpriteP - 1;
}

/**
 * If 3D models are found for psprites, here we will create vissprites
 * for them.
 */
void R_ProjectPlayerSprites(void)
{
    int                 i;
    float               inter;
    modeldef_t         *mf, *nextmf;
    ddpsprite_t        *psp;
    boolean             isFullBright = (levelFullBright != 0);
    boolean             isModel;
    ddplayer_t         *ddpl = &viewPlayer->shared;

    psp3d = false;

    // Cameramen have no psprites.
    if((ddpl->flags & DDPF_CAMERA) || (ddpl->flags & DDPF_CHASECAM))
        return;

    // Determine if we should be drawing all the psprites full bright?
    if(!isFullBright)
    {
        for(i = 0, psp = ddpl->pSprites; i < DDMAXPSPRITES; ++i, psp++)
        {
            if(!psp->statePtr)
                continue;

            // If one of the psprites is fullbright, both are.
            if(psp->statePtr->flags & STF_FULLBRIGHT)
                isFullBright = true;
        }
    }

    for(i = 0, psp = ddpl->pSprites; i < DDMAXPSPRITES; ++i, psp++)
    {
        vispsprite_t        *spr = &visPSprites[i];

        spr->type = VPSPR_SPRITE;
        spr->psp = psp;

        if(!psp->statePtr)
            continue;

        // First, determine whether this is a model or a sprite.
        isModel = false;
        if(useModels)
        {   // Is there a model for this frame?
            mobj_t      dummy;

            // Setup a dummy for the call to R_CheckModelFor.
            dummy.state = psp->statePtr;
            dummy.tics = psp->tics;

            inter = R_CheckModelFor(&dummy, &mf, &nextmf);
            if(mf)
                isModel = true;
        }

        if(isModel)
        {   // Yes, draw a 3D model (in Rend_Draw3DPlayerSprites).

            // There are 3D psprites.
            psp3d = true;

            spr->type = VPSPR_MODEL;

            spr->data.model.subsector = ddpl->mo->subsector;
            spr->data.model.flags = 0;
            // 32 is the raised weapon height.
            spr->data.model.gzt = viewZ;
            spr->data.model.secFloor = ddpl->mo->subsector->sector->SP_floorvisheight;
            spr->data.model.secCeil = ddpl->mo->subsector->sector->SP_ceilvisheight;
            spr->data.model.pClass = 0;
            spr->data.model.floorClip = 0;

            spr->data.model.mf = mf;
            spr->data.model.nextMF = nextmf;
            spr->data.model.inter = inter;
            spr->data.model.viewAligned = true;
            spr->center[VX] = viewX;
            spr->center[VY] = viewY;
            spr->center[VZ] = viewZ;

            // Offsets to rotation angles.
            spr->data.model.yawAngleOffset = psp->pos[VX] * weaponOffsetScale - 90;
            spr->data.model.pitchAngleOffset =
                (32 - psp->pos[VY]) * weaponOffsetScale * weaponOffsetScaleY / 1000.0f;
            // Is the FOV shift in effect?
            if(weaponFOVShift > 0 && fieldOfView > 90)
                spr->data.model.yawAngleOffset -= weaponFOVShift * (fieldOfView - 90) / 90;
            // Real rotation angles.
            spr->data.model.yaw =
                viewAngle / (float) ANGLE_MAX *-360 + spr->data.model.yawAngleOffset + 90;
            spr->data.model.pitch = viewPitch * 85 / 110 + spr->data.model.yawAngleOffset;
            memset(spr->data.model.visOff, 0, sizeof(spr->data.model.visOff));

            spr->data.model.alpha = psp->alpha;
            spr->data.model.stateFullBright = false;
        }
        else
        {   // No, draw a 2D sprite (in Rend_DrawPlayerSprites).
            spr->type = VPSPR_SPRITE;

            // Adjust the center slightly so an angle can be calculated.
            spr->center[VX] = viewX;
            spr->center[VY] = viewY;
            spr->center[VZ] = viewZ;

            spr->data.sprite.subsector = ddpl->mo->subsector;
            spr->data.sprite.alpha = psp->alpha;
            spr->data.sprite.isFullBright = isFullBright;
        }
    }
}

float R_MovementYaw(float momx, float momy)
{
    // Multiply by 100 to get some artificial accuracy in bamsAtan2.
    return BANG2DEG(bamsAtan2(-100 * momy, 100 * momx));
}

float R_MovementPitch(float momx, float momy, float momz)
{
    return
        BANG2DEG(bamsAtan2
                 (100 * momz, 100 * P_AccurateDistance(momx, momy)));
}

/**
 * Determine the correct Z coordinate for the mobj. The visible Z coordinate
 * may be slightly different than the actual Z coordinate due to smoothed
 * plane movement.
 */
boolean RIT_VisMobjZ(sector_t *sector, void *data)
{
    vissprite_t        *vis = data;

    assert(sector != NULL);
    assert(data != NULL);

    if(vis->data.mo.floorAdjust &&
       projectedMobj->pos[VZ] == sector->SP_floorheight)
    {
        vis->center[VZ] = sector->SP_floorvisheight;
    }

    if(projectedMobj->pos[VZ] + projectedMobj->height ==
       sector->SP_ceilheight)
    {
        vis->center[VZ] = sector->SP_ceilvisheight - projectedMobj->height;
    }
    return true;
}

/**
 * Generates a vissprite for a mobj if it might be visible.
 */
void R_ProjectSprite(mobj_t *mo)
{
    sector_t           *sect = mo->subsector->sector;
    float               thangle = 0;
    float               pos[2];
    spritedef_t        *sprDef;
    spriteframe_t      *sprFrame = NULL;
    spritetex_t        *sprTex;
    int                 i;
    unsigned            rot;
    boolean             flip;
    vissprite_t        *vis;
    angle_t             ang;
    boolean             align;
    modeldef_t         *mf = NULL, *nextmf = NULL;
    float               interp = 0, distance;
    material_t         *mat;

    if(mo->ddFlags & DDMF_DONTDRAW || mo->translucency == 0xff ||
       mo->state == NULL || mo->state == states)
    {
        // Never make a vissprite when DDMF_DONTDRAW is set or when
        // the mo is fully transparent, or when the mo hasn't got
        // a valid state.
        return;
    }

    // Transform the origin point.
    pos[VX] = mo->pos[VX] - viewX;
    pos[VY] = mo->pos[VY] - viewY;

    // Decide which patch to use for sprite relative to player.

#ifdef RANGECHECK
    if((unsigned) mo->sprite >= (unsigned) numSprites)
    {
        Con_Error("R_ProjectSprite: invalid sprite number %i\n",
                  mo->sprite);
    }
#endif
    sprDef = &sprites[mo->sprite];
    if(mo->frame >= sprDef->numFrames)
    {
        // The frame is not defined, we can't display this object.
        return;
    }
    sprFrame = &sprDef->spriteFrames[mo->frame];

    // Determine distance to object.
    distance = Rend_PointDist2D(mo->pos);

    // Check for a 3D model.
    if(useModels)
    {
        interp = R_CheckModelFor(mo, &mf, &nextmf);
        if(mf && !(mf->flags & MFF_NO_DISTANCE_CHECK) && maxModelDistance &&
           distance > maxModelDistance)
        {
            // Don't use a 3D model.
            mf = nextmf = NULL;
            interp = -1;
        }
    }

    if(sprFrame->rotate && !mf)
    {   // Choose a different rotation based on player view.
        ang = R_PointToAngle(mo->pos[VX], mo->pos[VY]);
        rot = (ang - mo->angle + (unsigned) (ANG45 / 2) * 9) >> 29;
        mat = sprFrame->mats[rot];
        flip = (boolean) sprFrame->flip[rot];
    }
    else
    {   // Use single rotation for all views.
        mat = sprFrame->mats[0];
        flip = (boolean) sprFrame->flip[0];
    }

    sprTex = spriteTextures[mat->ofTypeID];

    // Align to the view plane?
    if(mo->ddFlags & DDMF_VIEWALIGN)
        align = true;
    else
        align = false;

    if(alwaysAlign == 1)
        align = true;

    // Perform visibility checking.
    if(!mf)
    {   // Its a sprite.
        if(!align && alwaysAlign != 2 && alwaysAlign != 3)
        {
            float               center[2], v1[2], v2[2];
            float               width, offset;

            width = (float) sprTex->info.width;
            offset = (float) sprTex->info.offsetX - (width / 2);

            // Project a line segment relative to the view in 2D, then check
            // if not entirely clipped away in the 360 degree angle clipper.
            center[VX] = mo->pos[VX];
            center[VY] = mo->pos[VY];
            M_ProjectViewRelativeLine2D(center, (align || alwaysAlign == 3),
                                        width, offset, v1, v2);

            // Check for visibility.
            if(!C_CheckViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]))
                return; // Isn't visible.
        }
    }
    else
    {   // Its a model.
        float               v[2], off[2];
        float               sinrv, cosrv;

        v[VX] = mo->pos[VX];
        v[VY] = mo->pos[VY];

        thangle =
            BANG2RAD(bamsAtan2(pos[VY] * 10, pos[VX] * 10)) - PI / 2;
        sinrv = sin(thangle);
        cosrv = cos(thangle);
        off[VX] = cosrv * mo->radius;
        off[VY] = sinrv * mo->radius;
        if(!C_CheckViewRelSeg
           (v[VX] - off[VX], v[VY] - off[VY], v[VX] + off[VX],
            v[VY] + off[VY]))
        {
            // The visibility check indicates that the model's origin is
            // not visible. However, if the model is close to the viewpoint
            // we will need to draw it. Otherwise large models are likely
            // to disappear too early.
            if(P_ApproxDistance
               (distance, mo->pos[VZ] + (mo->height / 2) - viewZ) >
               MAX_OBJECT_RADIUS)
            {
                return;         // Can't be visible.
            }
        }
        // Viewaligning means scaling down Z with models.
        align = false;
    }

    //
    // Store information in a vissprite.
    //
    vis = R_NewVisSprite();
    vis->type = VSPR_MAP_OBJECT;
    vis->distance = distance;
    vis->data.mo.subsector = mo->subsector;
    vis->light = LO_GetLuminous(mo->light);

    vis->data.mo.mf = mf;
    vis->data.mo.nextMF = nextmf;
    vis->data.mo.inter = interp;
    vis->data.mo.flags = mo->ddFlags;
    vis->data.mo.id = mo->thinker.id;
    vis->data.mo.selector = mo->selector;
    vis->center[VX] = mo->pos[VX];
    vis->center[VY] = mo->pos[VY];
    vis->center[VZ] = mo->pos[VZ];

    vis->data.mo.floorAdjust =
        (fabs(sect->SP_floorvisheight - sect->SP_floorheight) < 8);

    // The mo's Z coordinate must match the actual visible
    // floor/ceiling height.  When using smoothing, this requires
    // iterating through the sectors (planes) in the vicinity.
    validCount++;
    projectedMobj = mo;
    P_MobjSectorsIterator(mo, RIT_VisMobjZ, vis);

    vis->data.mo.gzt =
        vis->center[VZ] + ((float) sprTex->info.offsetY);

    vis->data.mo.viewAligned = align;
    vis->data.mo.stateFullBright =
        (mo->state->flags & STF_FULLBRIGHT)? true : false;

    vis->data.mo.secFloor = mo->subsector->sector->SP_floorvisheight;
    vis->data.mo.secCeil  = mo->subsector->sector->SP_ceilvisheight;

    if(mo->ddFlags & DDMF_TRANSLATION)
        vis->data.mo.pClass = (mo->ddFlags >> DDMF_CLASSTRSHIFT) & 0x3;
    else
        vis->data.mo.pClass = 0;

    // Foot clipping.
    vis->data.mo.floorClip = mo->floorClip;
    if(mo->ddFlags & DDMF_BOB)
    {
        // Bobbing is applied to the floorclip.
        vis->data.mo.floorClip += R_GetBobOffset(mo);
    }

    if(mf)
    {
        // Determine the rotation angles (in degrees).
        if(mf->sub[0].flags & MFF_ALIGN_YAW)
        {
            vis->data.mo.yaw = 90 - thangle / PI * 180;
        }
        else if(mf->sub[0].flags & MFF_SPIN)
        {
            vis->data.mo.yaw =
                modelSpinSpeed * 70 * ddLevelTime + MOBJ_TO_ID(mo) % 360;
        }
        else if(mf->sub[0].flags & MFF_MOVEMENT_YAW)
        {
            vis->data.mo.yaw = R_MovementYaw(mo->mom[MX], mo->mom[MY]);
        }
        else
        {
            if(useSRVOAngle && !netGame && !playback)
                vis->data.mo.yaw = (float) (mo->visAngle << 16);
            else
                vis->data.mo.yaw = (float) mo->angle;

            vis->data.mo.yaw /= (float) ANGLE_MAX * -360;
        }

        // How about a unique offset?
        if(mf->sub[0].flags & MFF_IDANGLE)
        {
            // Multiply with an arbitrary factor.
            vis->data.mo.yaw += MOBJ_TO_ID(mo) % 360;
        }

        if(mf->sub[0].flags & MFF_ALIGN_PITCH)
        {
            vis->data.mo.pitch =
                -BANG2DEG(bamsAtan2
                          (((vis->center[VZ] + vis->data.mo.gzt) / 2 -
                            viewZ) * 10, distance * 10));
        }
        else if(mf->sub[0].flags & MFF_MOVEMENT_PITCH)
        {
            vis->data.mo.pitch =
                R_MovementPitch(mo->mom[MX], mo->mom[MY], mo->mom[MZ]);
        }
        else
            vis->data.mo.pitch = 0;
    }
    vis->data.mo.matFlip[0] = flip;
    vis->data.mo.matFlip[1] = false;
    vis->data.mo.mat = mat;

    // The three highest bits of the selector are used for an alpha level.
    // 0 = opaque (alpha -1)
    // 1 = 1/8 transparent
    // 4 = 1/2 transparent
    // 7 = 7/8 transparent
    i = mo->selector >> DDMOBJ_SELECTOR_SHIFT;
    if(i & 0xe0)
    {
        vis->data.mo.alpha = 1 - ((i & 0xe0) >> 5) / 8.0f;
    }
    else
    {
        if(mo->translucency)
            vis->data.mo.alpha = 1 - mo->translucency * reciprocal255;
        else
            vis->data.mo.alpha = -1;
    }

    // Reset the visual offset.
    memset(vis->data.mo.visOff, 0, sizeof(vis->data.mo.visOff));

    // Short-range visual offsets.
    if((vis->data.mo.mf && useSRVO > 0) ||
       (!vis->data.mo.mf && useSRVO > 1))
    {
        if(mo->state && mo->tics >= 0)
        {
            float                   mul =
                (mo->tics - frameTimePos) / (float) mo->state->tics;

            for(i = 0; i < 3; i++)
                vis->data.mo.visOff[i] = mo->srvo[i] * mul;
        }

        if(mo->mom[MX] != 0 || mo->mom[MY] != 0 || mo->mom[MZ] != 0)
        {
            // Use the object's speed to calculate a short-range
            // offset.
            vis->data.mo.visOff[VX] += mo->mom[MX] * frameTimePos;
            vis->data.mo.visOff[VY] += mo->mom[MY] * frameTimePos;
            vis->data.mo.visOff[VZ] += mo->mom[MZ] * frameTimePos;
        }
    }
}

void R_AddSprites(sector_t *sec)
{
    float               visibleTop;
    mobj_t             *mo;
    spriteinfo_t        spriteInfo;
    boolean             raised = false;

    // Don't use validCount, because other parts of the renderer may
    // change it.
    if(sec->addSpriteCount == frameCount)
        return;                 // already added

    sec->addSpriteCount = frameCount;

    for(mo = sec->mobjList; mo; mo = mo->sNext)
    {
        R_ProjectSprite(mo);

        // Hack: Sprites have a tendency to extend into the ceiling in
        // sky sectors. Here we will raise the skyfix dynamically, at
        // runtime, to make sure that no sprites get clipped by the sky.
        // Only check
        if(R_IsSkySurface(&sec->SP_ceilsurface))
        {
            if(!(mo->dPlayer && mo->dPlayer->flags & DDPF_CAMERA) && // Cameramen don't exist!
               mo->pos[VZ] <= sec->SP_ceilheight &&
               mo->pos[VZ] >= sec->SP_floorheight &&
               !(sec->flags & SECF_SELFREFHACK))
            {
                R_GetSpriteInfo(mo->sprite, mo->frame, &spriteInfo);
                visibleTop = mo->pos[VZ] + spriteInfo.height;

                if(visibleTop >
                    sec->SP_ceilheight + sec->skyFix[PLN_CEILING].offset)
                {
                    // Raise sector skyfix.
                    sec->skyFix[PLN_CEILING].offset =
                        visibleTop - sec->SP_ceilheight + 16; // Add some leeway.
                    raised = true;
                }
            }
        }
    }

    // This'll adjust all adjacent sky ceiling fixes.
    if(raised)
        R_SkyFix(false, true);
}

void R_SortVisSprites(void)
{
    int                 i, count;
    vissprite_t        *ds, *best = 0;
    vissprite_t         unsorted;
    float               bestdist;

    count = visSpriteP - visSprites;

    unsorted.next = unsorted.prev = &unsorted;
    if(!count)
        return;

    for(ds = visSprites; ds < visSpriteP; ds++)
    {
        ds->next = ds + 1;
        ds->prev = ds - 1;
    }
    visSprites[0].prev = &unsorted;
    unsorted.next = &visSprites[0];
    (visSpriteP - 1)->next = &unsorted;
    unsorted.prev = visSpriteP - 1;

    //
    // Pull the vissprites out by distance.
    //

    visSprSortedHead.next = visSprSortedHead.prev = &visSprSortedHead;

    /**
     * \todo
     * Oprofile results from nuts.wad show over 25% of total execution time
     * was spent sorting vissprites (nuts.wad map01 is a perfect
     * pathological test case).
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

static void scaleFloatRGB(float *out, const float *in, float mul)
{
    memset(out, 0, sizeof(float) * 3);
    R_ScaleAmbientRGB(out, in, mul);
}

/**
 * Setup the light/dark factors and dot product offset for glow lights.
 */
static void glowLightSetup(vlight_t *light)
{
    light->lightSide = 1;
    light->darkSide = 0;
    light->offset = .3f;
}

/**
 * Iterator for processing light sources around a vissprite.
 */
boolean visSpriteLightIterator(lumobj_t *lum, float xyDist, void *data)
{
    float               dist;
    float               glowHeight;
    boolean             addLight = false;
    vlightiterparams_t *params = (vlightiterparams_t*) data;

    // Is the light close enough to make the list?
    switch(lum->type)
    {
    case LT_OMNI:
        {
        float       zDist;

        zDist = params->pos[VZ] - lum->pos[VZ] + LUM_OMNI(lum)->zOff;
        dist = P_ApproxDistance(xyDist, zDist);

        if(dist < (float) loMaxRadius)
            addLight = true;
        }
        break;

    case LT_PLANE:
        if(LUM_PLANE(lum)->intensity &&
           (lum->color[0] > 0 || lum->color[1] > 0|| lum->color[2] > 0))
        {
            // Floor glow
            glowHeight =
                (MAX_GLOWHEIGHT * LUM_PLANE(lum)->intensity) * glowHeightFactor;

            // Don't make too small or too large glows.
            if(glowHeight > 2)
            {
                float       delta[3];

                if(glowHeight > glowHeightMax)
                    glowHeight = glowHeightMax;

                delta[VX] = params->pos[VX] - lum->pos[VX];
                delta[VY] = params->pos[VY] - lum->pos[VY];
                delta[VZ] = params->pos[VZ] - lum->pos[VZ];

                if(!((dist = M_DotProduct(delta, LUM_PLANE(lum)->normal)) < 0))
                    // Is on the front of the glow plane.
                    addLight = true;
            }
        }
        break;

    default:
        Con_Error("visSpriteLightIterator: Invalid value, lum->type = %i.",
                  (int) lum->type);
        break;
    }

    // If the light is not close enough, skip it.
    if(addLight)
    {
        vlight_t           *light;
        uint                i, maxIndex = 0;
        float               maxDist = -1;

        // See if this light can be inserted into the list.
        // (In most cases it should be.)
        for(i = 1, light = lights + 1; i < params->maxLights; ++i, light++)
        {
            if(light->approxDist > maxDist)
            {
                maxDist = light->approxDist;
                maxIndex = i;
            }
        }

        // Now we know the farthest light on the current list (at maxIndex).
        if(dist < maxDist)
        {
            // The new light is closer. Replace the old max.
            light = &lights[maxIndex];

            light->used = true;
            switch(lum->type)
            {
            case LT_OMNI:
                light->lum = lum;
                light->approxDist = dist;
                break;

            case LT_PLANE:
                light->lum = NULL;
                light->approxDist = dist;
                glowLightSetup(light);

                light->worldVector[VX] = LUM_PLANE(lum)->normal[VX];
                light->worldVector[VY] = LUM_PLANE(lum)->normal[VY];
                light->worldVector[VZ] = -LUM_PLANE(lum)->normal[VZ];

                dist = 1 - dist / glowHeight;
                scaleFloatRGB(light->color, lum->color, dist);
                R_ScaleAmbientRGB(params->ambientColor, lum->color, dist / 3);
                break;

            default:
                Con_Error("visSpriteLightIterator: Invalid value, "
                          "lum->type = %i.", (int) lum->type);
                break;
            }

            if(*params->numLights < maxIndex + 1)
                *params->numLights = maxIndex + 1;
        }
    }

    return true;
}

void R_CollectAffectingLights(const collectaffectinglights_params_t *params,
                              vlight_t **ptr, uint *num)
{
    uint                i, numLights;
    vlight_t           *light;

    if(!params || !ptr || !num)
        return;

    // Determine the lighting properties that affect this vissprite and find
    // any lights close enough to contribute additional light.
    numLights = 0;
    if(params->maxLights)
    {
        memset(lights, 0, sizeof(lights));

        // Should always be lit with world light.
        numLights++;
        lights[0].used = true;

        // Set the correct intensity.
        for(i = 0; i < 3; ++i)
        {
            lights->worldVector[i] = worldLight[i];
            lights->color[i] = params->ambientColor[i];
        }

        if(params->starkLight)
        {
            lights->lightSide = .35f;
            lights->darkSide = .5f;
            lights->offset = 0;
        }
        else
        {
            // World light can both light and shade. Normal objects
            // get more shade than light (to prevent them from
            // becoming too bright when compared to ambient light).
            lights->lightSide = .2f;
            lights->darkSide = .8f;
            lights->offset = .3f;
        }
    }

    // Add extra light using dynamic lights.
    if(params->maxLights > numLights && loInited && params->subsector)
    {
        vlightiterparams_t vars;

        memcpy(vars.pos, params->center, sizeof(vars.pos));
        vars.numLights = &numLights;
        vars.maxLights = params->maxLights;
        vars.ambientColor = params->ambientColor;

        // Find the nearest sources of light. They will be used to
        // light the vertices. First initialize the array.
        for(i = numLights; i < MAX_VISSPRITE_LIGHTS; ++i)
            lights[i].approxDist = DDMAXFLOAT;

        LO_LumobjsRadiusIterator(params->subsector, params->center[VX],
                                 params->center[VY], (float) loMaxRadius,
                                 &vars, visSpriteLightIterator);
    }

    // Don't use too many lights.
    if(numLights > params->maxLights)
        numLights = params->maxLights;

    // Calculate color for light sources nearby.
    for(i = 0, light = lights; i < numLights; ++i, light++)
    {
        if(!light->used)
            continue;

        if(light->lum)
        {
            int             c;
            float           intensity, lightCenter[3];
            lumobj_t       *l = light->lum;

            // The intensity of the light.
            intensity = (1 - light->approxDist /
                            LUM_OMNI(l)->radius) * 2;
            if(intensity < 0)
                intensity = 0;
            if(intensity > 1)
                intensity = 1;

            if(intensity == 0)
            {   // No point in lighting with this!
                light->used = false;
                continue;
            }

            // The center of the light source.
            lightCenter[VX] = l->pos[VX];
            lightCenter[VY] = l->pos[VY];
            lightCenter[VZ] = l->pos[VZ] + LUM_OMNI(l)->zOff;

            // Calculate the normalized direction vector,
            // pointing out of the vissprite.
            for(c = 0; c < 3; ++c)
            {
                light->vector[c] = (params->center[c] - lightCenter[c]) /
                                        light->approxDist;
                // ...and the color of the light.
                light->color[c] = l->color[c] * intensity;
            }
        }
        else
        {
            memcpy(light->vector, light->worldVector,
                   sizeof(light->vector));
        }
    }

    *ptr = lights;
    *num = numLights;
}

/**
 * @return              The current floatbob offset for the mobj, if the mobj
 *                      is flagged for bobbing, else @c 0.
 */
float R_GetBobOffset(mobj_t *mo)
{
    if(mo->ddFlags & DDMF_BOB)
    {
        return (sin(MOBJ_TO_ID(mo) + ddLevelTime / 1.8286 * 2 * PI) * 8);
    }
    return 0;
}
