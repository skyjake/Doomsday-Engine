/** @file defs/dedtypes.h  Definition types and structures (DED v1).
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include <vector>
#include <de/libcore.h>
#include <de/Vector>
#include <de/memory.h>
#include "../uri.h"

#include "def_share.h"
#include "api_gl.h"
#include "dedarray.h"

#define DED_DUP_URI(u)              u = (u? new de::Uri(*u) : 0)

#define DED_SPRITEID_LEN            4
#define DED_STRINGID_LEN            31
#define DED_FUNC_LEN                255

#define DED_MAX_MATERIAL_LAYERS     1   ///< Maximum number of material layers.
#define DED_MAX_MATERIAL_DECORATIONS 16 ///< Maximum number of material (light) decorations.

#define DED_PTCGEN_ANY_MOBJ_TYPE    -2  ///< Particle generator applies to ANY mobj type.

typedef char ded_stringid_t[DED_STRINGID_LEN + 1];
typedef ded_stringid_t ded_string_t;
typedef ded_stringid_t ded_mobjid_t;
typedef ded_stringid_t ded_stateid_t;
typedef ded_stringid_t ded_soundid_t;
typedef ded_stringid_t ded_musicid_t;
typedef ded_stringid_t ded_funcid_t;
typedef char ded_func_t[DED_FUNC_LEN + 1];
typedef int ded_flags_t;
typedef char* ded_anystring_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_uri_s {
    de::Uri *uri;

    void release() {
        delete uri;
    }
    void reallocate() {
        DED_DUP_URI(uri);
    }
} ded_uri_t;

#ifdef _MSC_VER
// MSVC needs some hand-holding.
template struct LIBDOOMSDAY_PUBLIC DEDArray<ded_uri_t>;
#endif

// Embedded sound information.
typedef struct ded_embsound_s {
    ded_string_t    name;
    int             id; // Figured out at runtime.
    float           volume;
} ded_embsound_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_ptcstage_s {
    ded_flags_t     type;
    int             tics;
    float           variance; // Stage variance (time).
    float           color[4]; // RGBA
    float           radius;
    float           radiusVariance;
    ded_flags_t     flags;
    float           bounce;
    float           resistance; // Air resistance.
    float           gravity;
    float           vectorForce[3];
    float           spin[2]; // Yaw and pitch.
    float           spinResistance[2]; // Yaw and pitch.
    int             model;
    ded_string_t    frameName; // For model particles.
    ded_string_t    endFrameName;
    short           frame, endFrame;
    ded_embsound_t  sound, hitSound;

    void release() {}
    void reallocate() {}

    /**
     * Takes care of consistent variance.
     * Currently only used visually, collisions use the constant radius.
     * The variance can be negative (results will be larger).
     */
    float particleRadius(int ptcIDX) const;
} ded_ptcstage_t;

typedef struct {
    char            id[DED_SPRITEID_LEN + 1];

    void release() {}
} ded_sprid_t;

typedef struct {
    char            str[DED_STRINGID_LEN + 1];
} ded_str_t;

typedef struct {
    ded_stringid_t  id;
    int             value;

    void release() {}
} ded_flag_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_mobj_s {
    ded_mobjid_t    id;
    int             doomEdNum;
    ded_string_t    name;

    ded_stateid_t   states[STATENAMES_COUNT];

    ded_soundid_t   seeSound;
    ded_soundid_t   attackSound;
    ded_soundid_t   painSound;
    ded_soundid_t   deathSound;
    ded_soundid_t   activeSound;

    int             reactionTime;
    int             painChance;
    int             spawnHealth;
    float           speed;
    float           radius;
    float           height;
    int             mass;
    int             damage;
    ded_flags_t     flags[NUM_MOBJ_FLAGS];
    int             misc[NUM_MOBJ_MISC];

    void release() {}
    void reallocate() {}
} ded_mobj_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_state_s {
    ded_stateid_t   id; // ID of this state.
    ded_sprid_t     sprite;
    ded_flags_t     flags;
    int             frame;
    int             tics;
    ded_funcid_t    action;
    ded_stateid_t   nextState;
    int             misc[NUM_STATE_MISC];
    ded_anystring_t execute; // Console command.

    void release() {
        M_Free(execute);
    }
    void reallocate() {
        if(execute) execute = M_StrDup(execute);
    }
} ded_state_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_light_s {
    ded_stateid_t   state;
    char            uniqueMapID[64];
    float           offset[3]; /* Origin offset in world coords
                                  Zero means automatic */
    float           size; // Zero: automatic
    float           color[3]; // Red Green Blue (0,1)
    float           lightLevel[2]; // Min/max lightlevel for bias
    ded_flags_t     flags;
    de::Uri*        up, *down, *sides;
    de::Uri*        flare;
    float           haloRadius; // Halo radius (zero = no halo).

    void release() {
        delete up;
        delete down;
        delete sides;
        delete flare;
    }
    void reallocate() {
        DED_DUP_URI(up);
        DED_DUP_URI(down);
        DED_DUP_URI(sides);
        DED_DUP_URI(flare);
    }
} ded_light_t;

struct LIBDOOMSDAY_PUBLIC ded_submodel_t
{
    de::Uri*        filename;
    de::Uri*        skinFilename; // Optional; override model's skin.
    ded_string_t    frame;
    int             frameRange;
    ded_flags_t     flags; // ASCII string of the flags.
    int             skin;
    int             skinRange;
    de::Vector3f    offset; // Offset XYZ within model.
    float           alpha;
    float           parm; // Custom parameter.
    unsigned char   selSkinBits[2]; // Mask and offset.
    unsigned char   selSkins[8];
    de::Uri*        shinySkin;
    float           shiny;
    de::Vector3f    shinyColor;
    float           shinyReact;
    blendmode_t     blendMode; // bm_*

    ded_submodel_t()
        : filename(0)
        , skinFilename(0)
        , frameRange(0)
        , flags(0)
        , skin(0)
        , skinRange(0)
        , alpha(0)
        , parm(0)
        , shinySkin(0)
        , shiny(0)
        , shinyColor(1, 1, 1)
        , shinyReact(1.0f)
        , blendMode(BM_NORMAL)
    {
        de::zap(frame);
        de::zap(selSkinBits);
        de::zap(selSkins);
    }
};

struct LIBDOOMSDAY_PUBLIC ded_model_t
{
    ded_stringid_t  id; // Optional identifier for the definition.
    ded_stateid_t   state;
    int             off;
    ded_sprid_t     sprite; // Only used by autoscale.
    int             spriteFrame; // Only used by autoscale.
    ded_flags_t     group;
    int             selector;
    ded_flags_t     flags;
    float           interMark;
    float           interRange[2]; // 0-1 by default.
    int             skinTics; // Tics per skin in range.
    de::Vector3f    scale; // Scale XYZ
    float           resize;
    de::Vector3f    offset; // Offset XYZ
    float           shadowRadius; // Radius for shadow (0=auto).

    typedef std::vector<ded_submodel_t> Submodels;
    Submodels _sub;

    ded_model_t(char const *spriteId = "")
        : off(0)
        , spriteFrame(0)
        , group(0)
        , selector(0)
        , flags(0)
        , interMark(0)
        , skinTics(0)
        , scale(1, 1, 1)
        , resize(0)
        , shadowRadius(0)
    {
        de::zap(id);
        de::zap(state);
        de::zap(sprite);
        de::zap(interRange);

        strcpy(sprite.id, spriteId);
        interRange[1] = 1;
    }

    bool hasSub(unsigned int subnum) const
    {
        return subnum < _sub.size();
    }

    unsigned int subCount() const
    {
        return _sub.size();
    }

    ded_submodel_t &sub(unsigned int subnum)
    {
        DENG2_ASSERT(hasSub(subnum));
        return _sub[subnum];
    }

    ded_submodel_t const &sub(unsigned int subnum) const
    {
        DENG2_ASSERT(hasSub(subnum));
        return _sub[subnum];
    }

    void appendSub()
    {
        _sub.push_back(ded_submodel_t());
    }
};

typedef struct LIBDOOMSDAY_PUBLIC ded_sound_s {
    ded_soundid_t   id; // ID of this sound, refered to by others.
    ded_string_t    name; // A tag name for the sound.
    ded_string_t    lumpName; // Actual lump name of the sound ("DS" not included).
    de::Uri*        ext; // External sound file (WAV).
    ded_soundid_t   link; // Link to another sound.
    int             linkPitch;
    int             linkVolume;
    int             priority; // Priority classification.
    int             channels; // Max number of channels to occupy.
    int             group; // Exclusion group.
    ded_flags_t     flags; // Flags (like chg_pitch).

    void release() {
        delete ext;
    }
    void reallocate() {
        DED_DUP_URI(ext);
    }
} ded_sound_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_music_s {
    ded_musicid_t   id; // ID of this piece of music.
    ded_string_t    lumpName; // Lump name.
    de::Uri*        path; // External file (not a normal MUS file).
    int             cdTrack; // 0 = no track.

    void release() {
        delete path;
    }
    void reallocate() {
        DED_DUP_URI(path);
    }
} ded_music_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_skylayer_s {
    ded_flags_t     flags;
    de::Uri*        material;
    float           offset;
    float           colorLimit;

    void release() {
        delete material;
    }
    void reallocate() {
        DED_DUP_URI(material);
    }
} ded_skylayer_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_skymodel_s {
    ded_stringid_t  id;
    int             layer; // Defaults to -1.
    float           frameInterval; // Seconds per frame.
    float           yaw;
    float           yawSpeed; // Angles per second.
    float           coordFactor[3];
    float           rotate[2];
    ded_anystring_t execute; // Executed on every frame change.
    float           color[4]; // RGBA

    void release() {
        M_Free(execute);
    }
    void reallocate() {
        execute = M_StrDup(execute);
    }
} ded_skymodel_t;

#define NUM_SKY_LAYERS      2
#define NUM_SKY_MODELS      32

// Sky flags.
#define SIF_DRAW_SPHERE     0x1 ///< Always draw the sky sphere.

#define DEFAULT_SKY_HEIGHT               ( .666667f )
#define DEFAULT_SKY_SPHERE_XOFFSET       ( 0 )
#define DEFAULT_SKY_SPHERE_FADEOUT_LIMIT ( .3f )

typedef struct LIBDOOMSDAY_PUBLIC ded_sky_s {
    ded_stringid_t  id;
    ded_flags_t     flags; // Flags.
    float           height;
    float           horizonOffset;
    float           color[3]; // Color of sky-lit sectors.
    ded_skylayer_t  layers[NUM_SKY_LAYERS];
    ded_skymodel_t  models[NUM_SKY_MODELS];

    void release() {
        for(int i = 0; i < NUM_SKY_LAYERS; ++i) {
            layers[i].release();
        }
        for(int i = 0; i < NUM_SKY_MODELS; ++i) {
            models[i].release();
        }
    }
    void reallocate() {
        for(int i = 0; i < NUM_SKY_LAYERS; ++i) {
            layers[i].reallocate();
        }
        for(int i = 0; i < NUM_SKY_MODELS; ++i) {
            models[i].reallocate();
        }
    }
} ded_sky_t;

/// @todo These values should be tweaked a bit.
#define DEFAULT_FOG_START       0
#define DEFAULT_FOG_END         2100
#define DEFAULT_FOG_DENSITY     0.0001f
#define DEFAULT_FOG_COLOR_RED   138.0f/255
#define DEFAULT_FOG_COLOR_GREEN 138.0f/255
#define DEFAULT_FOG_COLOR_BLUE  138.0f/255

typedef struct LIBDOOMSDAY_PUBLIC ded_mapinfo_s {
    de::Uri*        uri; // ID of the map (e.g. E2M3 or MAP21).
    ded_string_t    name; // Name of the map.
    ded_string_t    author; // Author of the map.
    ded_flags_t     flags; // Flags.
    ded_musicid_t   music; // Music to play.
    float           parTime; // Par time, in seconds.
    float           fogColor[3]; // Fog color (RGB).
    float           fogStart;
    float           fogEnd;
    float           fogDensity;
    float           ambient; // Ambient light level.
    float           gravity; // 1 = normal.
    ded_stringid_t  skyID; // ID of the sky definition to use with this map. If not set, use the sky data in the mapinfo.
    ded_sky_t       sky;
    ded_anystring_t execute; // Executed during map setup (savegames, too).

    void release() {
        delete uri;
        delete execute;
        sky.release();
    }
    void reallocate() {
        DED_DUP_URI(uri);
        execute = M_StrDup(execute);
        sky.reallocate();
    }
} ded_mapinfo_t;

typedef struct {
    ded_stringid_t  id;
    char*           text;

    void release() {
        M_Free(text);
    }
} ded_text_t;

typedef struct {
    ded_stringid_t  id;
    DEDArray<ded_uri_t> materials;

    void release() {
        materials.clear();
    }
} ded_tenviron_t;

typedef struct {
    char *id;
    char *text;

    void release() {
        M_Free(id);
        M_Free(text);
    }
} ded_value_t;

typedef struct {
    ded_stringid_t  id;
    de::Uri*        before;
    de::Uri*        after;
    char*           script;

    void release() {
        delete before;
        delete after;
        M_Free(script);
    }
} ded_finale_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_linetype_s {
    int             id;
    char            comment[64];
    ded_flags_t     flags[3];
    ded_flags_t     lineClass;
    ded_flags_t     actType;
    int             actCount;
    float           actTime;
    int             actTag;
    int             aparm[9];
    //ded_stringid_t    aparm_str[3];   // aparms 4, 6, 9
    ded_stringid_t  aparm9;
    float           tickerStart;
    float           tickerEnd;
    int             tickerInterval;
    ded_soundid_t   actSound;
    ded_soundid_t   deactSound;
    int             evChain;
    int             actChain;
    int             deactChain;
    int             actLineType;
    int             deactLineType;
    ded_flags_t     wallSection;
    de::Uri*        actMaterial;
    de::Uri*        deactMaterial;
    char            actMsg[128];
    char            deactMsg[128];
    float           materialMoveAngle;
    float           materialMoveSpeed;
    int             iparm[20];
    char            iparmStr[20][64];
    float           fparm[20];
    char            sparm[5][128];

    void release() {
        delete actMaterial;
        delete deactMaterial;
    }
    void reallocate() {
        DED_DUP_URI(actMaterial);
        DED_DUP_URI(deactMaterial);
    }
} ded_linetype_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_sectortype_s {
    int             id;
    char            comment[64];
    ded_flags_t     flags;
    int             actTag;
    int             chain[5];
    ded_flags_t     chainFlags[5];
    float           start[5];
    float           end[5];
    float           interval[5][2];
    int             count[5];
    ded_soundid_t   ambientSound;
    float           soundInterval[2]; // min,max
    float           materialMoveAngle[2]; // floor, ceil
    float           materialMoveSpeed[2]; // floor, ceil
    float           windAngle;
    float           windSpeed;
    float           verticalWind;
    float           gravity;
    float           friction;
    ded_func_t      lightFunc;
    int             lightInterval[2];
    ded_func_t      colFunc[3]; // RGB
    int             colInterval[3][2];
    ded_func_t      floorFunc;
    float           floorMul, floorOff;
    int             floorInterval[2];
    ded_func_t      ceilFunc;
    float           ceilMul, ceilOff;
    int             ceilInterval[2];

    void release() {}
    void reallocate() {}
} ded_sectortype_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_detail_stage_s {
    int             tics;
    float           variance;
    de::Uri *       texture; // The file/lump with the detail texture.
    float           scale;
    float           strength;
    float           maxDistance;

    void release() {
        delete texture;
    }
    void reallocate() {
        DED_DUP_URI(texture);
    }
} ded_detail_stage_t;

// Flags for detail texture definitions.
#define DTLF_NO_IWAD        0x1 // Don't use if from IWAD.
#define DTLF_PWAD           0x2 // Can use if from PWAD.
#define DTLF_EXTERNAL       0x4 // Can use if from external resource.

typedef struct LIBDOOMSDAY_PUBLIC ded_detailtexture_s {
    de::Uri        *material1;
    de::Uri        *material2;
    ded_flags_t     flags;
    // There is only one stage.
    ded_detail_stage_t stage;

    void release() {
        delete material1;
        delete material2;
        stage.release();
    }
    void reallocate() {
        DED_DUP_URI(material1);
        DED_DUP_URI(material2);
        stage.reallocate();
    }
} ded_detailtexture_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_ptcgen_s {
    struct ded_ptcgen_s* stateNext; // List of generators for a state.
    ded_stateid_t   state; // Triggered by this state (if mobj-gen).
    de::Uri*        material;
    ded_mobjid_t    type; // Triggered by this type of mobjs.
    ded_mobjid_t    type2; // Also triggered by this type.
    int             typeNum;
    int             type2Num;
    ded_mobjid_t    damage; // Triggered by mobj damage of this type.
    int             damageNum;
    de::Uri*        map; // Triggered by this map.
    ded_flags_t     flags;
    float           speed; // Particle spawn velocity.
    float           speedVariance; // Spawn speed variance (0-1).
    float           vector[3]; // Particle launch vector.
    float           vectorVariance; // Launch vector variance (0-1). 1=totally random.
    float           initVectorVariance; // Initial launch vector variance (0-1).
    float           center[3]; // Offset to the mobj (relat. to source).
    int             subModel; // Model source: origin submodel #.
    float           spawnRadius;
    float           spawnRadiusMin; // Spawn uncertainty box.
    float           maxDist; // Max visibility for particles.
    int             spawnAge; // How long until spawning stops?
    int             maxAge; // How long until generator dies?
    int             particles; // Maximum number of particles.
    float           spawnRate; // Particles spawned per tic.
    float           spawnRateVariance;
    int             preSim; // Tics to pre-simulate when spawned.
    int             altStart;
    float           altStartVariance; // Probability for alt start.
    float           force; // Radial strength of the sphere force.
    float           forceRadius; // Radius of the sphere force.
    float           forceAxis[3]; /* Rotation axis of the sphere force
                                      (+ speed). */
    float           forceOrigin[3]; // Offset for the force sphere.
    DEDArray<ded_ptcstage_t> stages;

    void release() {
        delete material;
        delete map;
        stages.clear();
    }
    void reallocate() {
        DED_DUP_URI(map);
        DED_DUP_URI(material);
        stages.reallocate();
    }
} ded_ptcgen_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_shine_stage_s {
    int             tics;
    float           variance;
    de::Uri        *texture;
    de::Uri        *maskTexture;
    blendmode_t     blendMode; // Blend mode flags (bm_*).
    float           shininess;
    float           minColor[3];
    float           maskWidth;
    float           maskHeight;

    void release() {
        delete texture;
        delete maskTexture;
    }
    void reallocate() {
        DED_DUP_URI(texture);
        DED_DUP_URI(maskTexture);
    }
} ded_shine_stage_t;

// Flags for reflection definitions.
#define REFF_NO_IWAD        0x1 // Don't use if from IWAD.
#define REFF_PWAD           0x2 // Can use if from PWAD.
#define REFF_EXTERNAL       0x4 // Can use if from external resource.

typedef struct LIBDOOMSDAY_PUBLIC ded_reflection_s {
    de::Uri        *material;
    ded_flags_t     flags;
    // There is only one stage.
    ded_shine_stage_t stage;

    void release() {
        delete material;
        stage.release();
    }
    void reallocate() {
        DED_DUP_URI(material);
        stage.reallocate();
    }
} ded_reflection_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_group_member_s {
    de::Uri*        material;
    int             tics;
    int             randomTics;

    void release() {
        delete material;
    }
    void reallocate() {
        DED_DUP_URI(material);
    }
} ded_group_member_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_group_s {
    ded_flags_t flags;
    DEDArray<ded_group_member_t> members;

    void release() {
        members.clear();
    }
} ded_group_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_material_layer_stage_s {
    de::Uri*        texture;
    int             tics;
    float           variance; // Stage variance (time).
    float           glowStrength;
    float           glowStrengthVariance;
    float           texOrigin[2];

    void release() {
        delete texture;
    }
    void reallocate() {
        DED_DUP_URI(texture);
    }
} ded_material_layer_stage_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_material_layer_s {
    DEDArray<ded_material_layer_stage_t> stages;

    void release() {
        stages.clear();
    }
    void reallocate() {
        stages.reallocate();
    }
} ded_material_layer_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_decorlight_stage_s {
    int tics;
    float variance; // Stage variance (time).
    float pos[2]; // Coordinates on the surface.
    float elevation; // Distance from the surface.
    float color[3]; // Light color.
    float radius; // Dynamic light radius (-1 = no light).
    float haloRadius; // Halo radius (zero = no halo).
    float lightLevels[2]; // Fade by sector lightlevel.
    int sysFlareIdx;
    de::Uri *up, *down, *sides;
    de::Uri *flare; // Overrides sysFlareIdx

    void release() {
        delete up;
        delete down;
        delete sides;
        delete flare;
    }
    void reallocate() {
        DED_DUP_URI(up);
        DED_DUP_URI(down);
        DED_DUP_URI(sides);
        DED_DUP_URI(flare);
    }
} ded_decorlight_stage_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_material_decoration_s {
    int patternOffset[2];
    int patternSkip[2];
    DEDArray<ded_decorlight_stage_t> stages;

    void release() {
        stages.clear();
    }
    void reallocate() {
        stages.reallocate();
    }
} ded_material_decoration_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_material_s {
    de::Uri *uri;
    dd_bool autoGenerated;
    ded_flags_t flags;
    int width;
    int height; // In world units.
    ded_material_layer_t layers[DED_MAX_MATERIAL_LAYERS];
    ded_material_decoration_t decorations[DED_MAX_MATERIAL_DECORATIONS];

    void release() {
        delete uri;
        for(int i = 0; i < DED_MAX_MATERIAL_LAYERS; ++i) {
            layers[i].release();
        }
        for(int i = 0; i < DED_MAX_MATERIAL_DECORATIONS; ++i) {
            decorations[i].release();
        }
    }
    void reallocate() {
        DED_DUP_URI(uri);
        for(int i = 0; i < DED_MAX_MATERIAL_LAYERS; ++i) {
            layers[i].reallocate();
        }
        for(int i = 0; i < DED_MAX_MATERIAL_DECORATIONS; ++i) {
            decorations[i].reallocate();
        }
    }
} ded_material_t;

// An oldschool material-linked decoration definition.
typedef struct LIBDOOMSDAY_PUBLIC ded_decoration_s {
    int patternOffset[2];
    int patternSkip[2];
    // There is only one stage.
    ded_decorlight_stage_t stage;

    void release() {
        stage.release();
    }
    void reallocate() {
        stage.reallocate();
    }
} ded_decoration_t;

// There is a fixed number of light decorations in each decoration.
#define DED_DECOR_NUM_LIGHTS    16

// Flags for decoration definitions.
#define DCRF_NO_IWAD        0x1 // Don't use if from IWAD.
#define DCRF_PWAD           0x2 // Can use if from PWAD.
#define DCRF_EXTERNAL       0x4 // Can use if from external resource.

typedef struct LIBDOOMSDAY_PUBLIC ded_decor_s {
    de::Uri *material;
    ded_flags_t flags;
    ded_decoration_t lights[DED_DECOR_NUM_LIGHTS];

    void release() {
        delete material;
        for(int i = 0; i < DED_DECOR_NUM_LIGHTS; ++i) {
            lights[i].release();
        }
    }
    void reallocate() {
        DED_DUP_URI(material);
        for(int i = 0; i < DED_DECOR_NUM_LIGHTS; ++i) {
            lights[i].reallocate();
        }
    }
} ded_decor_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_compositefont_mappedcharacter_s {
    unsigned char   ch;
    de::Uri*        path;

    void release() {
        delete path;
    }
    void reallocate() {
        DED_DUP_URI(path);
    }
} ded_compositefont_mappedcharacter_t;

typedef struct LIBDOOMSDAY_PUBLIC ded_compositefont_s {
    de::Uri*        uri;
    DEDArray<ded_compositefont_mappedcharacter_t> charMap;

    void release() {
        delete uri;
        charMap.clear();
    }
} ded_compositefont_t;

#endif // LIBDOOMSDAY_DEFINITION_TYPES_H
