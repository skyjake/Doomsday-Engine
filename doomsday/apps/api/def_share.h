/**
 * @file def_share.h
 * Shared definition data structures and constants. @ingroup defs
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_SHARED_GAME_DEFINITIONS_H
#define DE_SHARED_GAME_DEFINITIONS_H

#include "dd_types.h"
#include <doomsday/world/actions.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup defs
///@{

#define NUM_MOBJ_FLAGS          3
#define NUM_MOBJ_MISC           4
#define NUM_STATE_MISC          3

typedef struct {
    char            name[5];
} sprname_t;

typedef struct state_s {
    spritenum_t     sprite;
    int             flags;
    int             frame;
    int             tics;
    acfnptr_t       action;
    int             nextState;
    int             misc[NUM_STATE_MISC];
} state_t;

typedef enum {
    STATENAMES_FIRST,
    SN_SPAWN = STATENAMES_FIRST,
    SN_SEE,
    SN_PAIN,
    SN_MELEE,
    SN_MISSILE,
    SN_CRASH,
    SN_DEATH,
    SN_XDEATH,
    SN_RAISE,
    STATENAMES_COUNT
} statename_t;

typedef enum {
    SOUNDNAMES_FIRST,
    SDN_PAIN = SOUNDNAMES_FIRST,
    SDN_DEATH,
    SDN_ACTIVE,
    SDN_ATTACK,
    SDN_SEE,
    SOUNDNAMES_COUNT
} soundname_t;

typedef struct mobjinfo_s {
    int             doomEdNum;
    int             spawnHealth;
    float           speed;
    float           radius;
    float           height;
    int             mass;
    int             damage;
    int             flags;
    int             flags2;
    int             flags3;
    int             reactionTime;
    int             painChance;
    int             states[STATENAMES_COUNT];
    int             painSound;
    int             deathSound;
    int             activeSound;
    int             attackSound;
    int             seeSound;
    int             misc[NUM_MOBJ_MISC];
} mobjinfo_t;

typedef struct {
    char            lumpName[9];
    int             lumpNum;
    char*           extFile;
    void*           data;
} musicinfo_t;

typedef struct {
    char*           text; ///< Pointer to the text (don't modify).
} ddtext_t;

/**
 * @defgroup mapInfoFlags Map Info Flags
 * @ingroup defs apiFlags
 */
///@{
#define MIF_FOG                 0x1  ///< Fog is used in the map.
#define MIF_DRAW_SPHERE         0x2  ///< Always draw the sky sphere.
#define MIF_NO_INTERMISSION     0x4  ///< Skip any intermission between maps.
#define MIF_LIGHTNING           0x8  ///< Lightning is used in the map.
#define MIF_SPAWN_ALL_FIREMACES 0x10 ///< Heretic: Spawn all firemaces (disable randomness).
#define MIF_DIM_TORCH           0x20 ///< Heretic: Use the attenuated torch (not fullbright).
///@}

#define DDLT_MAX_APARAMS    10
#define DDLT_MAX_PARAMS     20
#define DDLT_MAX_SPARAMS    5

typedef struct {
    int             id;
    int             flags;
    int             flags2;
    int             flags3;
    int             lineClass;
    int             actType;
    int             actCount;
    float           actTime;
    int             actTag;
    int             aparm[DDLT_MAX_APARAMS];
    float           tickerStart, tickerEnd;
    int             tickerInterval;
    int             actSound, deactSound;
    int             evChain, actChain, deactChain;
    int             wallSection;
    materialid_t    actMaterial, deactMaterial;
    int             actLineType, deactLineType;
    char*           actMsg, *deactMsg;
    float           materialMoveAngle;
    float           materialMoveSpeed;
    int             iparm[DDLT_MAX_PARAMS];
    float           fparm[DDLT_MAX_PARAMS];
    char*           sparm[DDLT_MAX_SPARAMS];
} linetype_t;

#define DDLT_MAX_CHAINS     5

typedef struct {
    int             id;
    int             flags;
    int             actTag;
    int             chain[DDLT_MAX_CHAINS];
    int             chainFlags[DDLT_MAX_CHAINS];
    float           start[DDLT_MAX_CHAINS];
    float           end[DDLT_MAX_CHAINS];
    float           interval[DDLT_MAX_CHAINS][2];
    int             count[DDLT_MAX_CHAINS];
    int             ambientSound;
    float           soundInterval[2]; ///< min,max
    float           materialMoveAngle[2]; ///< floor, ceil
    float           materialMoveSpeed[2]; ///< floor, ceil
    float           windAngle;
    float           windSpeed;
    float           verticalWind;
    float           gravity;
    float           friction;
    char*           lightFunc;
    int             lightInterval[2];
    char*           colFunc[3]; ///< RGB
    int             colInterval[3][2];
    char*           floorFunc;
    float           floorMul, floorOff;
    int             floorInterval[2];
    char*           ceilFunc;
    float           ceilMul, ceilOff;
    int             ceilInterval[2];
} sectortype_t;

struct stateinfo_t
{
    mobjinfo_t *owner;
    struct ded_light_s *light;
    struct ded_ptcgen_s *ptcGens;
};

typedef struct sfxinfo_s
{
    void *data;           ///< Pointer to sound data.
    lumpnum_t lumpNum;
    char lumpName[9];     ///< Actual lump name of the sound (full name).
    char id[32];          ///< Identifier name (from the def).
    char name[32];        ///< Long name.
    struct sfxinfo_s *link;      ///< Link to another sound.
    int linkPitch;
    int linkVolume;
    int priority;
    int channels;         ///< Max. channels for the sound to occupy.
    int usefulness;       ///< Used to determine when to cache out.
    int flags;
    int group;
    ddstring_t external;  ///< Path to external file.
}
sfxinfo_t;

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_SHARED_GAME_DEFINITIONS_H
