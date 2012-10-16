/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * r_model.h: 3D Model Resources
 */

#ifndef __DOOMSDAY_REFRESH_MODEL_H__
#define __DOOMSDAY_REFRESH_MODEL_H__

#include "gl_model.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FRAME_MODELS        DED_MAX_SUB_MODELS

// Model frame flags.
#define MFF_FULLBRIGHT          0x00000001
#define MFF_SHADOW1             0x00000002
#define MFF_SHADOW2             0x00000004
#define MFF_BRIGHTSHADOW        0x00000008
#define MFF_MOVEMENT_PITCH      0x00000010 // Pitch aligned to movement.
#define MFF_SPIN                0x00000020 // Spin around (for bonus items).
#define MFF_SKINTRANS           0x00000040 // Color translation -> skins.
#define MFF_AUTOSCALE           0x00000080 // Scale to match sprite height.
#define MFF_MOVEMENT_YAW        0x00000100
#define MFF_DONT_INTERPOLATE    0x00000200 // Don't interpolate from the frame.
#define MFF_BRIGHTSHADOW2       0x00000400
#define MFF_ALIGN_YAW           0x00000800
#define MFF_ALIGN_PITCH         0x00001000
#define MFF_DARKSHADOW          0x00002000
#define MFF_IDSKIN              0x00004000 // Mobj id -> skin in skin range
#define MFF_DISABLE_Z_WRITE     0x00008000
#define MFF_NO_DISTANCE_CHECK   0x00010000
#define MFF_SELSKIN             0x00020000
#define MFF_PARTICLE_SUB1       0x00040000 // Sub1 center is particle origin.
#define MFF_NO_PARTICLES        0x00080000 // No particles for this object.
#define MFF_SHINY_SPECULAR      0x00100000 // Shiny skin rendered as additive.
#define MFF_SHINY_LIT           0x00200000 // Shiny skin is not fullbright.
#define MFF_IDFRAME             0x00400000 // Mobj id -> frame in frame range
#define MFF_IDANGLE             0x00800000 // Mobj id -> static angle offset
#define MFF_DIM                 0x01000000 // Never fullbright.
#define MFF_SUBTRACT            0x02000000 // Subtract blending.
#define MFF_REVERSE_SUBTRACT    0x04000000 // Reverse subtract blending.
#define MFF_TWO_SIDED           0x08000000 // Disable culling.
#define MFF_NO_TEXCOMP          0x10000000 // Never compress skins.
#define MFF_WORLD_TIME_ANIM     0x20000000

typedef struct {
    short           model;
    short           frame;
    char            frameRange;
    int             flags;
    short           skin;
    char            skinRange;
    float           offset[3];
    byte            alpha;
    struct texture_s* shinySkin;
    blendmode_t     blendMode;
} submodeldef_t;

#define MODELDEF_ID_MAXLEN      32

typedef struct modeldef_s {
    char            id[MODELDEF_ID_MAXLEN + 1];

    state_t*        state; // Pointer to the states list (in dd_defns.c).
    int             flags;
    unsigned int    group;
    int             select;
    short           skinTics;
    float           interMark; // [0,1) When is this frame in effect?
    float           interRange[2];
    float           offset[3];
    float           resize, scale[3];
    float           ptcOffset[MAX_FRAME_MODELS][3];
    float           visualRadius;
    ded_model_t*    def;

    // Points to next inter-frame, or NULL.
    struct modeldef_s* interNext;

    // Points to next selector, or NULL (only for "base" modeldefs).
    struct modeldef_s* selectNext;

    // Submodels.
    submodeldef_t   sub[MAX_FRAME_MODELS];
} modeldef_t;

extern modeldef_t *modefs;
extern int numModelDefs;
extern byte useModels;
extern float rModelAspectMod;

void            R_InitModels(void);
void            R_ShutdownModels(void);
model_t*        R_ModelForId(uint modelRepositoryId);
float           R_CheckModelFor(struct mobj_s* mo, modeldef_t** mdef, modeldef_t** nextmdef);
modeldef_t*     R_CheckIDModelFor(const char* id);
int             R_ModelFrameNumForName(int modelnum, char* fname);
void            R_SetModelFrame(modeldef_t* modef, int frame);
void            R_SetSpriteReplacement(int sprite, char* modelname);

void R_PrecacheModelsForState(int stateIndex);

/**
 * @note The skins are also bound here once so they should be ready for use
 *       the next time they are needed.
 */
int R_PrecacheModelsForMobj(thinker_t* th, void* context);

void R_PrecacheModel(modeldef_t* modef);

#ifdef __cplusplus
} extern "C"
#endif

#endif
