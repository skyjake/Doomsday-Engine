/** @file vissprite.h  Projected visible sprite ("vissprite") management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CLIENT_RENDER_VISSPRITE_H
#define DE_CLIENT_RENDER_VISSPRITE_H

#include <de/Vector>
#include <de/GLState>

#include "render/billboard.h"
#include "rend_model.h"

#define MAXVISSPRITES   8192

namespace world { class BspLeaf; }
class ClientMaterial;

/**
 * These constants are used as the type of vissprite.
 * @ingroup render
 */
enum visspritetype_t
{
    VSPR_SPRITE,
    VSPR_MASKED_WALL,
    VSPR_MODEL,
    VSPR_MODEL_GL2,    ///< GL2 model (de::ModelDrawable)
    VSPR_FLARE
};

#define MAX_VISSPRITE_LIGHTS    (10)

/// @ingroup render
struct VisEntityPose
{
    de::Vec3d origin;
    float topZ = 0.f;               ///< Global top Z coordinate (origin Z is the bottom).
    de::Vec3d srvo;                 ///< Short-range visual offset.
    coord_t distance = 0.0;         ///< Distance from viewer.
    float yaw = 0.f;
    float extraYawAngle = 0.f;
    float yawAngleOffset = 0.f;     ///< @todo We do not need three sets of angles...
    float pitch = 0.f;
    float extraPitchAngle = 0.f;
    float pitchAngleOffset = 0.f;
    float extraScale = 0.f;
    bool viewAligned = false;
    bool mirrored = false;          ///< If true the model will be mirrored about its Z axis (in model space).

    VisEntityPose() = default;

    VisEntityPose(const de::Vec3d &origin_, const de::Vec3d &visOffset,
                  bool viewAlign_ = false,
                  de::dfloat topZ_ = 0,
                  de::dfloat yaw_ = 0,
                  de::dfloat yawAngleOffset_ = 0,
                  de::dfloat pitch_ = 0,
                  de::dfloat pitchAngleOffset_= 0)
        : origin(origin_)
        , topZ(topZ_)
        , srvo(visOffset)
        , distance(Rend_PointDist2D(origin_))
        , yaw(yaw_)
        , extraYawAngle(0)
        , yawAngleOffset(yawAngleOffset_)
        , pitch(pitch_)
        , extraPitchAngle(0)
        , pitchAngleOffset(pitchAngleOffset_)
        , extraScale(0)
        , viewAligned(viewAlign_)
        , mirrored(false)
    {}

    inline coord_t midZ() const { return (origin.z + topZ) / 2; }

    de::Vec3d mid() const { return de::Vec3d(origin.x, origin.y, midZ()); }
};

/// @ingroup render
struct VisEntityLighting
{
    de::Vec4f ambientColor;
    de::duint vLightListIdx = 0;

    VisEntityLighting() = default;

    VisEntityLighting(const de::Vec4f &ambientColor, de::duint lightListIndex)
        : ambientColor(ambientColor)
        , vLightListIdx(lightListIndex)
    {}

    void setupLighting(const de::Vec3d &origin, de::ddouble distance, const world::BspLeaf &bspLeaf);
};

/**
 * vissprite_t is a mobj or masked wall that will be drawn refresh.
 * @ingroup render
 */
struct vissprite_t
{
    vissprite_t *prev = nullptr;
    vissprite_t *next = nullptr;
    visspritetype_t type;

    VisEntityPose pose;
    VisEntityLighting light;

    // An anonymous union for the data.
    union vissprite_data_u {
        drawspriteparams_t sprite;
        drawmaskedwallparams_t wall;
        drawmodelparams_t model;
        drawmodel2params_t model2;
        drawflareparams_t flare;
    } data;
};

#define VS_SPRITE(v)        (&((v)->data.sprite))
#define VS_WALL(v)          (&((v)->data.wall))
#define VS_MODEL(v)         (&((v)->data.model))
#define VS_MODEL2(v)        (&((v)->data.model2))
#define VS_FLARE(v)         (&((v)->data.flare))

void VisSprite_SetupSprite(vissprite_t *spr, const VisEntityPose &pose, const VisEntityLighting &light,
    de::dfloat secFloor, de::dfloat secCeil, de::dfloat floorClip, de::dfloat top,
    ClientMaterial &material, bool matFlipS, bool matFlipT, blendmode_t blendMode,
    de::dint tClass, de::dint tMap, world::BspLeaf *bspLeafAtOrigin,
    bool floorAdjust, bool fitTop, bool fitBottom);

void VisSprite_SetupModel(vissprite_t *spr, const VisEntityPose &pose, const VisEntityLighting &light,
    FrameModelDef *mf, FrameModelDef *nextMF, de::dfloat inter,
    de::dint id, de::dint selector, world::BspLeaf *bspLeafAtOrigin, de::dint mobjDDFlags, de::dint tmap,
    bool fullBright, bool alwaysInterpolate);

/// @ingroup render
enum vispspritetype_t
{
    VPSPR_SPRITE,
    VPSPR_MODEL,
    VPSPR_MODEL2
};

/// @ingroup render
struct vispsprite_t
{
    vispspritetype_t type;
    ddpsprite_t *psp;
    de::Vec3d origin;
    const world::BspLeaf *bspLeaf;
    VisEntityLighting light;

    union vispsprite_data_u {
        struct vispsprite_sprite_s {
            de::dfloat alpha;
            dd_bool isFullBright;
        } sprite;
        struct vispsprite_model_s {
            coord_t topZ;                 ///< global top for silhouette clipping
            de::dint flags;               ///< for color translation and shadow draw
            de::duint id;
            de::dint selector;
            de::dint pClass;              ///< player class (used in translation)
            coord_t floorClip;
            dd_bool stateFullBright;
            dd_bool viewAligned;          ///< @c true= Align to view plane.
            coord_t secFloor;
            coord_t secCeil;
            de::dfloat alpha;
            de::ddouble visOff[3];        ///< Last-minute offset to coords.
            dd_bool floorAdjust;          ///< Allow moving sprite to match visible floor.

            FrameModelDef *mf;
            FrameModelDef *nextMF;
            de::dfloat yaw;
            de::dfloat pitch;
            de::dfloat pitchAngleOffset;
            de::dfloat yawAngleOffset;
            de::dfloat inter;             ///< Frame interpolation, 0..1
        } model;
        struct vispsprite_model2_s {
            const render::Model *model;
            const render::StateAnimator *animator;
            de::dfloat pitchAngleOffset;
            de::dfloat yawAngleOffset;
        } model2;
    } data;
};

DE_EXTERN_C vissprite_t visSprites[MAXVISSPRITES], *visSpriteP;
DE_EXTERN_C vissprite_t visSprSortedHead;
DE_EXTERN_C vispsprite_t visPSprites[DDMAXPSPRITES];

/// To be called at the start of the current render frame to clear the vissprite list.
void R_ClearVisSprites();

vissprite_t *R_NewVisSprite(visspritetype_t type);

void R_SortVisSprites();

#endif  // DE_CLIENT_RENDER_VISSPRITE_H
