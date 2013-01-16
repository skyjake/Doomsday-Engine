/**
 * @file r_things.h Map Object Management and Refresh
 * @ingroup render
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_THINGS_H
#define LIBDENG_RENDER_THINGS_H

#include "map/p_maptypes.h"
#include "resource/r_data.h"
#include "resource/materials.h"
#include "rend_model.h"

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

typedef struct {
    byte rotate; // 0= no rotations, 1= only front, 2= more...
    material_t *mats[8]; // Material to use for view angles 0-7
    byte flip[8]; // Flip (1 = flip) to use for view angles 0-7
} spriteframe_t;

typedef struct {
    char name[5];
    int numFrames;
    spriteframe_t *spriteFrames;
} spritedef_t;

#define MAXVISSPRITES   8192

// These constants are used as the type of vissprite_s.
typedef enum {
    VSPR_SPRITE,
    VSPR_MASKED_WALL,
    VSPR_MODEL,
    VSPR_FLARE
} visspritetype_t;

typedef struct rendmaskedwallparams_s {
    void *material; /// Material::Variant
    blendmode_t blendMode; ///< Blendmode to be used when drawing
                               /// (two sided mid textures only)
    struct wall_vertex_s {
        float pos[3]; ///< x y and z coordinates.
        float color[4];
    } vertices[4];

    double texOffset[2];
    float texCoord[2][2]; ///< u and v coordinates.

    DGLuint modTex; ///< Texture to modulate with.
    float modTexCoord[2][2]; ///< u and v coordinates.
    float modColor[4];
} rendmaskedwallparams_t;

typedef struct rendspriteparams_s {
// Position/Orientation/Scale
    coord_t center[3]; // The real center point.
    coord_t srvo[3]; // Short-range visual offset.
    coord_t distance; // Distance from viewer.
    boolean viewAligned;

// Appearance
    boolean noZWrite;
    blendmode_t blendMode;

    // Material:
    void *material; /// Material::Variant
    boolean matFlip[2]; // [S, T] Flip along the specified axis.

    // Lighting/color:
    float ambientColor[4];
    uint vLightListIdx;

// Misc
    struct bspleaf_s *bspLeaf;
} rendspriteparams_t;

/** @name rendFlareFlags */
//@{
/// Do not draw a primary flare (aka halo).
#define RFF_NO_PRIMARY 0x1
/// Flares do not turn in response to viewangle/viewdir
#define RFF_NO_TURN 0x2
//@}

typedef struct rendflareparams_s {
    byte flags; // @see rendFlareFlags.
    int size;
    float color[3];
    byte factor;
    float xOff;
    DGLuint tex; // Flaremap if flareCustom ELSE (flaretexName id. Zero = automatical)
    float mul; // Flare brightness factor.
    boolean isDecoration;
    uint lumIdx;
} rendflareparams_t;

#define MAX_VISSPRITE_LIGHTS    (10)

/**
 * vissprite_t is a mobj or masked wall that will be drawn refresh.
 */
typedef struct vissprite_s {
    struct vissprite_s *prev, *next;
    visspritetype_t type; // VSPR_* type of vissprite.
    coord_t distance; // Vissprites are sorted by distance.
    coord_t origin[3];

    // An anonymous union for the data.
    union vissprite_data_u {
        rendspriteparams_t sprite;
        rendmaskedwallparams_t wall;
        rendmodelparams_t model;
        rendflareparams_t flare;
    } data;
} vissprite_t;

#define VS_SPRITE(v)        (&((v)->data.sprite))
#define VS_WALL(v)          (&((v)->data.wall))
#define VS_MODEL(v)         (&((v)->data.model))
#define VS_FLARE(v)         (&((v)->data.flare))

typedef enum {
    VPSPR_SPRITE,
    VPSPR_MODEL
} vispspritetype_t;

typedef struct vispsprite_s {
    vispspritetype_t type;
    ddpsprite_t *psp;
    coord_t origin[3];

    union vispsprite_data_u {
        struct vispsprite_sprite_s {
            BspLeaf *bspLeaf;
            float alpha;
            boolean isFullBright;
        } sprite;
        struct vispsprite_model_s {
            BspLeaf *bspLeaf;
            coord_t gzt; // global top for silhouette clipping
            int flags; // for color translation and shadow draw
            uint id;
            int selector;
            int pClass; // player class (used in translation)
            coord_t floorClip;
            boolean stateFullBright;
            boolean viewAligned;    // Align to view plane.
            coord_t secFloor, secCeil;
            float alpha;
            coord_t visOff[3]; // Last-minute offset to coords.
            boolean floorAdjust; // Allow moving sprite to match visible floor.

            struct modeldef_s *mf, *nextMF;
            float yaw, pitch;
            float pitchAngleOffset;
            float yawAngleOffset;
            float inter; // Frame interpolation, 0..1
        } model;
    } data;
} vispsprite_t;

DENG_EXTERN_C int levelFullBright;
DENG_EXTERN_C spritedef_t *sprites;
DENG_EXTERN_C int numSprites;
DENG_EXTERN_C float pspOffset[2], pspLightLevelMultiplier;
DENG_EXTERN_C int alwaysAlign;
DENG_EXTERN_C float weaponOffsetScale, weaponFOVShift;
DENG_EXTERN_C int weaponOffsetScaleY;
DENG_EXTERN_C byte weaponScaleMode; // cvar
DENG_EXTERN_C float modelSpinSpeed;
DENG_EXTERN_C int maxModelDistance, noSpriteZWrite;
DENG_EXTERN_C int useSRVO, useSRVOAngle;
DENG_EXTERN_C vissprite_t visSprites[MAXVISSPRITES], *visSpriteP;
DENG_EXTERN_C vissprite_t visSprSortedHead;
DENG_EXTERN_C vispsprite_t visPSprites[DDMAXPSPRITES];
DENG_EXTERN_C int psp3d;

#ifdef __cplusplus
extern "C" {
#endif

material_t *R_GetMaterialForSprite(int sprite, int frame);

/// @return  Radius of the mobj as it would visually appear to be.
coord_t R_VisualRadius(struct mobj_s *mo);

/**
 * Calculate the strength of the shadow this mobj should cast.
 *
 * @note Implemented using a greatly simplified version of the lighting equation;
 *       no light diminishing or light range compression.
 */
float R_ShadowStrength(struct mobj_s *mo);

float R_Alpha(struct mobj_s *mo);

/**
 * @return  The current floatbob offset for the mobj, if the mobj is flagged
 *          for bobbing; otherwise @c 0.
 */
coord_t R_GetBobOffset(struct mobj_s *mo);

float R_MovementYaw(float const mom[2]);
float R_MovementXYYaw(float momx, float momy);

float R_MovementPitch(float const mom[3]);
float R_MovementXYZPitch(float momx, float momy, float momz);

/**
 * Generates a vissprite for a mobj if it might be visible.
 */
void R_ProjectSprite(struct mobj_s *mobj);

/**
 * If 3D models are found for psprites, here we will create vissprites for them.
 */
void R_ProjectPlayerSprites(void);

void R_SortVisSprites(void);

vissprite_t *R_NewVisSprite(void);

void R_AddSprites(BspLeaf *bspLeaf);

/// To be called at the start of the current render frame to clear the vissprite list.
void R_ClearVisSprites(void);

void R_InitSprites(void);

void R_ShutdownSprites(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RENDER_THINGS_H */
