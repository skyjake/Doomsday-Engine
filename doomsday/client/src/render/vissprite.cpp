/** @file vissprite.cpp Projected visible sprite ("vissprite") management.
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

#include "de_base.h"
#include "de_render.h"
#include "de_resource.h"

#include "render/vissprite.h"

using namespace de;

vissprite_t visSprites[MAXVISSPRITES], *visSpriteP;
vispsprite_t visPSprites[DDMAXPSPRITES];

vissprite_t visSprSortedHead;

static vissprite_t overflowVisSprite;

void R_ClearVisSprites()
{
    visSpriteP = visSprites;
}

vissprite_t *R_NewVisSprite(visspritetype_t type)
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

    zapPtr(spr);
    spr->type = type;

    return spr;
}

void VisSprite_SetupSprite(drawspriteparams_t &p,
    Vector3d const &center, coord_t distToEye, Vector3d const &visOffset,
    float /*secFloor*/, float /*secCeil*/, float /*floorClip*/, float /*top*/,
    Material &material, bool matFlipS, bool matFlipT, blendmode_t blendMode,
    Vector4f const &ambientColor,
    uint vLightListIdx, int tClass, int tMap, BspLeaf *bspLeafAtOrigin,
    bool /*floorAdjust*/, bool /*fitTop*/, bool /*fitBottom*/, bool viewAligned)
{
    MaterialVariantSpec const &spec = Rend_SpriteMaterialSpec(tClass, tMap);
    MaterialVariant *variant = material.chooseVariant(spec, true);

    DENG2_ASSERT((tClass == 0 && tMap == 0) ||
                 (spec.primarySpec->variant.flags & TSF_HAS_COLORPALETTE_XLAT));

    p.center[VX]      = center.x;
    p.center[VY]      = center.y;
    p.center[VZ]      = center.z;
    p.srvo[VX]        = visOffset.x;
    p.srvo[VY]        = visOffset.y;
    p.srvo[VZ]        = visOffset.z;
    p.distance        = distToEye;
    p.bspLeaf         = bspLeafAtOrigin;
    p.viewAligned     = viewAligned;
    p.noZWrite        = noSpriteZWrite;

    p.material        = variant;
    p.matFlip[0]      = matFlipS;
    p.matFlip[1]      = matFlipT;
    p.blendMode       = (useSpriteBlend? blendMode : BM_NORMAL);

    p.ambientColor[0] = ambientColor.x;
    p.ambientColor[1] = ambientColor.y;
    p.ambientColor[2] = ambientColor.z;
    p.ambientColor[3] = (useSpriteAlpha? ambientColor.w : 1);

    p.vLightListIdx   = vLightListIdx;
}

void VisSprite_SetupModel(drawmodelparams_t &p,
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

    p.ambientColor[0]   = ambientColor.x;
    p.ambientColor[1]   = ambientColor.y;
    p.ambientColor[2]   = ambientColor.z;
    p.ambientColor[3]   = ambientColor.w;

    p.vLightListIdx     = vLightListIdx;
}

void R_SortVisSprites()
{
    int count = visSpriteP - visSprites;
    if(!count) return;

    vissprite_t unsorted;
    unsorted.next = unsorted.prev = &unsorted;

    for(vissprite_t *ds = visSprites; ds < visSpriteP; ds++)
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
     * @todo Optimize:
     * Profile results from nuts.wad show over 25% of total execution time
     * was spent sorting vissprites (nuts.wad map01 is a perfect pathological
     * test case).
     *
     * Rather than try to speed up the sort, it would make more sense to
     * actually construct the vissprites in z order if it can be done in
     * linear time.
     */

    vissprite_t *best = 0;
    for(int i = 0; i < count; ++i)
    {
        coord_t bestdist = 0;
        for(vissprite_t *ds = unsorted.next; ds != &unsorted; ds = ds->next)
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
