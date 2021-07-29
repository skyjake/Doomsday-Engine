/** @file dedtypes.h  Definition types and structures (DED v1).
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_DEFINITION_TYPES_H
#define LIBDOOMSDAY_DEFINITION_TYPES_H

#include <de/libcore.h>
#include <de/vector.h>
#include <de/legacy/memory.h>
#include "../uri.h"

#include "dd_share.h"
#include "def_share.h"
#include "api_gl.h"
#include "dedarray.h"

#define DED_DUP_URI(u) u = (u ? new res::Uri(*u) : 0)

#define DED_SPRITEID_LEN 4
#define DED_STRINGID_LEN 31
#define DED_FUNC_LEN 255

#define DED_MAX_MATERIAL_LAYERS 1 ///< Maximum number of material layers (map renderer limitations).
#define DED_MAX_MATERIAL_DECORATIONS 16 ///< Maximum number of material decorations (arbitrary).

#define DED_PTCGEN_ANY_MOBJ_TYPE -2 ///< Particle generator applies to ANY mobj type.

typedef char           ded_stringid_t[DED_STRINGID_LEN + 1];
typedef ded_stringid_t ded_string_t;
typedef ded_stringid_t ded_mobjid_t;
typedef ded_stringid_t ded_stateid_t;
typedef ded_stringid_t ded_soundid_t;
typedef ded_stringid_t ded_funcid_t;
typedef char           ded_func_t[DED_FUNC_LEN + 1];
typedef int            ded_flags_t;
typedef char *         ded_anystring_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_uri_s {
    res::Uri *uri;

    void release() { delete uri; }
    void reallocate() { DED_DUP_URI(uri); }
} ded_uri_t;

#ifdef _MSC_VER
// MSVC needs some hand-holding.
template struct LIBDOOMSDAY_PUBLIC DEDArray<ded_uri_t>;
#endif

// Embedded sound information.
typedef struct ded_embsound_s {
    ded_string_t name;
    int          id; // Figured out at runtime.
    float        volume;
} ded_embsound_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_ptcstage_s {
    ded_flags_t    type;
    int            tics;
    float          variance; // Stage variance (time).
    float          color[4]; // RGBA
    float          radius;
    float          radiusVariance;
    ded_flags_t    flags;
    float          bounce;
    float          resistance; // Air resistance.
    float          gravity;
    float          vectorForce[3];
    float          spin[2];           // Yaw and pitch.
    float          spinResistance[2]; // Yaw and pitch.
    int            model;
    ded_string_t   frameName; // For model particles.
    ded_string_t   endFrameName;
    short          frame, endFrame;
    ded_embsound_t sound, hitSound;

    void release() {}
    void reallocate() {}

    /**
     * Takes care of consistent variance.
     * Currently only used visually, collisions use the constant radius.
     * The variance can be negative (results will be larger).
     */
    float particleRadius(int ptcIDX) const;
} ded_ptcstage_t;

typedef struct ded_sprid_s {
    char id[DED_SPRITEID_LEN + 1];

    void release() {}
} ded_sprid_t;

typedef struct {
    char str[DED_STRINGID_LEN + 1];
} ded_str_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_light_s {
    ded_stateid_t state;
    char          uniqueMapID[64];
    float         offset[3];     /* Origin offset in world coords
                                  Zero means automatic */
    float         size;          // Zero: automatic
    float         color[3];      // Red Green Blue (0,1)
    float         lightLevel[2]; // Min/max lightlevel for bias
    ded_flags_t   flags;
    res::Uri *     up, *down, *sides;
    res::Uri *     flare;
    float         haloRadius; // Halo radius (zero = no halo).

    void release()
    {
        delete up;
        delete down;
        delete sides;
        delete flare;
    }
    void reallocate()
    {
        DED_DUP_URI(up);
        DED_DUP_URI(down);
        DED_DUP_URI(sides);
        DED_DUP_URI(flare);
    }
} ded_light_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_sound_s {
    ded_soundid_t id;       // ID of this sound, refered to by others.
    ded_string_t  name;     // A tag name for the sound.
    ded_string_t  lumpName; // Actual lump name of the sound ("DS" not included).
    res::Uri *     ext;      // External sound file (WAV).
    ded_soundid_t link;     // Link to another sound.
    int           linkPitch;
    int           linkVolume;
    int           priority; // Priority classification.
    int           channels; // Max number of channels to occupy.
    int           group;    // Exclusion group.
    ded_flags_t   flags;    // Flags (like chg_pitch).

    void release() { delete ext; }
    void reallocate() { DED_DUP_URI(ext); }
} ded_sound_t;

struct ded_text_t {
    ded_stringid_t id;
    char *         text;

    void release() { M_Free(text); }

    void setText(const char *newTextToCopy)
    {
        release();
        text = M_StrDup(newTextToCopy);
    }
};

typedef struct ded_tenviron_s {
    ded_stringid_t      id;
    DEDArray<ded_uri_t> materials;

    void release() { materials.clear(); }
} ded_tenviron_t;

typedef struct ded_value_s {
    char *id;
    char *text;

    void release()
    {
        M_Free(id);
        M_Free(text);
    }
} ded_value_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_linetype_s {
    int            id;
    char           comment[64];
    ded_flags_t    flags[3];
    ded_flags_t    lineClass;
    ded_flags_t    actType;
    int            actCount;
    float          actTime;
    int            actTag;
    int            aparm[9];
    ded_stringid_t aparm9;
    float          tickerStart;
    float          tickerEnd;
    int            tickerInterval;
    ded_soundid_t  actSound;
    ded_soundid_t  deactSound;
    int            evChain;
    int            actChain;
    int            deactChain;
    int            actLineType;
    int            deactLineType;
    ded_flags_t    wallSection;
    res::Uri *      actMaterial;
    res::Uri *      deactMaterial;
    char           actMsg[128];
    char           deactMsg[128];
    float          materialMoveAngle;
    float          materialMoveSpeed;
    int            iparm[20];
    char           iparmStr[20][64];
    float          fparm[20];
    char           sparm[5][128];

    void release()
    {
        delete actMaterial;
        delete deactMaterial;
    }
    void reallocate()
    {
        DED_DUP_URI(actMaterial);
        DED_DUP_URI(deactMaterial);
    }
} ded_linetype_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_sectortype_s {
    int           id;
    char          comment[64];
    ded_flags_t   flags;
    int           actTag;
    int           chain[5];
    ded_flags_t   chainFlags[5];
    float         start[5];
    float         end[5];
    float         interval[5][2];
    int           count[5];
    ded_soundid_t ambientSound;
    float         soundInterval[2];     // min,max
    float         materialMoveAngle[2]; // floor, ceil
    float         materialMoveSpeed[2]; // floor, ceil
    float         windAngle;
    float         windSpeed;
    float         verticalWind;
    float         gravity;
    float         friction;
    ded_func_t    lightFunc;
    int           lightInterval[2];
    ded_func_t    colFunc[3]; // RGB
    int           colInterval[3][2];
    ded_func_t    floorFunc;
    float         floorMul, floorOff;
    int           floorInterval[2];
    ded_func_t    ceilFunc;
    float         ceilMul, ceilOff;
    int           ceilInterval[2];

    void release() {}
    void reallocate() {}
} ded_sectortype_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_detail_stage_s {
    int      tics;
    float    variance;
    res::Uri *texture; // The file/lump with the detail texture.
    float    scale;
    float    strength;
    float    maxDistance;

    void release() { delete texture; }
    void reallocate() { DED_DUP_URI(texture); }
} ded_detail_stage_t;

// Flags for detail texture definitions.
#define DTLF_NO_IWAD 0x1  // Don't use if from IWAD.
#define DTLF_PWAD 0x2     // Can use if from PWAD.
#define DTLF_EXTERNAL 0x4 // Can use if from external resource.

typedef struct LIBDOOMSDAY_PUBLIC ded_detailtexture_s {
    res::Uri *   material1;
    res::Uri *   material2;
    ded_flags_t flags;
    // There is only one stage.
    ded_detail_stage_t stage;

    void release()
    {
        delete material1;
        delete material2;
        stage.release();
    }
    void reallocate()
    {
        DED_DUP_URI(material1);
        DED_DUP_URI(material2);
        stage.reallocate();
    }
} ded_detailtexture_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_ptcgen_s {
    struct ded_ptcgen_s *    stateNext; // List of generators for a state.
    ded_stateid_t            state;     // Triggered by this state (if mobj-gen).
    res::Uri *                material;
    ded_mobjid_t             type;  // Triggered by this type of mobjs.
    ded_mobjid_t             type2; // Also triggered by this type.
    int                      typeNum;
    int                      type2Num;
    ded_mobjid_t             damage; // Triggered by mobj damage of this type.
    int                      damageNum;
    res::Uri *                map; // Triggered by this map.
    ded_flags_t              flags;
    float                    speed;              // Particle spawn velocity.
    float                    speedVariance;      // Spawn speed variance (0-1).
    float                    vector[3];          // Particle launch vector.
    float                    vectorVariance;     // Launch vector variance (0-1). 1=totally random.
    float                    initVectorVariance; // Initial launch vector variance (0-1).
    float                    center[3];          // Offset to the mobj (relat. to source).
    int                      subModel;           // Model source: origin submodel #.
    float                    spawnRadius;
    float                    spawnRadiusMin; // Spawn uncertainty box.
    float                    maxDist;        // Max visibility for particles.
    int                      spawnAge;       // How long until spawning stops?
    int                      maxAge;         // How long until generator dies?
    int                      particles;      // Maximum number of particles.
    float                    spawnRate;      // Particles spawned per tic.
    float                    spawnRateVariance;
    int                      preSim; // Tics to pre-simulate when spawned.
    int                      altStart;
    float                    altStartVariance; // Probability for alt start.
    float                    force;            // Radial strength of the sphere force.
    float                    forceRadius;      // Radius of the sphere force.
    float                    forceAxis[3];     /* Rotation axis of the sphere force
                                      (+ speed). */
    float                    forceOrigin[3];   // Offset for the force sphere.
    DEDArray<ded_ptcstage_t> stages;

    void release()
    {
        delete material;
        delete map;
        stages.clear();
    }
    void reallocate()
    {
        DED_DUP_URI(map);
        DED_DUP_URI(material);
        stages.reallocate();
    }
} ded_ptcgen_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_shine_stage_s {
    int         tics;
    float       variance;
    res::Uri *   texture;
    res::Uri *   maskTexture;
    blendmode_t blendMode; // Blend mode flags (bm_*).
    float       shininess;
    float       minColor[3];
    float       maskWidth;
    float       maskHeight;

    void release()
    {
        delete texture;
        delete maskTexture;
    }
    void reallocate()
    {
        DED_DUP_URI(texture);
        DED_DUP_URI(maskTexture);
    }
} ded_shine_stage_t;

// Flags for reflection definitions.
#define REFF_NO_IWAD 0x1  // Don't use if from IWAD.
#define REFF_PWAD 0x2     // Can use if from PWAD.
#define REFF_EXTERNAL 0x4 // Can use if from external resource.

typedef struct LIBDOOMSDAY_PUBLIC ded_reflection_s {
    res::Uri *   material;
    ded_flags_t flags;
    // There is only one stage.
    ded_shine_stage_t stage;

    void release()
    {
        delete material;
        stage.release();
    }
    void reallocate()
    {
        DED_DUP_URI(material);
        stage.reallocate();
    }
} ded_reflection_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_group_member_s {
    res::Uri *material;
    int      tics;
    int      randomTics;

    void release() { delete material; }
    void reallocate() { DED_DUP_URI(material); }
} ded_group_member_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_group_s {
    ded_flags_t                  flags;
    DEDArray<ded_group_member_t> members;

    void release() { members.clear(); }

    ded_group_member_t *tryFindFirstMemberWithMaterial(const res::Uri &materialUri)
    {
        if (!materialUri.isEmpty())
        {
            for (int i = 0; i < members.size(); ++i)
            {
                if (members[i].material && *members[i].material == materialUri)
                {
                    return &members[i];
                }
                // Only animate if the first frame in the group?
                if (flags & AGF_FIRST_ONLY) break;
            }
        }
        return nullptr; // Not found.
    }
} ded_group_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_compositefont_mappedcharacter_s {
    unsigned char ch;
    res::Uri *     path;

    void release() { delete path; }
    void reallocate() { DED_DUP_URI(path); }
} ded_compositefont_mappedcharacter_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_compositefont_s {
    res::Uri *                                     uri;
    DEDArray<ded_compositefont_mappedcharacter_t> charMap;

    void release()
    {
        delete uri;
        charMap.clear();
    }
} ded_compositefont_t;

#endif // LIBDOOMSDAY_DEFINITION_TYPES_H
