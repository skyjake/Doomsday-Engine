/**
 * @file models.h
 *
 * 3D Model Resources.
 *
 * @ingroup resource
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

#ifndef LIBDENG_RESOURCE_MODELS_H
#define LIBDENG_RESOURCE_MODELS_H

#include <vector>
#include <de/Vector>

#include "def_data.h"
#include "gl/gl_model.h"

/// Unique identifier associated with each model in the collection.
typedef uint modelid_t;

/// Special value used to signify an invalid model id.
#define NOMODELID               0

/**
 * @defgroup modelFrameFlags Model frame flags
 * @ingroup flags
 */
///@{
#define MFF_FULLBRIGHT          0x00000001
#define MFF_SHADOW1             0x00000002
#define MFF_SHADOW2             0x00000004
#define MFF_BRIGHTSHADOW        0x00000008
#define MFF_MOVEMENT_PITCH      0x00000010 ///< Pitch aligned to movement.
#define MFF_SPIN                0x00000020 ///< Spin around (for bonus items).
#define MFF_SKINTRANS           0x00000040 ///< Color translation -> skins.
#define MFF_AUTOSCALE           0x00000080 ///< Scale to match sprite height.
#define MFF_MOVEMENT_YAW        0x00000100
#define MFF_DONT_INTERPOLATE    0x00000200 ///< Don't interpolate from the frame.
#define MFF_BRIGHTSHADOW2       0x00000400
#define MFF_ALIGN_YAW           0x00000800
#define MFF_ALIGN_PITCH         0x00001000
#define MFF_DARKSHADOW          0x00002000
#define MFF_IDSKIN              0x00004000 ///< Mobj id -> skin in skin range
#define MFF_DISABLE_Z_WRITE     0x00008000
#define MFF_NO_DISTANCE_CHECK   0x00010000
#define MFF_SELSKIN             0x00020000
#define MFF_PARTICLE_SUB1       0x00040000 ///< Sub1 center is particle origin.
#define MFF_NO_PARTICLES        0x00080000 ///< No particles for this object.
#define MFF_SHINY_SPECULAR      0x00100000 ///< Shiny skin rendered as additive.
#define MFF_SHINY_LIT           0x00200000 ///< Shiny skin is not fullbright.
#define MFF_IDFRAME             0x00400000 ///< Mobj id -> frame in frame range
#define MFF_IDANGLE             0x00800000 ///< Mobj id -> static angle offset
#define MFF_DIM                 0x01000000 ///< Never fullbright.
#define MFF_SUBTRACT            0x02000000 ///< Subtract blending.
#define MFF_REVERSE_SUBTRACT    0x04000000 ///< Reverse subtract blending.
#define MFF_TWO_SIDED           0x08000000 ///< Disable culling.
#define MFF_NO_TEXCOMP          0x10000000 ///< Never compress skins.
#define MFF_WORLD_TIME_ANIM     0x20000000
///@}

struct SubmodelDef
{
    modelid_t modelId;
    short frame;
    char frameRange;
    int flags;
    short skin;
    char skinRange;
    float offset[3];
    byte alpha;
    struct texture_s* shinySkin;
    blendmode_t blendMode;

    SubmodelDef()
        : modelId(0),
          frame(0),
          frameRange(0),
          flags(0),
          skin(0),
          skinRange(0),
          alpha(0),
          shinySkin(0),
          blendMode(BM_NORMAL)
    {
        de::zap(offset);
    }
};

typedef SubmodelDef submodeldef_t;

#define MODELDEF_ID_MAXLEN      32

struct ModelDef
{
    char id[MODELDEF_ID_MAXLEN + 1];

    /// Pointer to the states list.
    state_t* state;

    int flags;
    unsigned int group;
    int select;
    short skinTics;

    /// [0,1) When is this frame in effect?
    float interMark;
    float interRange[2];
    float offset[3];
    float resize;
    float scale[3];

    typedef std::vector<de::Vector3f> PtcOffsets;
    PtcOffsets ptcOffset;

    float visualRadius;

    ded_model_t* def;

    /// Points to next inter-frame, or NULL.
    ModelDef *interNext;

    /// Points to next selector, or NULL (only for "base" modeldefs).
    ModelDef *selectNext;

    /// Submodels.
    typedef std::vector<SubmodelDef> Subs;
    Subs sub;

    ModelDef(char const *modelDefId = "")
        : state(0),
          flags(0),
          group(0),
          select(0),
          skinTics(0),
          interMark(0),
          resize(0),
          visualRadius(0),
          def(0),
          interNext(0),
          selectNext(0)
    {
        de::zap(id);
        strncpy(id, modelDefId, MODELDEF_ID_MAXLEN);

        de::zap(interRange);
        de::zap(offset);
        de::zap(scale);
    }

    SubmodelDef *addSub()
    {
        sub.push_back(SubmodelDef());
        ptcOffset.push_back(de::Vector3f());
        return &sub.back();
    }

    void clearSubs()
    {
        sub.clear();
        ptcOffset.clear();
    }
};

typedef ModelDef modeldef_t;
typedef std::vector<ModelDef> ModelDefs;

#ifdef __CLIENT__

extern ModelDefs modefs;
extern byte useModels;
extern float rModelAspectMod;

/**
 * @pre States must be initialized before this.
 */
void Models_Init(void);

/**
 * Frees all memory allocated for models.
 */
void Models_Shutdown(void);

model_t* Models_ToModel(modelid_t id);

/**
 * Is there a model for this mobj? The decision is made based on the state and tics
 * of the mobj. Returns the modeldefs that are in effect at the moment (interlinks
 * checked appropriately).
 */
float Models_ModelForMobj(struct mobj_s* mo, modeldef_t** mdef, modeldef_t** nextmdef);

/**
 * Lookup a model definition by id.
 *
 * @param id  Unique id of the definition to lookup.
 * @return  Found model definition; otherwise @c 0.
 */
modeldef_t* Models_Definition(char const* id);

void Models_Cache(modeldef_t* modef);

/**
 * @note The skins are also bound here once so they should be ready for use
 *       the next time they are needed.
 */
int Models_CacheForMobj(thinker_t* th, void* context);

#endif // __CLIENT__

#endif // LIBDENG_RESOURCE_MODELS_H
