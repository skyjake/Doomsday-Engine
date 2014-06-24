/** @file r_things.cpp  Map Object => Vissprite Projection.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "render/r_things.h"

#include "de_render.h"
#include "dd_main.h" // App_WorldSystem()
#include "dd_loop.h" // frameTimePos
#include "def_main.h" // states

#include "gl/gl_tex.h"
#include "gl/gl_texmanager.h" // GL_PrepareFlaremap

#include "network/net_main.h" // clients[]

#include "render/vissprite.h"

#include "MaterialSnapshot"

#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "BspLeaf"
#include "SectorCluster"

#include <de/vector1.h>

using namespace de;

static void evaluateLighting(Vector3d const &origin, BspLeaf *bspLeafAtOrigin,
    coord_t distToEye, bool fullbright, Vector4f &ambientColor, uint *vLightListIdx)
{
    DENG2_ASSERT(bspLeafAtOrigin != 0);

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
            Vector4f color = bspLeafAtOrigin->map().lightGrid().evaluate(origin);
            // Apply light range compression.
            for(int i = 0; i < 3; ++i)
            {
                color[i] += Rend_LightAdaptationDelta(color[i]);
            }
            ambientColor = color;
        }
        else
        {
            Vector4f const color = bspLeafAtOrigin->cluster().lightSourceColorfIntensity();

            float lightLevel = color.w;
            /* if(spr->type == VSPR_DECORATION)
            {
                // Wall decorations receive an additional light delta.
                lightLevel += R_WallAngleLightLevelDelta(line, side);
            } */

            // Apply distance attenuation.
            lightLevel = Rend_AttenuateLightLevel(distToEye, lightLevel);

            // Add extra light.
            lightLevel = de::clamp(0.f, lightLevel + Rend_ExtraLightDelta(), 1.f);

            Rend_ApplyLightAdaptation(lightLevel);

            // Determine the final color.
            ambientColor = color * lightLevel;
        }

        Rend_ApplyTorchLight(ambientColor, distToEye);

        collectaffectinglights_params_t parm; zap(parm);
        parm.origin[VX]      = origin.x;
        parm.origin[VY]      = origin.y;
        parm.origin[VZ]      = origin.z;
        parm.bspLeaf         = bspLeafAtOrigin;
        parm.ambientColor[0] = ambientColor.x;
        parm.ambientColor[1] = ambientColor.y;
        parm.ambientColor[2] = ambientColor.z;

        *vLightListIdx = R_CollectAffectingLights(&parm);
    }
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

struct findmobjzoriginworker_params_t
{
    vissprite_t *vis;
    mobj_t const *mo;
    bool floorAdjust;
};

static int findMobjZOriginWorker(Sector *sector, void *parameters)
{
    DENG_ASSERT(sector != 0);
    DENG_ASSERT(parameters != 0);
    findmobjzoriginworker_params_t *p = (findmobjzoriginworker_params_t *) parameters;

    if(p->floorAdjust && p->mo->origin[VZ] == sector->floor().height())
    {
        p->vis->origin.z = sector->floor().heightSmoothed();
    }

    if(p->mo->origin[VZ] + p->mo->height == sector->ceiling().height())
    {
        p->vis->origin.z = sector->ceiling().heightSmoothed() - p->mo->height;
    }

    return false; // Continue iteration.
}

/**
 * Determine the correct Z coordinate for the mobj. The visible Z coordinate
 * may be slightly different than the actual Z coordinate due to smoothed
 * plane movement.
 *
 * @todo fixme: Should use the visual plane heights of sector clusters.
 */
static void findMobjZOrigin(mobj_t *mo, bool floorAdjust, vissprite_t *vis)
{
    DENG_ASSERT(mo != 0);
    DENG_ASSERT(vis != 0);

    findmobjzoriginworker_params_t params; zap(params);
    params.vis         = vis;
    params.mo          = mo;
    params.floorAdjust = floorAdjust;

    validCount++;
    Mobj_TouchedSectorsIterator(mo, findMobjZOriginWorker, &params);
}

void R_ProjectSprite(mobj_t *mo)
{
    if(!mo) return;

    // Not all objects can/will be visualized. Skip this object if:
    // ...hidden?
    if((mo->ddFlags & DDMF_DONTDRAW)) return;
    // ...not linked into the map?
    if(!Mobj_HasCluster(*mo)) return;
    // ...in an invalid state?
    if(!mo->state || !runtimeDefs.states.indexOf(mo->state)) return;
    // ...no sprite frame is defined?
    Sprite *sprite = Mobj_Sprite(*mo);
    if(!sprite) return;
    // ...fully transparent?
    float const alpha = Mobj_Alpha(mo);
    if(alpha <= 0) return;
    // ...origin lies in a sector with no volume?
    SectorCluster &cluster = Mobj_Cluster(*mo);
    if(!cluster.hasWorldVolume()) return;

    // Determine distance to object.
    Vector3d const moPos = mobjOriginSmoothed(mo);
    coord_t const distFromEye = Rend_PointDist2D(moPos);

    // Should we use a 3D model?
    ModelDef *mf = 0, *nextmf = 0;
    float interp = 0;
    if(useModels)
    {
        mf = Mobj_ModelDef(*mo, &nextmf, &interp);
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
    Material *mat = 0;
    bool matFlipS = false;
    bool matFlipT = false;

    try
    {
        SpriteViewAngle const &sprViewAngle =
            sprite->closestViewAngle(mo->angle, R_ViewPointToAngle(mo->origin), mf != 0);

        mat      = sprViewAngle.material;
        matFlipS = sprViewAngle.mirrorX;
    }
    catch(Sprite::MissingViewAngleError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_GL_WARNING("Projecting sprite '%i' frame '%i': %s")
            << mo->sprite << mo->frame << er.asText();
    }

    if(!mat) return;

    // A valid sprite texture in the "Sprites" scheme is required.
    MaterialSnapshot const &ms = mat->prepare(Rend_SpriteMaterialSpec(mo->tclass, mo->tmap));
    if(!ms.hasTexture(MTU_PRIMARY))
    {
        return;
    }

    Texture &tex = ms.texture(MTU_PRIMARY).generalCase();
    if(tex.manifest().schemeName().compareWithoutCase("Sprites"))
    {
        return;
    }

    bool const fullbright = ((mo->state->flags & STF_FULLBRIGHT) != 0 || levelFullBright);
    // Align to the view plane? (Means scaling down Z with models)
    bool const viewAlign  = (!mf && ((mo->ddFlags & DDMF_VIEWALIGN) || alwaysAlign == 1))
                            || alwaysAlign == 3;

    /*
     * Perform visibility checking by projecting a view-aligned line segment
     * relative to the viewer and determining if the whole of the segment has
     * been clipped away according to the 360 degree angle clipper.
     */
    coord_t const visWidth = Mobj_VisualRadius(*mo) * 2; /// @todo ignorant of rotation...
    Vector2d v1, v2;
    R_ProjectViewRelativeLine2D(moPos, mf || viewAlign, visWidth,
                                (mf? 0 : coord_t(-tex.origin().x) - (visWidth / 2.0f)),
                                v1, v2);

    // Not visible?
    if(!C_CheckRangeFromViewRelPoints(v1, v2))
    {
#define MAX_OBJECT_RADIUS       128

        // Sprite visibility is absolute.
        if(!mf) return;

        // If the model is close to the viewpoint we should still to draw it,
        // otherwise large models are likely to disappear too early.
        viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
        Vector2d delta(distFromEye, moPos.z + (mo->height / 2) - viewData->current.origin.z);
        if(M_ApproxDistance(delta.x, delta.y) > MAX_OBJECT_RADIUS)
            return;

#undef MAX_OBJECT_RADIUS
    }

    // Store information in a vissprite.
    vissprite_t *vis = R_NewVisSprite(mf? VSPR_MODEL : VSPR_SPRITE);

    vis->origin   = moPos;
    vis->distance = distFromEye;

    /*
     * The Z origin of the visual should match that of the mobj. When smoothing
     * is enabled this requires examining all touched sector planes in the vicinity.
     */
    Plane &floor     = cluster.visFloor();
    Plane &ceiling   = cluster.visCeiling();
    bool floorAdjust = false;
    if(!Mobj_OriginBehindVisPlane(mo))
    {
        floorAdjust = de::abs(floor.heightSmoothed() - floor.height()) < 8;
        findMobjZOrigin(mo, floorAdjust, vis);
    }

    coord_t gzt = vis->origin.z + -tex.origin().y;

    // Determine floor clipping.
    coord_t floorClip = mo->floorClip;
    if(mo->ddFlags & DDMF_BOB)
    {
        // Bobbing is applied using floorclip.
        floorClip += Mobj_BobOffset(mo);
    }

    float yaw = 0, pitch = 0;
    if(mf)
    {
        // Determine the rotation angles (in degrees).
        if(mf->testSubFlag(0, MFF_ALIGN_YAW))
        {
            // Transform the origin point.
            viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
            Vector2d delta(moPos.y - viewData->current.origin.y,
                           moPos.x - viewData->current.origin.x);

            yaw = 90 - (BANG2RAD(bamsAtan2(delta.x * 10, delta.y * 10)) - PI / 2) / PI * 180;
        }
        else if(mf->testSubFlag(0, MFF_SPIN))
        {
            yaw = modelSpinSpeed * 70 * App_WorldSystem().time() + MOBJ_TO_ID(mo) % 360;
        }
        else if(mf->testSubFlag(0, MFF_MOVEMENT_YAW))
        {
            yaw = R_MovementXYYaw(mo->mom[MX], mo->mom[MY]);
        }
        else
        {
            yaw = Mobj_AngleSmoothed(mo) / float( ANGLE_MAX ) * -360;
        }

        // How about a unique offset?
        if(mf->testSubFlag(0, MFF_IDANGLE))
        {
            yaw += MOBJ_TO_ID(mo) % 360; // arbitrary
        }

        if(mf->testSubFlag(0, MFF_ALIGN_PITCH))
        {
            viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
            Vector2d delta((vis->origin.z + gzt) / 2 - viewData->current.origin.z, distFromEye);

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

    if(!mf)
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
        if(ms.height() < ceiling.heightSmoothed() - floor.heightSmoothed())
        {
            // Sprite fits in, adjustment possible?
            if(fitTop && gzt > ceiling.heightSmoothed())
                gzt = ceiling.heightSmoothed();

            if(floorAdjust && fitBottom && gzt - ms.height() < floor.heightSmoothed())
                gzt = floor.heightSmoothed() + ms.height();
        }
        // Adjust by the floor clip.
        gzt -= floorClip;

        Vector3d const origin(vis->origin.x, vis->origin.y, gzt - ms.height() / 2.0f);
        Vector4f ambientColor;
        uint vLightListIdx = 0;
        evaluateLighting(origin, &Mobj_BspLeafAtOrigin(*mo), vis->distance, fullbright,
                         ambientColor, &vLightListIdx);

        // Apply uniform alpha (overwritting intensity factor).
        ambientColor.w = alpha;

        VisSprite_SetupSprite(vis->data.sprite, origin, vis->distance, visOff,
                              floor.heightSmoothed(), ceiling.heightSmoothed(),
                              floorClip, gzt, *mat, matFlipS, matFlipT, blendMode,
                              ambientColor, vLightListIdx,
                              mo->tclass, mo->tmap,
                              &Mobj_BspLeafAtOrigin(*mo),
                              floorAdjust, fitTop, fitBottom, viewAlign);
    }
    else
    {
        Vector4f ambientColor;
        uint vLightListIdx = 0;
        evaluateLighting(vis->origin, &Mobj_BspLeafAtOrigin(*mo), vis->distance,
                         fullbright, ambientColor, &vLightListIdx);

        // Apply uniform alpha (overwritting intensity factor).
        ambientColor.w = alpha;

        VisSprite_SetupModel(vis->data.model, vis->origin, vis->distance,
                             Vector3d(visOff.x, visOff.y, visOff.z - floorClip),
                             gzt, yaw, 0, pitch, 0,
                             mf, nextmf, interp,
                             ambientColor, vLightListIdx, mo->thinker.id, mo->selector,
                             &Mobj_BspLeafAtOrigin(*mo),
                             mo->ddFlags, mo->tmap, viewAlign,
                             fullbright && !(mf && mf->testSubFlag(0, MFF_DIM)), false);
    }

    // Do we need to project a flare source too?
    if(mo->lumIdx != Lumobj::NoIndex && haloMode > 0)
    {
        /// @todo mark this light source visible for LensFx
        try
        {
            SpriteViewAngle const &sprViewAngle =
                sprite->closestViewAngle(mo->angle, R_ViewPointToAngle(mo->origin));

            Material *mat = sprViewAngle.material;

            // A valid sprite texture in the "Sprites" scheme is required.
            MaterialSnapshot const &ms = mat->prepare(Rend_SpriteMaterialSpec(mo->tclass, mo->tmap));
            if(!ms.hasTexture(MTU_PRIMARY))
                return;
            Texture &tex = ms.texture(MTU_PRIMARY).generalCase();
            if(tex.manifest().schemeName().compareWithoutCase("Sprites"))
                return;;
            pointlight_analysis_t const *pl = (pointlight_analysis_t const *)
                ms.texture(MTU_PRIMARY).generalCase().analysisDataPointer(Texture::BrightPointAnalysis);
            DENG2_ASSERT(pl != 0);

            Lumobj const *lum = cluster.sector().map().lumobj(mo->lumIdx);
            DENG_ASSERT(lum != 0);

            vissprite_t *vis = R_NewVisSprite(VSPR_FLARE);

            vis->distance = distFromEye;

            // Determine the exact center of the flare.
            vis->origin = moPos + visOff;
            vis->origin.z += lum->zOffset();

            float flareSize = pl->brightMul;
            // X offset to the flare position.
            float xOffset = ms.width() * pl->originX - -tex.origin().x;

            // Does the mobj have an active light definition?
            ded_light_t const *def = (mo->state? runtimeDefs.stateInfo[runtimeDefs.states.indexOf(mo->state)].light : 0);
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
            V3f_Set(vis->data.flare.color, lum->color().x, lum->color().y, lum->color().z);

            vis->data.flare.factor = mo->haloFactors[viewPlayer - ddPlayers];
            vis->data.flare.xOff = xOffset;
            vis->data.flare.mul = 1;
            vis->data.flare.tex = 0;

            if(def && def->flare)
            {
                de::Uri const &flaremapResourceUri = *def->flare;
                if(flaremapResourceUri.path().toStringRef().compareWithoutCase("-"))
                {
                    vis->data.flare.tex = GL_PrepareFlaremap(flaremapResourceUri);
                }
            }
        }
        catch(Sprite::MissingViewAngleError const &er)
        {
            // Log but otherwise ignore this error.
            LOG_GL_WARNING("Projecting flare source for sprite '%i' frame '%i': %s")
                << mo->sprite << mo->frame << er.asText();
        }
    }
}
