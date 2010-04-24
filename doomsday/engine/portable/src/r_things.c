/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#include "m_stack.h"

// MACROS ------------------------------------------------------------------

#define MAX_FRAMES              (128)
#define MAX_OBJECT_RADIUS       (128)

// TYPES -------------------------------------------------------------------

typedef struct vlightnode_s {
    struct vlightnode_s* next, *nextUsed;
    vlight_t        vlight;
} vlightnode_t;

typedef struct vlightlist_s {
    vlightnode_t*   head;
    boolean         sortByDist;
} vlightlist_t;

typedef struct {
    vec3_t          pos;
    boolean         haveList;
    uint            listIdx;
} vlightiterparams_t;

typedef struct spriterecord_frame_s {
    lumpnum_t       lump;
    byte            frame[2];
    byte            rotation[2];

    material_t*     mat;
    struct spriterecord_frame_s* next;
} spriterecord_frame_t;

typedef struct spriterecord_s {
    char            name[5];
    int             numFrames;
    spriterecord_frame_t* frames;
    struct spriterecord_s* next;
} spriterecord_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float weaponOffsetScale = 0.3183f; // 1/Pi
int weaponOffsetScaleY = 1000;
float weaponFOVShift = 45;
byte weaponNoStretch = false;
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
int numSprites;

vissprite_t visSprites[MAXVISSPRITES], *visSpriteP;
vispsprite_t visPSprites[DDMAXPSPRITES];

int maxModelDistance = 1500;
int levelFullBright = false;

vissprite_t visSprSortedHead;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// vlight nodes.
static vlightnode_t* vLightFirst, *vLightCursor;

// vlight link lists.
static uint numVLightLinkLists = 0, vLightLinkListCursor = 0;
static vlightlist_t* vLightLinkLists;

static vissprite_t overflowVisSprite;

static const float worldLight[3] = {-.400891f, -.200445f, .601336f};

// Tempory storage, used when reading sprite definitions.
static int numSpriteRecords;
static spriterecord_t* spriteRecords;
static zblockset_t* spriteRecordBlockSet, *spriteRecordFrameBlockSet;

// CODE --------------------------------------------------------------------

/**
 * Initialize the vlight system in preparation for rendering view(s) of the
 * game world. Called by R_InitLevel().
 */
void VL_InitForMap(void)
{
    vLightLinkLists = NULL;
    numVLightLinkLists = 0, vLightLinkListCursor = 0;
}

/**
 * Moves all used vlight nodes to the list of unused nodes, so they can be
 * reused.
 */
void VL_InitForNewFrame(void)
{
    // Start reusing nodes from the first one in the list.
    vLightCursor = vLightFirst;

    // Clear the mobj vlight link lists.
    vLightLinkListCursor = 0;
    if(numVLightLinkLists)
        memset(vLightLinkLists, 0, numVLightLinkLists * sizeof(vlightlist_t));
}

/**
 * Create a new vlight list.
 *
 * @param sortByDist    Lights in this list will be automatically sorted by
 *                      their approximate distance from the point being lit.
 * @return              Identifier for the new list.
 */
static uint newVLightList(boolean sortByDist)
{
    vlightlist_t*       list;

    // Ran out of vlight link lists?
    if(++vLightLinkListCursor >= numVLightLinkLists)
    {
        uint                newNum = numVLightLinkLists * 2;

        if(!newNum)
            newNum = 2;

        vLightLinkLists =
            Z_Realloc(vLightLinkLists, newNum * sizeof(vlightlist_t),
                      PU_MAP);
        numVLightLinkLists = newNum;
    }

    list = &vLightLinkLists[vLightLinkListCursor-1];
    list->head = NULL;
    list->sortByDist = sortByDist;

    return vLightLinkListCursor - 1;
}

static vlightnode_t* newVLightNode(void)
{
    vlightnode_t*       node;

    // Have we run out of nodes?
    if(vLightCursor == NULL)
    {
        node = Z_Malloc(sizeof(vlightnode_t), PU_STATIC, NULL);

        // Link the new node to the list.
        node->nextUsed = vLightFirst;
        vLightFirst = node;
    }
    else
    {
        node = vLightCursor;
        vLightCursor = vLightCursor->nextUsed;
    }

    node->next = NULL;
    return node;
}

/**
 * @return              Ptr to a new vlight node. If the list of unused
 *                      nodes is empty, a new node is created.
 */
static vlightnode_t* newVLight(void)
{
    vlightnode_t*       node = newVLightNode();
    //vlight_t*           vlight = &node->vlight;

    //// \todo move vlight setup here.
    return node;
}

static void linkVLightNodeToList(vlightnode_t* node, uint listIndex)
{
    vlightlist_t*       list = &vLightLinkLists[listIndex];

    if(list->sortByDist && list->head)
    {
        vlightnode_t*       last, *iter;
        vlight_t*           vlight;

        last = iter = list->head;
        do
        {
            vlight = &node->vlight;

            // Is this closer than the one being added?
            if(node->vlight.approxDist > vlight->approxDist)
            {
                last = iter;
                iter = iter->next;
            }
            else
            {   // Need to insert it here.
                node->next = last->next;
                last->next = node;
                return;
            }
        } while(iter);
    }

    node->next = list->head;
    list->head = node;
}

/**
 * Local function for R_InitSprites.
 */
static void installSpriteLump(spriteframe_t* sprTemp, int* maxFrame,
                              material_t* mat, uint frame, uint rotation,
                              boolean flipped)
{
    if(frame >= 30 || rotation > 8)
    {
        return;
    }

    if((int) frame > *maxFrame)
        *maxFrame = frame;

    if(rotation == 0)
    {
        int                 r;

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
 */
void R_PreInitSprites(void)
{
    float               startTime = Sys_GetSeconds();

    int                 i, numSpritePatches = 0;
    ddstack_t*          stack = Stack_New();

    // Free all memory acquired for spritetex_t structures.
    Z_FreeTags(PU_SPRITE, PU_SPRITE);

    /**
     * Step 1: Build database of lumps which may describe sprites.
     */
    numSpriteRecords = 0;
    spriteRecords = NULL;
    spriteRecordBlockSet = Z_BlockCreate(sizeof(spriterecord_t), 64,
                                         PU_STATIC),
    spriteRecordFrameBlockSet = Z_BlockCreate(sizeof(spriterecord_frame_t),
                                              256, PU_STATIC);
    for(i = 0; i < numLumps; ++i)
    {
        const char*         name = W_LumpName(i);
        spriterecord_t*     rec;

        if(name[0] == 'S')
        {
            if(!strnicmp(name + 1, "_START", 6) ||
               !strnicmp(name + 2, "_START", 6))
            {
                // We've arrived at *a* sprite block.
                Stack_Push(stack, NULL);
                continue;
            }
            else if(!strnicmp(name + 1, "_END", 4) ||
                    !strnicmp(name + 2, "_END", 4))
            {
                // The sprite block ends.
                Stack_Pop(stack);
                continue;
            }
        }

        if(!Stack_Height(stack))
            continue;

        /**
         * This lump is potentially a sprite.
         */

        // Check that the name is valid.
        if(!name[4] || !name[5] || (name[6] && !name[7]))
            continue; // This is not a sprite frame.

        // Indices 5 and 7 must be numbers (0-8).
        if(name[5] < '0' || name[5] > '8')
            continue;

        if(name[7] && (name[7] < '0' || name[7] > '8'))
            continue;

        if(W_LumpLength(i) < 8)
            continue;

        numSpritePatches++;

        // Its a valid, name. Have we already come accross it?
        rec = spriteRecords;
        while(rec)
        {
            if(!strnicmp(rec->name, name, 4))
                break;
            rec = rec->next;
        }

        if(!rec)
        {   // An entirely new sprite.
            rec = Z_BlockNewElement(spriteRecordBlockSet);
            strncpy(rec->name, name, 4);
            rec->name[4] = '\0';
            rec->numFrames = 0;
            rec->frames = NULL;

            rec->next = spriteRecords;
            spriteRecords = rec;
            numSpriteRecords++;
        }

        // Add the frame(s).
        {
        spriterecord_frame_t* sprFrame = rec->frames;
        boolean             link = false;

        while(sprFrame)
        {
            if(sprFrame->frame[0] == name[4] - 'A' + 1 &&
               sprFrame->rotation[0] == name[5] - '0')
                break;
            sprFrame = sprFrame->next;
        }

        if(!sprFrame)
        {   // A new frame.
            sprFrame = Z_BlockNewElement(spriteRecordFrameBlockSet);
            link = true;
        }

        sprFrame->lump = i;
        sprFrame->frame[0] = name[4] - 'A' + 1;
        sprFrame->rotation[0] = name[5] - '0';
        if(name[6])
        {
            sprFrame->frame[1] = name[6] - 'A' + 1;
            sprFrame->rotation[1] = name[7] - '0';
        }
        else
        {
            sprFrame->frame[1] = 0;
        }

        if(link)
        {
            sprFrame->next = rec->frames;
            rec->frames = sprFrame;
        }
        }
    }

    while(Stack_Height(stack))
        Stack_Pop(stack);
    Stack_Delete(stack);

    /**
     * Step 2: Create gltextures and materials for ALL sprite patches.
     */
    numSpriteTextures = numSpritePatches;
    spriteTextures = NULL;

    if(numSpritePatches)
    {
        int                 idx = 0;
        spritetex_t*        storage;
        spriterecord_t*     rec = spriteRecords;

        spriteTextures = Z_Malloc(sizeof(spritetex_t*) * numSpriteTextures, PU_SPRITE, 0);
        storage = Z_Malloc(sizeof(spritetex_t) * numSpriteTextures, PU_SPRITE, 0);

        do
        {
            spriterecord_frame_t* frame = rec->frames;

            do
            {
                const char*         name;
                const lumppatch_t*  patch;
                spritetex_t*        sprTex;
                const gltexture_t*  glTex;

                spriteTextures[idx] = sprTex = &storage[idx];

                patch = (const lumppatch_t *)
                    W_CacheLumpNum(frame->lump, PU_CACHE);
                name = W_LumpName(frame->lump);

                sprTex->lump = frame->lump;
                sprTex->offX = SHORT(patch->leftOffset);
                sprTex->offY = SHORT(patch->topOffset);
                sprTex->width = SHORT(patch->width);
                sprTex->height = SHORT(patch->height);

                glTex = GL_CreateGLTexture(name, idx++, GLT_SPRITE);

                // Create a new material for this sprite patch.
                frame->mat = P_MaterialCreate(name, sprTex->width,
                    sprTex->height, 0, glTex->id, MN_SPRITES, NULL);
            } while((frame = frame->next));
        } while((rec = rec->next));
    }

    VERBOSE(Con_Message("R_InitSpriteRecords: Done in %.2f seconds.\n",
                        Sys_GetSeconds() - startTime));
}

static void initSpriteDefs(spriterecord_t* const * sprRecords, int num)
{
    numSprites = num;
    if(sprites)
        Z_Free(sprites);
    sprites = NULL;

    if(numSprites)
    {
        int                 n, idx = 0;
        spriteframe_t       sprTemp[MAX_FRAMES];
        int                 maxFrame;

        sprites = Z_Malloc(numSprites * sizeof(*sprites), PU_STATIC, NULL);

        for(n = 0; n < num; ++n)
        {
            int                 frame;
            const spriterecord_t* rec;
            spritedef_t*        sprDef = &sprites[n];

            if(!sprRecords[n])
            {   // A record for a sprite we were unable to locate.
                sprDef->numFrames = 0;
                sprDef->spriteFrames = NULL;
                continue;
            }

            rec = sprRecords[n];

            memset(sprTemp, -1, sizeof(sprTemp));
            maxFrame = -1;

            {
            const spriterecord_frame_t* frame = rec->frames;

            do
            {
                installSpriteLump(sprTemp, &maxFrame, frame->mat,
                                  frame->frame[0] - 1, frame->rotation[0],
                                  false);
                if(frame->frame[1])
                    installSpriteLump(sprTemp, &maxFrame, frame->mat,
                                      frame->frame[1] - 1, frame->rotation[1],
                                      true);
            } while((frame = frame->next));
            }

            /**
             * Check the frames that were found for completeness.
             */
            if(maxFrame == -1)
            {   // Should NEVER happen.
                sprDef->numFrames = 0;
            }

            maxFrame++;
            for(frame = 0; frame < maxFrame; ++frame)
            {
                switch((int) sprTemp[frame].rotate)
                {
                case -1: // No rotations were found for that frame at all.
                    Con_Error("R_InitSprites: No patches found for %s frame %c.",
                              rec->name, frame + 'A');
                    break;

                case 0: // Only the first rotation is needed.
                    break;

                case 1: // Must have all 8 frames.
                    {
                    int                 rotation;
                    for(rotation = 0; rotation < 8; ++rotation)
                    {
                        if(!sprTemp[frame].mats[rotation])
                            Con_Error("R_InitSprites: Sprite %s frame %c is "
                                      "missing rotations.", rec->name,
                                      frame + 'A');
                    }
                    }
                    break;

                default:
                    Con_Error("R_InitSpriteDefs: Invalid value, "
                              "sprTemp[frame].rotate = %i.",
                              (int) sprTemp[frame].rotate);
                    break;
                }
            }

            // Allocate space for the frames present and copy sprTemp to it.
            strncpy(sprDef->name, rec->name, 4);
            sprDef->name[4] = '\0';
            sprDef->numFrames = maxFrame;
            sprDef->spriteFrames =
                Z_Malloc(maxFrame * sizeof(spriteframe_t), PU_SPRITE, NULL);
            memcpy(sprDef->spriteFrames, sprTemp,
                   maxFrame * sizeof(spriteframe_t));
        }
    }
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
void R_InitSprites(void)
{
    float               startTime = Sys_GetSeconds();

    int                 n, max;
    spriterecord_t*     rec, **list;

    /**
     * \kludge
     * As the games still rely upon the sprite definition indices to match
     * those of the sprite name table, use the later to re-index the sprite
     * records database before using them to derive the sprite definitions.
     * New sprites added in mods that we do not have sprite name defs for
     * are pushed to the end of the list (this is fine as the game will not
     * attempt to reference them by either name or indice as they are not
     * present in their game-side sprite index tables. Similarly, DeHackED
     * does not allow for new sprite frames to be added anyway).
     *
     * \todo
     * This unobvious requirement should be broken somehow and perhaps even
     * get rid of the sprite name definitions entirely.
     */
    max = MAX_OF(numSpriteRecords, countSprNames.num);
    n = max-1;
    list = M_Calloc(sizeof(spriterecord_t*) * max);
    rec = spriteRecords;
    do
    {
        int                 idx = Def_GetSpriteNum(rec->name);
        list[idx == -1? n-- : idx] = rec;
    } while((rec = rec->next));
    /// \kludge end

    // Create sprite definitions from the located sprite patch lumps.
    initSpriteDefs(list, max);

    M_Free(list);

    // We are now done with the sprite records.
    Z_BlockDestroy(spriteRecordBlockSet);
    spriteRecordBlockSet = NULL;
    Z_BlockDestroy(spriteRecordFrameBlockSet);
    spriteRecordFrameBlockSet = NULL;
    numSpriteRecords = 0;

    VERBOSE(Con_Message("R_InitSprites: Done in %.2f seconds.\n",
                        Sys_GetSeconds() - startTime));
}

material_t* R_GetMaterialForSprite(int sprite, int frame)
{
    spritedef_t*        sprDef;

    if((unsigned) sprite >= (unsigned) numSprites)
        return NULL;
    sprDef = &sprites[sprite];

    if(frame >= sprDef->numFrames)
        return NULL;

    return sprDef->spriteFrames[frame].mats[0];
}

boolean R_GetSpriteInfo(int sprite, int frame, spriteinfo_t* info)
{
    spritedef_t*        sprDef;
    spriteframe_t*      sprFrame;
    spritetex_t*        sprTex;
    material_t*         mat;
    material_snapshot_t ms;

    if((unsigned) sprite >= (unsigned) numSprites)
    {
        Con_Message("R_GetSpriteInfo: Warning, invalid sprite number %i.\n", sprite);
        return false;
    }

    sprDef = &sprites[sprite];

    if(frame >= sprDef->numFrames)
    {
        // We have no information to return.
        memset(info, 0, sizeof(*info));
        return false;
    }

    sprFrame = &sprDef->spriteFrames[frame];
    mat = sprFrame->mats[0];
    Material_Prepare(&ms, mat, false, NULL);

    sprTex = spriteTextures[ms.units[MTU_PRIMARY].texInst->tex->ofTypeID];

    info->numFrames = sprDef->numFrames;
    info->material = mat;
    info->realLump = sprTex->lump;
    info->flip = sprFrame->flip[0];
    info->offset = sprTex->offX;
    info->topOffset = sprTex->offY;
    info->width = ms.width;
    info->height = ms.height;
    return true;
}

boolean R_GetPatchInfo(lumpnum_t lump, patchinfo_t* info)
{
    if(info)
        return false;

    memset(info, 0, sizeof(*info));

    if(lump >= 0 && lump < numLumps)
    {
        lumppatch_t*        patch =
            (lumppatch_t*) W_CacheLumpNum(lump, PU_CACHE);

        info->lump = info->realLump = lump;
        info->width = SHORT(patch->width);
        info->height = SHORT(patch->height);
        info->topOffset = SHORT(patch->topOffset);
        info->offset = SHORT(patch->leftOffset);
        return true;
    }

    VERBOSE(Con_Message("R_GetPatchInfo: Warning, invalid lumpnum %i.\n", lump));
    return false;
}

/**
 * @return              Radius of the mobj as it would visually appear to be.
 */
float R_VisualRadius(mobj_t* mo)
{
    modeldef_t*         mf, *nextmf;
    material_snapshot_t ms;

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
    Material_Prepare(&ms, R_GetMaterialForSprite(mo->sprite, mo->frame),
                     true, NULL);
    return ms.width / 2;
}

/**
 * Called at frame start.
 */
void R_ClearSprites(void)
{
    visSpriteP = visSprites;
}

vissprite_t* R_NewVisSprite(void)
{
    vissprite_t*        spr;

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

/**
 * If 3D models are found for psprites, here we will create vissprites for
 * them.
 */
void R_ProjectPlayerSprites(void)
{
    int i;
    float inter;
    modeldef_t* mf, *nextmf;
    ddpsprite_t* psp;
    boolean isFullBright = (levelFullBright != 0);
    boolean isModel;
    ddplayer_t* ddpl = &viewPlayer->shared;
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

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
        vispsprite_t* spr = &visPSprites[i];

        spr->type = VPSPR_SPRITE;
        spr->psp = psp;

        if(!psp->statePtr)
            continue;

        // First, determine whether this is a model or a sprite.
        isModel = false;
        if(useModels)
        {   // Is there a model for this frame?
            mobj_t dummy;

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
            spr->data.model.gzt = viewData->current.pos[VZ];
            spr->data.model.secFloor = ddpl->mo->subsector->sector->SP_floorvisheight;
            spr->data.model.secCeil = ddpl->mo->subsector->sector->SP_ceilvisheight;
            spr->data.model.pClass = 0;
            spr->data.model.floorClip = 0;

            spr->data.model.mf = mf;
            spr->data.model.nextMF = nextmf;
            spr->data.model.inter = inter;
            spr->data.model.viewAligned = true;
            spr->center[VX] = viewData->current.pos[VX];
            spr->center[VY] = viewData->current.pos[VY];
            spr->center[VZ] = viewData->current.pos[VZ];

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
        {   // No, draw a 2D sprite (in Rend_DrawPlayerSprites).
            spr->type = VPSPR_SPRITE;

            // Adjust the center slightly so an angle can be calculated.
            spr->center[VX] = viewData->current.pos[VX];
            spr->center[VY] = viewData->current.pos[VY];
            spr->center[VZ] = viewData->current.pos[VZ];

            spr->data.sprite.subsector = ddpl->mo->subsector;
            spr->data.sprite.alpha = psp->alpha;
            spr->data.sprite.isFullBright = (psp->flags & DDPSPF_FULLBRIGHT)!=0;
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

typedef struct {
    vissprite_t*        vis;
    const mobj_t*       mo;
    boolean             floorAdjust;
} vismobjzparams_t;

/**
 * Determine the correct Z coordinate for the mobj. The visible Z coordinate
 * may be slightly different than the actual Z coordinate due to smoothed
 * plane movement.
 */
boolean RIT_VisMobjZ(sector_t* sector, void* data)
{
    vismobjzparams_t*   params;

    assert(sector != NULL);
    assert(data != NULL);
    params = (vismobjzparams_t*) data;

    if(params->floorAdjust && params->mo->pos[VZ] == sector->SP_floorheight)
    {
        params->vis->center[VZ] = sector->SP_floorvisheight;
    }

    if(params->mo->pos[VZ] + params->mo->height == sector->SP_ceilheight)
    {
        params->vis->center[VZ] = sector->SP_ceilvisheight - params->mo->height;
    }

    return true;
}

static void setupSpriteParamsForVisSprite(rendspriteparams_t *params,
                                          float x, float y, float z, float distance, float visOffX, float visOffY, float visOffZ,
                                          float secFloor, float secCeil,
                                          float floorClip, float top,
                                          material_t* mat, boolean matFlipS, boolean matFlipT, blendmode_t blendMode,
                                          float ambientColorR, float ambientColorG, float ambientColorB, float alpha,
                                          uint vLightListIdx,
                                          int transMap, int transClass, subsector_t* ssec,
                                          boolean floorAdjust, boolean fitTop, boolean fitBottom,
                                          boolean viewAligned,
                                          boolean brightShadow, boolean shadow, boolean altShadow,
                                          boolean fullBright)
{
    spritetex_t*        sprTex = NULL;
    material_snapshot_t ms;
    material_load_params_t mparams;

    if(!params)
        return; // Wha?

    memset(&mparams, 0, sizeof(mparams));
    mparams.tmap = transMap;
    mparams.tclass = transClass;

    Material_Prepare(&ms, mat, true, &mparams);

    sprTex = spriteTextures[ms.units[MTU_PRIMARY].texInst->tex->ofTypeID];

    params->width = ms.width;
    params->height = ms.height;

    params->center[VX] = x;
    params->center[VY] = y;
    params->center[VZ] = z;
    params->srvo[VX] = visOffX;
    params->srvo[VY] = visOffY;
    params->srvo[VZ] = visOffZ;
    params->distance = distance;
    params->viewOffX = (float) sprTex->offX - ms.width / 2.0f;
    params->subsector = ssec;
    params->viewAligned = viewAligned;
    params->noZWrite = noSpriteZWrite;

    params->mat = mat;
    params->tMap = transMap;
    params->tClass = transClass;
    params->matOffset[0] = ms.units[MTU_PRIMARY].texInst->data.sprite.texCoord[0];
    params->matOffset[1] = ms.units[MTU_PRIMARY].texInst->data.sprite.texCoord[1];
    params->matFlip[0] = matFlipS;
    params->matFlip[1] = matFlipT;
    params->blendMode = blendMode;

    params->ambientColor[CR] = ambientColorR;
    params->ambientColor[CG] = ambientColorG;
    params->ambientColor[CB] = ambientColorB;
    params->ambientColor[CA] = alpha;

    params->vLightListIdx = vLightListIdx;
}

void setupModelParamsForVisSprite(rendmodelparams_t *params,
                                  float x, float y, float z, float distance,
                                  float visOffX, float visOffY, float visOffZ, float gzt, float yaw, float yawAngleOffset, float pitch, float pitchAngleOffset,
                                  struct modeldef_s* mf, struct modeldef_s* nextMF, float inter,
                                  float ambientColorR, float ambientColorG, float ambientColorB, float alpha,
                                  uint vLightListIdx,
                                  int id, int selector, subsector_t* ssec, int mobjDDFlags, int tmap,
                                  boolean viewAlign, boolean fullBright,
                                  boolean alwaysInterpolate)
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
    params->center[VX] = x;
    params->center[VY] = y;
    params->center[VZ] = z;
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

void getLightingParams(float x, float y, float z, subsector_t* ssec,
                       float distance, boolean fullBright,
                       float ambientColor[3], uint* vLightListIdx)
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
            vec3_t              point;

            // Evaluate the position in the light grid.
            V3_Set(point, x, y, z);
            LG_Evaluate(point, ambientColor);
        }
        else
        {
            float               lightLevel = ssec->sector->lightLevel;
            const float*        secColor =
                R_GetSectorLightColor(ssec->sector);

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
        lparams.center[VX] = x;
        lparams.center[VY] = y;
        lparams.center[VZ] = z;
        lparams.subsector = ssec;
        lparams.ambientColor = ambientColor;

        *vLightListIdx = R_CollectAffectingLights(&lparams);
    }
}

/**
 * Generates a vissprite for a mobj if it might be visible.
 */
void R_ProjectSprite(mobj_t* mo)
{
    sector_t* sect = mo->subsector->sector;
    float thangle = 0, alpha, floorClip, secFloor, secCeil;
    float pos[2], yaw, pitch;
    vec3_t visOff;
    spritedef_t* sprDef;
    spriteframe_t* sprFrame = NULL;
    int i, tmap = 0, tclass = 0;
    unsigned rot;
    boolean matFlipS, matFlipT;
    vissprite_t* vis;
    angle_t ang;
    boolean align, fullBright, viewAlign, floorAdjust;
    modeldef_t* mf = NULL, *nextmf = NULL;
    float interp = 0, distance, gzt;
    spritetex_t* sprTex;
    vismobjzparams_t params;
    visspritetype_t visType = VSPR_SPRITE;
    float ambientColor[3];
    uint vLightListIdx = 0;
    material_t* mat;
    material_snapshot_t ms;
    material_load_params_t mparams;
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

    if(mo->ddFlags & DDMF_DONTDRAW || mo->translucency == 0xff ||
       mo->state == NULL || mo->state == states)
    {
        // Never make a vissprite when DDMF_DONTDRAW is set or when
        // the mo is fully transparent, or when the mo hasn't got
        // a valid state.
        return;
    }
    if(sect->SP_floorvisheight >= sect->SP_ceilvisheight)
    {   // Never make a vissprite when the mobj's origin sector is of zero height.
        return;
    }

    // Transform the origin point.
    pos[VX] = mo->pos[VX] - viewData->current.pos[VX];
    pos[VY] = mo->pos[VY] - viewData->current.pos[VY];

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
        matFlipS = (boolean) sprFrame->flip[rot];
    }
    else
    {   // Use single rotation for all views.
        mat = sprFrame->mats[0];
        matFlipS = (boolean) sprFrame->flip[0];
    }
    matFlipT = false;

    tmap = mo->tmap;
    tclass = mo->tclass;

    memset(&mparams, 0, sizeof(mparams));
    mparams.tmap = tmap;
    mparams.tclass = tclass;

    Material_Prepare(&ms, mat, true, &mparams);

    sprTex = spriteTextures[ms.units[MTU_PRIMARY].texInst->tex->ofTypeID];

    // Align to the view plane?
    if(mo->ddFlags & DDMF_VIEWALIGN)
        align = true;
    else
        align = false;

    if(alwaysAlign == 1)
        align = true;

    // Perform visibility checking.
    {
    float               center[2], v1[2], v2[2];
    float               width = R_VisualRadius(mo)*2, offset = 0;

    if(!mf)
        offset = (float) sprTex->offX - (width / 2);

    // Project a line segment relative to the view in 2D, then check
    // if not entirely clipped away in the 360 degree angle clipper.
    center[VX] = mo->pos[VX];
    center[VY] = mo->pos[VY];
    M_ProjectViewRelativeLine2D(center, mf || (align || alwaysAlign == 3),
                                width, offset, v1, v2);

    // Check for visibility.
    if(!C_CheckViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]))
    {   // Isn't visible.
        if(mf)
        {
            // If the model is close to the viewpoint we will need to
            // draw it. Otherwise large models are likely to disappear
            // too early.
            if(P_ApproxDistance
                (distance, mo->pos[VZ] + (mo->height / 2) - viewData->current.pos[VZ]) >
               MAX_OBJECT_RADIUS)
            {
                return; // Can't be visible.
            }
        }
        else
        {
            return;
        }
    }
    }

    if(!mf)
    {
        visType = mf? VSPR_MODEL : VSPR_SPRITE;
    }
    else
    {   // Its a model.
        visType = VSPR_MODEL;
        thangle = BANG2RAD(bamsAtan2(pos[VY] * 10, pos[VX] * 10)) -
            PI / 2;

        // Viewaligning means scaling down Z with models.
        align = false;
    }

    // Store information in a vissprite.
    vis = R_NewVisSprite();
    vis->type = visType;
    vis->center[VX] = mo->pos[VX];
    vis->center[VY] = mo->pos[VY];
    vis->center[VZ] = mo->pos[VZ];
    vis->distance = distance;

    floorAdjust = (fabs(sect->SP_floorvisheight - sect->SP_floorheight) < 8);

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

    gzt = vis->center[VZ] + ((float) sprTex->offY);

    viewAlign = (align || alwaysAlign == 3)? true : false;
    fullBright = ((mo->state->flags & STF_FULLBRIGHT) || levelFullBright)? true : false;

    secFloor = mo->subsector->sector->SP_floorvisheight;
    secCeil = mo->subsector->sector->SP_ceilvisheight;

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
            yaw = R_MovementYaw(mo->mom[MX], mo->mom[MY]);
        }
        else
        {
            if(useSRVOAngle && !netGame && !playback)
                yaw = (mo->visAngle << 16) / (float) ANGLE_MAX * -360;
            else
                yaw = mo->angle / (float) ANGLE_MAX * -360;
        }

        // How about a unique offset?
        if(mf->sub[0].flags & MFF_IDANGLE)
        {
            // Multiply with an arbitrary factor.
            yaw += MOBJ_TO_ID(mo) % 360;
        }

        if(mf->sub[0].flags & MFF_ALIGN_PITCH)
        {
            pitch = -BANG2DEG(bamsAtan2
                (((vis->center[VZ] + gzt) / 2 - viewData->current.pos[VZ]) * 10,
                              distance * 10));
        }
        else if(mf->sub[0].flags & MFF_MOVEMENT_PITCH)
        {
            pitch = R_MovementPitch(mo->mom[MX], mo->mom[MY], mo->mom[MZ]);
        }
        else
            pitch = 0;
    }

    /**
     * The three highest bits of the selector are used for an alpha level.
     * 0 = opaque (alpha -1)
     * 1 = 1/8 transparent
     * 4 = 1/2 transparent
     * 7 = 7/8 transparent
     */
    i = mo->selector >> DDMOBJ_SELECTOR_SHIFT;
    if(i & 0xe0)
    {
        alpha = 1 - ((i & 0xe0) >> 5) / 8.0f;
    }
    else
    {
        if(mo->translucency)
            alpha = 1 - mo->translucency * reciprocal255;
        else
            alpha = -1;
    }

    // Determine possible short-range visual offset.
    V3_Set(visOff, 0, 0, 0);

    if((mf && useSRVO > 0) || (!mf && useSRVO > 1))
    {
        if(mo->state && mo->tics >= 0)
        {
            V3_Set(visOff, mo->srvo[VX], mo->srvo[VY], mo->srvo[VZ]);
            V3_Scale(visOff, (mo->tics - frameTimePos) / (float) mo->state->tics);
        }

        if(!INRANGE_OF(mo->mom[MX], 0, NOMOMENTUM_THRESHOLD) ||
           !INRANGE_OF(mo->mom[MY], 0, NOMOMENTUM_THRESHOLD) ||
           !INRANGE_OF(mo->mom[MZ], 0, NOMOMENTUM_THRESHOLD))
        {
            vec3_t              tmp;

            // Use the object's speed to calculate a short-range offset.
            V3_Set(tmp, mo->mom[MX], mo->mom[MY], mo->mom[MZ]);
            V3_Scale(tmp, frameTimePos);

            V3_Sum(visOff, visOff, tmp);
        }
    }

    if(!mf && mat)
    {
        boolean brightShadow = (mo->ddFlags & DDMF_BRIGHTSHADOW)? true : false;
        boolean shadow = (mo->ddFlags & DDMF_SHADOW)? true : false;
        boolean altShadow = (mo->ddFlags & DDMF_ALTSHADOW)? true : false;
        boolean fitTop = (mo->ddFlags & DDMF_FITTOP)? true : false;
        boolean fitBottom = (mo->ddFlags & DDMF_NOFITBOTTOM)? false : true;
        float finalAlpha;
        blendmode_t blendMode;

        if(useSpriteAlpha)
        {
            if(missileBlend && brightShadow)
                finalAlpha = .8f; // 80 %.
            else if(shadow)
                finalAlpha = .333f; // One third.
            else if(altShadow)
                finalAlpha = .666f; // Two thirds.
            else
                finalAlpha = 1;

            // Sprite has a custom alpha multiplier?
            if(alpha >= 0)
                finalAlpha *= alpha;
        }
        else
            finalAlpha = 1;

        if(missileBlend && brightShadow)
        {   // Additive blending.
            blendMode = BM_ADD;
        }
        else if(noSpriteTrans && finalAlpha >= .98f)
        {   // Use the "no translucency" blending mode.
            blendMode = BM_ZEROALPHA;
        }
        else
        {
            blendMode = BM_NORMAL;
        }

        // We must find the correct positioning using the sector floor
        // and ceiling heights as an aid.
        if(ms.height < secCeil - secFloor)
        {   // Sprite fits in, adjustment possible?
            // Check top.
            if(fitTop && gzt > secCeil)
                gzt = secCeil;
            // Check bottom.
            if(floorAdjust && fitBottom &&
               gzt - ms.height < secFloor)
                gzt = secFloor + ms.height;
        }
        // Adjust by the floor clip.
        gzt -= floorClip;

        getLightingParams(vis->center[VX], vis->center[VY],
                          gzt - ms.height / 2.0f,
                          mo->subsector, vis->distance, fullBright,
                          ambientColor, &vLightListIdx);

        setupSpriteParamsForVisSprite(&vis->data.sprite,
                                      vis->center[VX], vis->center[VY],
                                      gzt - ms.height / 2.0f,
                                      vis->distance,
                                      visOff[VX], visOff[VY], visOff[VZ],
                                      secFloor, secCeil,
                                      floorClip, gzt, mat, matFlipS, matFlipT, blendMode,
                                      ambientColor[CR], ambientColor[CG], ambientColor[CB], finalAlpha,
                                      vLightListIdx,
                                      tmap,
                                      tclass,
                                      mo->subsector,
                                      floorAdjust,
                                      fitTop,
                                      fitBottom,
                                      viewAlign,
                                      brightShadow, shadow, altShadow,
                                      fullBright);
    }
    else
    {
        getLightingParams(vis->center[VX], vis->center[VY], vis->center[VZ],
                          mo->subsector, vis->distance, fullBright,
                          ambientColor, &vLightListIdx);

        setupModelParamsForVisSprite(&vis->data.model,
                                     vis->center[VX], vis->center[VY], vis->center[VZ], vis->distance,
                                     visOff[VX], visOff[VY], visOff[VZ] - floorClip, gzt, yaw, 0, pitch, 0,
                                     mf, nextmf, interp,
                                     ambientColor[CR], ambientColor[CG], ambientColor[CB], alpha,
                                     vLightListIdx, mo->thinker.id, mo->selector,
                                     mo->subsector, mo->ddFlags,
                                     mo->tmap,
                                     viewAlign,
                                     fullBright && !(mf && (mf->sub[0].flags & MFF_DIM)),
                                     false);
    }

    // Do we need to project a flare source too?
    if(mo->lumIdx)
    {
        const lumobj_t* lum;
        const ded_light_t* def;
        float flareSize, xOffset;
        spritedef_t* sprDef;
        spriteframe_t* sprFrame;
        material_t* mat;
        material_snapshot_t ms;
        const gltexture_inst_t* texInst;

        // Determine the sprite frame lump of the source.
        sprDef = &sprites[mo->sprite];
        sprFrame = &sprDef->spriteFrames[mo->frame];
        if(sprFrame->rotate)
        {
            mat =
                sprFrame->
                mats[(R_PointToAngle(mo->pos[VX], mo->pos[VY]) - mo->angle +
                      (unsigned) (ANG45 / 2) * 9) >> 29];
        }
        else
        {
            mat = sprFrame->mats[0];
        }

#if _DEBUG
if(!mat)
    Con_Error("R_ProjectSprite: Sprite '%i' frame '%i' missing material.",
              (int) mo->sprite, mo->frame);
#endif

        // Ensure we have up-to-date information about the material.
        Material_Prepare(&ms, mat, true, NULL);
        if(ms.units[MTU_PRIMARY].texInst->tex->type != GLT_SPRITE)
            return; // *Very* strange...
        texInst = ms.units[MTU_PRIMARY].texInst;

        lum = LO_GetLuminous(mo->lumIdx);
        def = (mo->state? stateLights[mo->state - states] : 0);

        vis = R_NewVisSprite();
        vis->type = VSPR_FLARE;
        vis->distance = distance;

        // Determine the exact center of the flare.
        V3_Sum(vis->center, mo->pos, visOff);
        vis->center[VZ] += LUM_OMNI(lum)->zOff;

        flareSize = texInst->data.sprite.lumSize;
        // X offset to the flare position.
        xOffset = (texInst->data.sprite.flareX - (float) ms.width / 2) -
            (spriteTextures[texInst->tex->ofTypeID]->offX -
                (float) ms.width / 2);

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
        V3_Copy(vis->data.flare.color, LUM_OMNI(lum)->color);

        vis->data.flare.factor = mo->haloFactors[viewPlayer - ddPlayers];
        vis->data.flare.xOff = xOffset;
        vis->data.flare.mul = 1;

        if(def)
        {
            if(!(def->flare.id && def->flare.id[0] == '-'))
            {
                vis->data.flare.tex =
                    GL_GetFlareTexture(def->flare.id, -1);
            }
            else
            {
                vis->data.flare.flags |= RFF_NO_PRIMARY;
                vis->data.flare.tex = 0;
            }
        }
    }
}

typedef struct {
    subsector_t*        ssec;
} addspriteparams_t;

boolean RIT_AddSprite(void* ptr, void* data)
{
    mobj_t*             mo = (mobj_t*) ptr;
    addspriteparams_t*  params = (addspriteparams_t*) data;
    sector_t*           sec = params->ssec->sector;

    if(mo->addFrameCount != frameCount)
    {
        material_t*         mat;

        R_ProjectSprite(mo);

        // Hack: Sprites have a tendency to extend into the ceiling in
        // sky sectors. Here we will raise the skyfix dynamically, to make sure
        // that no sprites get clipped by the sky.
        // Only check
        if((mat = R_GetMaterialForSprite(mo->sprite, mo->frame)) &&
           R_IsSkySurface(&sec->SP_ceilsurface))
        {
            if(!(mo->dPlayer && mo->dPlayer->flags & DDPF_CAMERA) && // Cameramen don't exist!
               mo->pos[VZ] <= sec->SP_ceilheight &&
               mo->pos[VZ] >= sec->SP_floorheight)
            {
                float               visibleTop;

                visibleTop = mo->pos[VZ] + mat->height;

                if(visibleTop > skyFix[PLN_CEILING].height)
                {
                    // Raise skyfix ceiling.
                    skyFix[PLN_CEILING].height = visibleTop + 16; // Add some leeway.
                }
            }
        }

        mo->addFrameCount = frameCount;
    }

    return true; // Continue iteration.
}

void R_AddSprites(subsector_t* ssec)
{
    addspriteparams_t params;

    // Don't use validCount, because other parts of the renderer may
    // change it.
    if(ssec->addSpriteCount == frameCount)
        return; // Already added.

    params.ssec = ssec;
    R_IterateSubsectorContacts(ssec, OT_MOBJ, RIT_AddSprite, &params);

    ssec->addSpriteCount = frameCount;
}

void R_SortVisSprites(void)
{
    int                 i, count;
    vissprite_t*        ds, *best = 0;
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

    // Pull the vissprites out by distance.
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

static void scaleFloatRGB(float* out, const float* in, float mul)
{
    memset(out, 0, sizeof(float) * 3);
    R_ScaleAmbientRGB(out, in, mul);
}

/**
 * Iterator for processing light sources around a vissprite.
 */
boolean visSpriteLightIterator(const lumobj_t* lum, float xyDist, void* data)
{
    float               dist;
    float               intensity;
    boolean             addLight = false;
    vlightiterparams_t* params = (vlightiterparams_t*) data;

    if(!(lum->type == LT_OMNI || lum->type == LT_PLANE))
        return true; // Continue iteration.

    // Is the light close enough to make the list?
    switch(lum->type)
    {
    case LT_OMNI:
        {
        float               zDist;

        zDist = params->pos[VZ] - lum->pos[VZ] + LUM_OMNI(lum)->zOff;
        dist = P_ApproxDistance(xyDist, zDist);

        if(dist < (float) loMaxRadius)
        {
            // The intensity of the light.
            intensity = MINMAX_OF(0, (1 - dist / LUM_OMNI(lum)->radius) * 2, 1);
            if(intensity > .05f)
                addLight = true;
        }
        break;
        }
    case LT_PLANE:
        if(LUM_PLANE(lum)->intensity &&
           (LUM_PLANE(lum)->color[0] > 0 || LUM_PLANE(lum)->color[1] > 0 ||
            LUM_PLANE(lum)->color[2] > 0))
        {
            float           glowHeight =
                (MAX_GLOWHEIGHT * LUM_PLANE(lum)->intensity) * glowHeightFactor;

            // Don't make too small or too large glows.
            if(glowHeight > 2)
            {
                float           delta[3];

                if(glowHeight > glowHeightMax)
                    glowHeight = glowHeightMax;

                delta[VX] = params->pos[VX] - lum->pos[VX];
                delta[VY] = params->pos[VY] - lum->pos[VY];
                delta[VZ] = params->pos[VZ] - lum->pos[VZ];

                if(!((dist = M_DotProduct(delta, LUM_PLANE(lum)->normal)) < 0))
                {   // Is on the front of the glow plane.
                    addLight = true;
                    intensity = 1 - dist / glowHeight;
                }
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
        vlightnode_t*       node = NULL;

        node = newVLight();

        if(node)
        {
            vlight_t*            vlight = &node->vlight;

            switch(lum->type)
            {
            case LT_OMNI:
                vlight->affectedByAmbient = true;
                vlight->approxDist = dist;
                vlight->lightSide = 1;
                vlight->darkSide = 0;
                vlight->offset = 0;

                // Calculate the normalized direction vector, pointing out of
                // the light origin.
                vlight->vector[VX] = (lum->pos[VX] - params->pos[VX]) / dist;
                vlight->vector[VY] = (lum->pos[VY] - params->pos[VY]) / dist;
                vlight->vector[VZ] = (lum->pos[VZ] + LUM_OMNI(lum)->zOff -
                                        params->pos[VZ]) / dist;

                vlight->color[CR] = LUM_OMNI(lum)->color[CR] * intensity;
                vlight->color[CG] = LUM_OMNI(lum)->color[CG] * intensity;
                vlight->color[CB] = LUM_OMNI(lum)->color[CB] * intensity;
                break;

            case LT_PLANE:
                vlight->affectedByAmbient = true;
                vlight->approxDist = dist;
                vlight->lightSide = 1;
                vlight->darkSide = 0;
                vlight->offset = .3f;

                /**
                 * Calculate the normalized direction vector, pointing out of
                 * the vissprite.
                 * \fixme Project the nearest point on the surface to
                 * determine the real direction vector.
                 */
                vlight->vector[VX] = LUM_PLANE(lum)->normal[VX];
                vlight->vector[VY] = LUM_PLANE(lum)->normal[VY];
                vlight->vector[VZ] = -LUM_PLANE(lum)->normal[VZ];

                vlight->color[CR] = LUM_PLANE(lum)->color[CR] * intensity;
                vlight->color[CG] = LUM_PLANE(lum)->color[CG] * intensity;
                vlight->color[CB] = LUM_PLANE(lum)->color[CB] * intensity;
                break;

            default:
                Con_Error("visSpriteLightIterator: Invalid value, "
                          "lum->type = %i.", (int) lum->type);
                break;
            }

            linkVLightNodeToList(node, params->listIdx);
        }
    }

    return true; // Continue iteration.
}

uint R_CollectAffectingLights(const collectaffectinglights_params_t* params)
{
    uint                vLightListIdx;
    uint                i;
    vlightnode_t*       node;
    vlight_t*           vlight;

    if(!params)
        return 0;

    // Determine the lighting properties that affect this vissprite.
    vLightListIdx = 0;
    node = newVLight();
    vlight = &node->vlight;

    // Should always be lit with world light.
    vlight->affectedByAmbient = false;
    vlight->approxDist = 0;

    for(i = 0; i < 3; ++i)
    {
        vlight->vector[i] = worldLight[i];
        vlight->color[i] = params->ambientColor[i];
    }

    if(params->starkLight)
    {
        vlight->lightSide = .35f;
        vlight->darkSide = .5f;
        vlight->offset = 0;
    }
    else
    {
        /**
         * World light can both light and shade. Normal objects get
         * more shade than light (to prevent them from becoming too
         * bright when compared to ambient light).
         */
        vlight->lightSide = .2f;
        vlight->darkSide = .8f;
        vlight->offset = .3f;
    }

    vLightListIdx = newVLightList(true); // Sort by distance.
    linkVLightNodeToList(node, vLightListIdx);

    // Add extra light by interpreting lumobjs into vlights.
    if(loInited && params->subsector)
    {
        vlightiterparams_t vars;

        vars.pos[VX] = params->center[VX];
        vars.pos[VY] = params->center[VY];
        vars.pos[VZ] = params->center[VZ];
        vars.haveList = true;
        vars.listIdx = vLightListIdx;

        LO_LumobjsRadiusIterator(params->subsector, params->center[VX],
                                 params->center[VY], (float) loMaxRadius,
                                 &vars, visSpriteLightIterator);
    }

    return vLightListIdx + 1;
}

/**
 * Calls func for all vlights in the given list.
 *
 * @param listIdx       Identifier of the list to process.
 * @param data          Ptr to pass to the callback.
 * @param func          Callback to make for each object.
 *
 * @return              @c true, iff every callback returns @c true.
 */
boolean VL_ListIterator(uint listIdx, void* data,
                        boolean (*func) (const vlight_t*, void*))
{
    vlightnode_t*       node;
    boolean             retVal, isDone;

    if(listIdx == 0 || listIdx > numVLightLinkLists)
        return true;
    listIdx--;

    node = vLightLinkLists[listIdx].head;
    retVal = true;
    isDone = false;
    while(node && !isDone)
    {
        if(!func(&node->vlight, data))
        {
            retVal = false;
            isDone = true;
        }
        else
            node = node->next;
    }

    return retVal;
}

/**
 * @return              The current floatbob offset for the mobj, if the mobj
 *                      is flagged for bobbing, else @c 0.
 */
float R_GetBobOffset(mobj_t* mo)
{
    if(mo->ddFlags & DDMF_BOB)
    {
        return (sin(MOBJ_TO_ID(mo) + ddMapTime / 1.8286 * 2 * PI) * 8);
    }

    return 0;
}
