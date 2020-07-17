/** @file r_things.cpp  Map Object => Vissprite Projection.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include "clientapp.h"
#include "dd_main.h"  // App_World()
#include "dd_loop.h"  // frameTimePos
#include "def_main.h"  // states
#include "gl/gl_main.h"
#include "gl/gl_tex.h"
#include "gl/gl_texmanager.h"  // GL_PrepareFlaremap
#include "network/net_main.h"  // clients[]
#include "render/rendersystem.h"
#include "render/r_main.h"
#include "render/angleclipper.h"
#include "render/stateanimator.h"
#include "render/rend_halo.h"
#include "render/vissprite.h"
#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "world/clientmobjthinkerdata.h"
#include "world/convexsubspace.h"
#include "world/subsector.h"

#include <doomsday/defs/sprite.h>
#include <doomsday/r_util.h>
#include <doomsday/world/bspleaf.h>
#include <doomsday/world/materials.h>
#include <de/legacy/vector1.h>
#include <de/modeldrawable.h>

using namespace de;
using world::World;

static void evaluateLighting(const Vec3d &origin, ConvexSubspace &subspaceAtOrigin,
    coord_t distToEye, bool fullbright, Vec4f &ambientColor, duint *vLightListIdx)
{
    if(fullbright)
    {
        ambientColor = Vec3f(1);
        *vLightListIdx = 0;
    }
    else
    {
        auto &subsec = subspaceAtOrigin.subsector().as<Subsector>();

#if 0
        Map &map = subsec.sector().map();
        if(useBias && map.hasLightGrid())
        {
            // Evaluate the position in the light grid.
            Vec4f color = map.lightGrid().evaluate(origin);
            // Apply light range compression.
            for(dint i = 0; i < 3; ++i)
            {
                color[i] += Rend_LightAdaptationDelta(color[i]);
            }
            ambientColor = color;
        }
        else
#endif
        {
            const Vec4f color = subsec.lightSourceColorfIntensity();

            dfloat lightLevel = color.w;
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

        *vLightListIdx = Rend_CollectAffectingLights(origin, ambientColor, &subspaceAtOrigin);
    }
}

/// @todo use Mobj_OriginSmoothed
static Vec3d mobjOriginSmoothed(mobj_t *mob)
{
    DE_ASSERT(mob);
    coord_t origin[] = { mob->origin[0], mob->origin[1], mob->origin[2] };

    // The client may have a Smoother for this object.
    if(netState.isClient && mob->dPlayer && P_GetDDPlayerIdx(mob->dPlayer) != consolePlayer)
    {
        Smoother_Evaluate(DD_Player(P_GetDDPlayerIdx(mob->dPlayer))->smoother(), origin);
    }

    return Vec3d(origin);
}

/**
 * Determine the correct Z coordinate for the mobj. The visible Z coordinate
 * may be slightly different than the actual Z coordinate due to smoothed
 * plane movement.
 *
 * @todo fixme: Should use the visual plane heights of subsectors.
 */
static void findMobjZOrigin(mobj_t &mob, bool floorAdjust, vissprite_t &vis)
{
    World::validCount++;
    Mobj_Map(mob).forAllSectorsTouchingMobj(mob, [&mob, &floorAdjust, &vis](world::Sector &sector) {
        if (floorAdjust && fequal(mob.origin[2], sector.floor().height()))
        {
            vis.pose.origin.z = sector.floor().as<Plane>().heightSmoothed();
        }
        if (fequal(mob.origin[2] + mob.height, sector.ceiling().height()))
        {
            vis.pose.origin.z = sector.ceiling().as<Plane>().heightSmoothed() - mob.height;
        }
        return LoopContinue;
    });
}

void R_ProjectSprite(mobj_t &mob)
{
    /// @todo Lots of stuff here! This needs to be broken down into multiple functions
    /// and/or classes that handle preprocessing of visible entities. Keep in mind that
    /// data/state can persist across frames in the mobjs' private data. -jk

    // Not all objects can/will be visualized. Skip this object if:
    // ...hidden?
    if((mob.ddFlags & DDMF_DONTDRAW)) return;
    // ...not linked into the map?
    if(!Mobj_HasSubsector(mob)) return;
    // ...in an invalid state?
    if(!mob.state || !runtimeDefs.states.indexOf(mob.state)) return;
    // ...no sprite frame is defined?
    const Record *spriteRec = Mobj_SpritePtr(mob);
    if(!spriteRec) return;
    // ...fully transparent?
    const dfloat alpha = Mobj_Alpha(mob);
    if(alpha <= 0) return;
    // ...origin lies in a sector with no volume?
    ConvexSubspace &subspace = Mobj_BspLeafAtOrigin(mob).subspace().as<ConvexSubspace>();
    auto &subsec = subspace.subsector().as<Subsector>();
    if(!subsec.hasWorldVolume()) return;

    const ClientMobjThinkerData *mobjData = THINKER_DATA_MAYBE(mob.thinker, ClientMobjThinkerData);

    // Determine distance to object.
    const Vec3d moPos = mobjOriginSmoothed(&mob);
    const coord_t distFromEye = Rend_PointDist2D(moPos);

    // Should we use a 3D model?
    FrameModelDef *mf = nullptr, *nextmf = nullptr;
    dfloat interp = 0;

    const render::StateAnimator *animator = nullptr; // GL2 model present?

    if(useModels)
    {
        mf = Mobj_ModelDef(mob, &nextmf, &interp);
        if(mf)
        {
            DE_ASSERT(mf->select == (mob.selector & DDMOBJ_SELECTOR_MASK))
            // Use a sprite if the object is beyond the maximum model distance.
            if(maxModelDistance && !(mf->flags & MFF_NO_DISTANCE_CHECK)
               && distFromEye > maxModelDistance)
            {
                mf = nextmf = nullptr;
                interp = -1;
            }
        }

        if(mobjData)
        {
            animator = mobjData->animator();
        }
    }

    const bool hasModel = (mf || animator);

    // Decide which material to use according to the sprite's angle and position
    // relative to that of the viewer.
    ClientMaterial *mat = nullptr;
    bool matFlipS = false;
    bool matFlipT = false;

    //try
    defn::Sprite const       sprite(*spriteRec);
    const defn::Sprite::View spriteView =
        sprite.nearestView(mob.angle, R_ViewPointToAngle(Vec2d(mob.origin)), !!hasModel);
    {
        if (auto *sprMat = world::Materials::get().materialPtr(*spriteView.material))
        {
            mat = &sprMat->as<ClientMaterial>();
        }
        matFlipS = spriteView.mirrorX;
    }
    /*catch(const defn::Sprite::MissingViewError &er)
    {
        // Log but otherwise ignore this error.
        LOG_GL_WARNING("Projecting sprite '%i' frame '%i': %s")
                << mob.sprite << mob.frame << er.asText();
    }*/
    if(!mat) return;
    MaterialAnimator &matAnimator = mat->getAnimator(Rend_SpriteMaterialSpec(mob.tclass, mob.tmap));

    // Ensure we've up to date info about the material.
    matAnimator.prepare();

    const Vec2ui &matDimensions = matAnimator.dimensions();
    TextureVariant *tex            = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;

    // A valid sprite texture in the "Sprites" scheme is required.
    if(!tex || tex->base().manifest().schemeName().compareWithoutCase(DE_STR("Sprites")))
    {
        return;
    }

    const bool fullbright = ((mob.state->flags & STF_FULLBRIGHT) != 0 || levelFullBright);
    // Align to the view plane? (Means scaling down Z with models)
    bool viewAlign  = (!hasModel && ((mob.ddFlags & DDMF_VIEWALIGN) || alwaysAlign == 1))
                       || alwaysAlign == 3;

    // Perform visibility checking by projecting a view-aligned line segment
    // relative to the viewer and determining if the whole of the segment has
    // been clipped away according to the 360 degree angle clipper.
    const coord_t visWidth = Mobj_VisualRadius(mob) * 2;  /// @todo ignorant of rotation...
    Vec2d v1, v2;
    R_ProjectViewRelativeLine2D(moPos, hasModel || viewAlign, visWidth,
                                (hasModel? 0 : coord_t(-tex->base().origin().x) - (visWidth / 2.0f)),
                                v1, v2);

    // Not visible?
    if(!ClientApp::render().angleClipper().checkRangeFromViewRelPoints(v1, v2))
    {
        const coord_t MAX_OBJECT_RADIUS = 128;

        // Sprite visibility is absolute.
        if(!hasModel) return;

        // If the model is close to the viewpoint we should still to draw it,
        // otherwise large models are likely to disappear too early.
        const viewdata_t *viewData = &viewPlayer->viewport();
        Vec2d delta(distFromEye, moPos.z + (mob.height / 2) - viewData->current.origin.z);
        if(M_ApproxDistance(delta.x, delta.y) > MAX_OBJECT_RADIUS)
            return;
    }

    // Store information in a vissprite.
    vissprite_t *vis = R_NewVisSprite(animator ? VSPR_MODELDRAWABLE :
                                            mf ? VSPR_MODEL :
                                                 VSPR_SPRITE);

    vis->pose.origin   = moPos;
    vis->pose.distance = distFromEye;

    // The Z origin of the visual should match that of the mobj. When smoothing
    // is enabled this requires examining all touched sector planes in the vicinity.
    Plane &floor     = subsec.visFloor();
    Plane &ceiling   = subsec.visCeiling();
    bool floorAdjust = false;
    if(!Mobj_OriginBehindVisPlane(&mob))
    {
        floorAdjust = de::abs(floor.heightSmoothed() - floor.height()) < 8;
        findMobjZOrigin(mob, floorAdjust, *vis);
    }

    coord_t topZ = vis->pose.origin.z + -tex->base().origin().y;  // global z top

    // Determine floor clipping.
    coord_t floorClip = mob.floorClip;
    if(mob.ddFlags & DDMF_BOB)
    {
        // Bobbing is applied using floorclip.
        floorClip += Mobj_BobOffset(mob);
    }

    // Determine angles.
    /// @todo Surely this can be done in a subclass/function. -jk
    dfloat yaw = 0, pitch = 0;

    // Determine the rotation angles (in degrees).
    if((mf && mf->testSubFlag(0, MFF_ALIGN_YAW)) ||
       (animator && animator->model().alignYaw == render::Model::AlignToView))
    {
        // Transform the origin point.
        const viewdata_t *viewData = &viewPlayer->viewport();
        Vec2d delta(moPos.y - viewData->current.origin.y,
                       moPos.x - viewData->current.origin.x);

        yaw = 90 - (BANG2RAD(bamsAtan2(delta.x * 10, delta.y * 10)) - PI / 2) / PI * 180;
    }
    else if(mf && mf->testSubFlag(0, MFF_SPIN))
    {
        yaw = modelSpinSpeed * 70 * App_World().time() + MOBJ_TO_ID(&mob) % 360;
    }
    else if((mf && mf->testSubFlag(0, MFF_MOVEMENT_YAW)) ||
            (animator && animator->model().alignYaw == render::Model::AlignToMomentum))
    {
        yaw = R_MovementXYYaw(mob.mom[0], mob.mom[1]);
    }
    else
    {
        yaw = Mobj_AngleSmoothed(&mob) / dfloat(ANGLE_MAX) * -360.f;
    }

    // How about a unique offset?
    if((mf && mf->testSubFlag(0, MFF_IDANGLE)) ||
       (animator && animator->model().alignYaw == render::Model::AlignRandomly))
    {
        yaw += MOBJ_TO_ID(&mob) % 360;  // arbitrary
    }

    if((mf && mf->testSubFlag(0, MFF_ALIGN_PITCH)) ||
       (animator && animator->model().alignPitch == render::Model::AlignToView))
    {
        const viewdata_t *viewData = &viewPlayer->viewport();
        Vec2d delta(vis->pose.midZ() - viewData->current.origin.z, distFromEye);

        pitch = -BANG2DEG(bamsAtan2(delta.x * 10, delta.y * 10));
    }
    else if((mf && mf->testSubFlag(0, MFF_MOVEMENT_PITCH)) ||
            (animator && animator->model().alignPitch == render::Model::AlignToMomentum))
    {
        pitch = R_MovementXYZPitch(mob.mom[0], mob.mom[1], mob.mom[2]);
    }

    // Determine possible short-range visual offset.
    Vec3d visOff;
    if((hasModel && useSRVO > 0) || (!hasModel && useSRVO > 1))
    {
        if(mob.tics >= 0)
        {
            visOff = Vec3d(mob.srvo) * (mob.tics - frameTimePos) / (float) mob.state->tics;
        }

        if(!INRANGE_OF(mob.mom[0], 0, NOMOMENTUM_THRESHOLD) ||
           !INRANGE_OF(mob.mom[1], 0, NOMOMENTUM_THRESHOLD) ||
           !INRANGE_OF(mob.mom[2], 0, NOMOMENTUM_THRESHOLD))
        {
            // Use the object's speed to calculate a short-range offset.
            // Note that the object may have momentum but still be blocked from moving
            // (e.g., Heretic gas pods).

            visOff += Vec3d(~mob.ddFlags & DDMF_MOVEBLOCKEDX ? mob.mom[VX] : 0.0,
                            ~mob.ddFlags & DDMF_MOVEBLOCKEDY ? mob.mom[VY] : 0.0,
                            ~mob.ddFlags & DDMF_MOVEBLOCKEDZ ? mob.mom[VZ] : 0.0) *
                      frameTimePos;
        }
    }

    // Will it be drawn as a 2D sprite?
    if(!hasModel)
    {
        const bool brightShadow = (mob.ddFlags & DDMF_BRIGHTSHADOW) != 0;
        const bool fitTop       = (mob.ddFlags & DDMF_FITTOP)       != 0;
        const bool fitBottom    = (mob.ddFlags & DDMF_NOFITBOTTOM)  == 0;

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
        if(matDimensions.y < ceiling.heightSmoothed() - floor.heightSmoothed())
        {
            // Sprite fits in, adjustment possible?
            if(fitTop && topZ > ceiling.heightSmoothed())
                topZ = ceiling.heightSmoothed();

            if(floorAdjust && fitBottom && topZ - matDimensions.y < floor.heightSmoothed())
                topZ = floor.heightSmoothed() + matDimensions.y;
        }
        // Adjust by the floor clip.
        topZ -= floorClip;

        Vec3d const origin(vis->pose.origin.x, vis->pose.origin.y, topZ - matDimensions.y / 2.0f);
        Vec4f ambientColor;
        duint vLightListIdx = 0;
        evaluateLighting(origin, subspace, vis->pose.distance, fullbright, ambientColor, &vLightListIdx);

        // Apply uniform alpha (overwritting intensity factor).
        ambientColor.w = alpha;

        VisSprite_SetupSprite(vis,
                              VisEntityPose(origin, visOff, viewAlign),
                              VisEntityLighting(ambientColor, vLightListIdx),
                              floor.heightSmoothed(), ceiling.heightSmoothed(),
                              floorClip, topZ, *mat, matFlipS, matFlipT, blendMode,
                              mob.tclass, mob.tmap,
                              &Mobj_BspLeafAtOrigin(mob),
                              floorAdjust, fitTop, fitBottom);
    }
    else // It will be drawn as a 3D model.
    {
        Vec4f ambientColor;
        duint vLightListIdx = 0;
        evaluateLighting(vis->pose.origin, subspace, vis->pose.distance,
                         fullbright && !animator, // GL2 models lit with more granularity
                         ambientColor, &vLightListIdx);

        // Apply uniform alpha (overwritting intensity factor).
        ambientColor.w = alpha;

        if(animator)
        {
            // Set up a GL2 model for drawing.
            vis->pose = VisEntityPose(vis->pose.origin,
                                      Vec3d(visOff.x, visOff.y, visOff.z - floorClip),
                                      /*viewAlign*/ false, topZ, yaw, 0, pitch, 0);
            vis->light = VisEntityLighting(ambientColor, vLightListIdx);

            vis->data.model2.object   = &mob;
            vis->data.model2.animator = animator;
            vis->data.model2.model    = &animator->model();
        }
        else
        {
            DE_ASSERT(mf);
            VisSprite_SetupModel(vis,
                                 VisEntityPose(vis->pose.origin,
                                               Vec3d(visOff.x, visOff.y, visOff.z - floorClip),
                                               viewAlign, topZ, yaw, 0, pitch, 0),
                                 VisEntityLighting(ambientColor, vLightListIdx),
                                 mf, nextmf, interp,
                                 mob.thinker.id, mob.selector,
                                 &Mobj_BspLeafAtOrigin(mob),
                                 mob.ddFlags, mob.tmap,
                                 fullbright && !(mf && mf->testSubFlag(0, MFF_DIM)), false);
        }
    }

    // Do we need to project a flare source too?
    if(mob.lumIdx != Lumobj::NoIndex && haloMode > 0)
    {
        /// @todo mark this light source visible for LensFx
        try
        {
            //const defn::Sprite::View spriteView = sprite.nearestView(mob.angle, R_ViewPointToAngle(mob.origin));

            // Lookup the Material for this Sprite and prepare the animator.
            MaterialAnimator &matAnimator = ClientMaterial::find(*spriteView.material)
                    .getAnimator(Rend_SpriteMaterialSpec(mob.tclass, mob.tmap));
            matAnimator.prepare();

            const Vec2ui &matDimensions = matAnimator.dimensions();
            TextureVariant *tex            = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;

            // A valid sprite texture in the "Sprites" scheme is required.
            if(!tex || tex->base().manifest().schemeName().compareWithoutCase(DE_STR("Sprites")))
            {
                return;
            }

            const auto *pl = (const pointlight_analysis_t *) tex->base().analysisDataPointer(res::Texture::BrightPointAnalysis);
            DE_ASSERT(pl);

            const Lumobj &lob = subsec.sector().map().as<Map>().lumobj(mob.lumIdx);
            vissprite_t *vis  = R_NewVisSprite(VSPR_FLARE);

            vis->pose.distance = distFromEye;

            // Determine the exact center of the flare.
            vis->pose.origin = moPos + visOff;
            vis->pose.origin.z += lob.zOffset();

            dfloat flareSize = pl->brightMul;
            // X offset to the flare position.
            dfloat xOffset = matDimensions.x * pl->originX - -tex->base().origin().x;

            // Does the mobj have an active light definition?
            const ded_light_t *def = (mob.state? runtimeDefs.stateInfo[runtimeDefs.states.indexOf(mob.state)].light : 0);
            if(def)
            {
                if(def->size)
                    flareSize = def->size;
                if(def->haloRadius)
                    flareSize = def->haloRadius;
                if(def->offset[0])
                    xOffset = def->offset[0];

                vis->data.flare.flags = def->flags;
            }

            vis->data.flare.size = flareSize * 60 * (50 + haloSize) / 100.0f;
            if(vis->data.flare.size < 8)
                vis->data.flare.size = 8;

            // Color is taken from the associated lumobj.
            V3f_Set(vis->data.flare.color, lob.color().x, lob.color().y, lob.color().z);

            vis->data.flare.factor = mob.haloFactors[DoomsdayApp::players().indexOf(viewPlayer)];
            vis->data.flare.xOff = xOffset;
            vis->data.flare.mul = 1;
            vis->data.flare.tex = 0;

            if (def && def->flare)
            {
                const res::Uri &flaremapResourceUri = *def->flare;
                if (flaremapResourceUri.path().toString().compareWithoutCase("-"))
                {
                    vis->data.flare.tex = GL_PrepareFlaremap(flaremapResourceUri);
                }
            }
        }
        catch(const defn::Sprite::MissingViewError &er)
        {
            // Log but otherwise ignore this error.
            LOG_GL_WARNING("Projecting flare source for sprite '%i' frame '%i': %s")
                    << mob.sprite << mob.frame << er.asText();
        }
        catch(const Resources::MissingResourceManifestError &er)
        {
            // Log but otherwise ignore this error.
            LOG_GL_WARNING("Projecting flare source for sprite '%i' frame '%i': %s")
                    << mob.sprite << mob.frame << er.asText();
        }
    }
}
