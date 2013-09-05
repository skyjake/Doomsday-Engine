/** @file vissprite.h Projected visible sprite ("vissprite") management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_VISSPRITE_H
#define DENG_CLIENT_RENDER_VISSPRITE_H

#ifdef __CLIENT__

#include <de/Vector>

#include "resource/r_data.h"
#include "rend_model.h"

#define MAXVISSPRITES   8192

// These constants are used as the type of vissprite_s.
typedef enum {
    VSPR_SPRITE,
    VSPR_MASKED_WALL,
    VSPR_MODEL,
    VSPR_FLARE
} visspritetype_t;

typedef struct rendmaskedwallparams_s {
    void *material; /// MaterialVariant
    blendmode_t blendMode; ///< Blendmode to be used when drawing
                               /// (two sided mid textures only)
    struct wall_vertex_s {
        float pos[3]; ///< x y and z coordinates.
        float color[4];
    } vertices[4];

    double texOffset[2];
    float texCoord[2][2]; ///< u and v coordinates.

    DGLuint modTex; ///< Texture to modulate with.
    float modTexCoord[2][2]; ///< [top-left, bottom-right][x, y]
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
    void *material; /// MaterialVariant
    boolean matFlip[2]; // [S, T] Flip along the specified axis.

    // Lighting/color:
    float ambientColor[4];
    uint vLightListIdx;

// Misc
    BspLeaf *bspLeaf;
} rendspriteparams_t;

/** @name rendFlareFlags */
//@{
/// Do not draw a primary flare (aka halo).
#define RFF_NO_PRIMARY      0x1
/// Flares do not turn in response to viewangle/viewdir
#define RFF_NO_TURN         0x2
//@}

typedef struct rendflareparams_s {
    byte flags; // @ref rendFlareFlags.
    int size;
    float color[3];
    byte factor;
    float xOff;
    DGLuint tex; // Flaremap if flareCustom ELSE (flaretexName id. Zero = automatical)
    float mul; // Flare brightness factor.
    boolean isDecoration;
    int lumIdx;
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

void VisSprite_SetupSprite(rendspriteparams_t &p,
    de::Vector3d const &center, coord_t distToEye, de::Vector3d const &visOffset,
    float secFloor, float secCeil, float floorClip, float top,
    Material &material, bool matFlipS, bool matFlipT, blendmode_t blendMode,
    de::Vector4f const &ambientColor,
    uint vLightListIdx, int tClass, int tMap, BspLeaf *bspLeafAtOrigin,
    bool floorAdjust, bool fitTop, bool fitBottom, bool viewAligned);

void VisSprite_SetupModel(rendmodelparams_t &p,
    de::Vector3d const &origin, coord_t distToEye, de::Vector3d const &visOffset,
    float gzt, float yaw, float yawAngleOffset, float pitch, float pitchAngleOffset,
    ModelDef *mf, ModelDef *nextMF, float inter,
    de::Vector4f const &ambientColor,
    uint vLightListIdx,
    int id, int selector, BspLeaf *bspLeafAtOrigin, int mobjDDFlags, int tmap,
    bool viewAlign, bool fullBright, bool alwaysInterpolate);

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

            ModelDef *mf, *nextMF;
            float yaw, pitch;
            float pitchAngleOffset;
            float yawAngleOffset;
            float inter; // Frame interpolation, 0..1
        } model;
    } data;
} vispsprite_t;

DENG_EXTERN_C vissprite_t visSprites[MAXVISSPRITES], *visSpriteP;
DENG_EXTERN_C vissprite_t visSprSortedHead;
DENG_EXTERN_C vispsprite_t visPSprites[DDMAXPSPRITES];

/// To be called at the start of the current render frame to clear the vissprite list.
void R_ClearVisSprites();

vissprite_t *R_NewVisSprite();

void R_SortVisSprites();

#endif // __CLIENT__

#endif // DENG_CLIENT_RENDER_VISSPRITE_H
