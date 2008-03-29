/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * r_things.h: Object Management and Refresh
 */

#ifndef __DOOMSDAY_REFRESH_THINGS_H__
#define __DOOMSDAY_REFRESH_THINGS_H__

#include "p_mapdata.h"
#include "r_data.h"
#include "r_materials.h"

#define MAXVISSPRITES   8192

// These constants are used as the type of vissprite_s.
enum {
    VSPR_MASKED_WALL,
    VSPR_MAP_OBJECT,
    VSPR_HUD_MODEL,
    VSPR_HUD_SPRITE,
    VSPR_DECORATION
};

typedef struct rendmaskedwallparams_s {
    int             texture;
    boolean         masked;
    blendmode_t     blendMode; // Blendmode to be used when drawing
                               // (two sided mid textures only)
    struct wall_vertex_s {
        float           pos[3]; // x y and z coordinates.
        float           color[4];
    } vertices[4];
    float           texCoord[2][2]; // u and v coordinates.

    DGLuint         modTex; // Texture to modulate with.
    float           modTexCoord[2][2]; // u and v coordinates.
    float           modColor[3];
} rendmaskedwallparams_t;

// A vissprite_t is a mobj or masked wall that will be drawn during
// a refresh.
typedef struct vissprite_s {
    struct vissprite_s *prev, *next;
    byte            type;          // VSPR_* type of vissprite.
    float           distance;      // Vissprites are sorted by distance.
    float           center[3];
    struct lumobj_s *light;        // For the halo (NULL if no halo).

    // An anonymous union for the data.
    union vissprite_data_u {
        struct vissprite_mobj_s {
            material_t     *mat;
            boolean         matFlip[2]; // {X, Y} Flip material?
            subsector_t    *subsector;
            float           gzt; // global top for silhouette clipping
            int             flags; // for color translation and shadow draw
            uint            id;
            int             selector;
            int             pClass; // player class (used in translation)
            float           floorClip;
            boolean         viewAligned;    // Align to view plane.
            float           secFloor, secCeil;
            float           rgb[3]; // Sector light color.
            float           lightLevel;
            float           alpha;
            float           visOff[3]; // Last-minute offset to coords.
            boolean         floorAdjust; // Allow moving sprite to match visible floor.

            // For models:
            struct modeldef_s *mf, *nextMF;
            float           yaw, pitch;
            float           pitchAngleOffset;
            float           yawAngleOffset;
            float           inter; // Frame interpolation, 0..1
        } mo;
        rendmaskedwallparams_t wall;
        struct vissprite_hudsprite_s {
            subsector_t    *subsector;
            float           lightLevel;
            float           alpha;
            float           rgb[3];
            ddpsprite_t    *psp;
        } psprite;
        struct vissprite_decormodel_s {
            subsector_t    *subsector;
            float           alpha;
            struct modeldef_s *mf;
            float           yaw, pitch;
            float           pitchAngleOffset;
            float           yawAngleOffset;
            float           inter; // Frame interpolation, 0..1
        } decormodel;
    } data;
} vissprite_t;

typedef struct visspritelightparams_s {
    float           center[3];
    subsector_t    *subsector;

    uint            maxLights;
    boolean         starkLight; // World light has a more pronounced effect.
} visspritelightparams_t;

// Sprites are patches with a special naming convention so they can be
// recognized by R_InitSprites.  The sprite and frame specified by a
// mobj is range checked at run time.

// a sprite is a patch_t that is assumed to represent a three dimensional
// object and may have multiple rotations pre drawn.  Horizontal flipping
// is used to save space. Some sprites will only have one picture used
// for all views.

typedef struct {
    boolean         rotate; // If false use 0 for any position
    material_t     *mats[8]; // Material to use for view angles 0-7
    byte            flip[8]; // Flip (1 = flip) to use for view angles 0-7
} spriteframe_t;

typedef struct {
    int             numFrames;
    spriteframe_t  *spriteFrames;
} spritedef_t;

extern spritedef_t *sprites;
extern int      numSprites;
extern float    pspOffset[2];
extern int      alwaysAlign;
extern float    weaponOffsetScale, weaponFOVShift;
extern int      weaponOffsetScaleY;
extern float    modelSpinSpeed;
extern int      maxModelDistance, noSpriteZWrite;
extern int      useSRVO, useSRVOAngle;
extern vissprite_t visSprites[MAXVISSPRITES], *visSpriteP;
extern vissprite_t visPSprites[DDMAXPSPRITES];
extern vissprite_t visSprSortedHead;

void            R_GetSpriteInfo(int sprite, int frame, spriteinfo_t *sprinfo);
void            R_GetPatchInfo(lumpnum_t lump, patchinfo_t *info);
float           R_VisualRadius(struct mobj_s *mo);
float           R_GetBobOffset(struct mobj_s *mo);
float           R_MovementYaw(float momx, float momy);
float           R_MovementPitch(float momx, float momy, float momz);
void            R_ProjectSprite(struct mobj_s *mobj);
void            R_ProjectPlayerSprites(void);
void            R_SortVisSprites(void);
vissprite_t    *R_NewVisSprite(void);
void            R_AddSprites(sector_t *sec);
void            R_AddPSprites(void);
void            R_DrawSprites(void);
void            R_InitSprites(void);
void            R_ClearSprites(void);

void            R_ClipVisSprite(vissprite_t *vis, int xl, int xh);

void            R_SetAmbientColor(float *rgba, float lightLevel, float distance);
void            R_DetermineLightsAffectingVisSprite(const visspritelightparams_t *params,
                                                    vlight_t **ptr, uint *num);

#endif
