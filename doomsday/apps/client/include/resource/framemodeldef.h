/** @file modeldef.h  3D model resource definition.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_RESOURCE_FRAMEMODELDEF_H
#define DE_RESOURCE_FRAMEMODELDEF_H

#include <vector>
#include <de/vector.h>
#include <doomsday/defs/ded.h>
#include <doomsday/defs/model.h>

#include "framemodel.h"

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
    int _flags;
    short skin;
    char skinRange;
    de::Vec3f offset;
    byte alpha;
    res::Texture *shinySkin;
    blendmode_t blendMode;

    SubmodelDef()
        : modelId(0)
        , frame(0)
        , frameRange(0)
        , _flags(0)
        , skin(0)
        , skinRange(0)
        , alpha(0)
        , shinySkin(0)
        , blendMode(BM_NORMAL)
    {}

    void setFlags(int newFlags)
    {
        _flags = newFlags;
    }

    /**
     * Tests if the flags in @a flag are all set for the submodel.
     *
     * @param flag  One or more flags.
     *
     * @return @c true, if all the flags were set; otherwise @c false.
     */
    bool testFlag(int flag) const
    {
        return (_flags & flag) == flag;
    }
};

#define MODELDEF_ID_MAXLEN      32

struct FrameModelDef
{
    char id[MODELDEF_ID_MAXLEN + 1];

    /// Pointer to the states list.
    state_t *state      = nullptr;

    int flags           = 0;
    uint group          = 0;
    int select          = 0;
    short skinTics      = 0;

    /// [0,1) When is this frame in effect?
    float interMark     = 0;
    float interRange[2];
    de::Vec3f offset;
    float resize        = 0;
    de::Vec3f scale;

    typedef std::vector<de::Vec3f> PtcOffsets;
    PtcOffsets _ptcOffset;

    float visualRadius  = 0;
    float shadowRadius  = 0; // if zero, visual radius used instead

    defn::Model def;

    /// Points to next inter-frame, or NULL.
    FrameModelDef *interNext = nullptr;

    /// Points to next selector, or NULL (only for "base" modeldefs).
    FrameModelDef *selectNext = nullptr;

    /// Submodels.
    typedef std::vector<SubmodelDef> Subs;
    Subs _sub;

    FrameModelDef(const char *modelDefId = "")
    {
        de::zap(id);
        de::zap(interRange);
        strncpy(id, modelDefId, MODELDEF_ID_MAXLEN);
    }

    SubmodelDef *addSub()
    {
        _sub.push_back(SubmodelDef());
        _ptcOffset.push_back(de::Vec3f());
        return &_sub.back();
    }

    void clearSubs()
    {
        _sub.clear();
        _ptcOffset.clear();
    }

    uint subCount() const
    {
        return uint(_sub.size());
    }

    bool testSubFlag(unsigned int subnum, int flag) const
    {
        if(!hasSub(subnum)) return false;
        return _sub[subnum].testFlag(flag);
    }

    modelid_t subModelId(unsigned int subnum) const
    {
        if(!hasSub(subnum)) return NOMODELID;
        return _sub[subnum].modelId;
    }

    SubmodelDef &subModelDef(unsigned int subnum)
    {
        DE_ASSERT(hasSub(subnum));
        return _sub[subnum];
    }

    const SubmodelDef &subModelDef(unsigned int subnum) const
    {
        DE_ASSERT(hasSub(subnum));
        return _sub[subnum];
    }

    bool hasSub(unsigned int subnum) const
    {
        return subnum < _sub.size();
    }

    de::Vec3f particleOffset(unsigned int subnum) const
    {
        if(hasSub(subnum))
        {
            DE_ASSERT(subnum < _ptcOffset.size());
            return _ptcOffset[subnum];
        }
        return de::Vec3f();
    }

    void setParticleOffset(unsigned int subnum, const de::Vec3f &off)
    {
        DE_ASSERT(hasSub(subnum));
        _ptcOffset[subnum] = off;
    }
};

#endif // DE_RESOURCE_FRAMEMODELDEF_H
