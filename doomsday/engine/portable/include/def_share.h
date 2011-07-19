/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * def_share.h: Shared Definition Data Structures and Constants
 */

#ifndef __DOOMSDAY_GAME_DATA_H__
#define __DOOMSDAY_GAME_DATA_H__

#include "dd_types.h"

#define NUM_MOBJ_FLAGS          3
#define NUM_MOBJ_MISC           4
#define NUM_STATE_MISC          3

typedef struct {
    char            name[5];
} sprname_t;

typedef void    (C_DECL * acfnptr_t) ();

typedef struct state_s {
    int             sprite;
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

typedef struct {
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
    char*           text; // Pointer to the text (don't modify).
} ddtext_t;

// Map Info flags.
#define MIF_FOG             0x1 // Fog is used in the map.
#define MIF_DRAW_SPHERE     0x2 // Always draw the sky sphere.
#define MIF_NO_INTERMISSION 0x4 // Skip any intermission between maps.

typedef struct {
    char*           name;
    char*           author;
    int             music;
    int             flags; // MIF_* flags.
    float           ambient;
    float           gravity;
    float           parTime;
} ddmapinfo_t;

typedef struct {
    char*           after;
    char*           before;
    int             game;
    char*           script;
} ddfinale_t;

typedef ddfinale_t finalescript_t;

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
    materialnum_t   actMaterial, deactMaterial;
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
    float           soundInterval[2]; // min,max
    float           materialMoveAngle[2]; // floor, ceil
    float           materialMoveSpeed[2]; // floor, ceil
    float           windAngle;
    float           windSpeed;
    float           verticalWind;
    float           gravity;
    float           friction;
    char*           lightFunc;
    int             lightInterval[2];
    char*           colFunc[3]; // RGB
    int             colInterval[3][2];
    char*           floorFunc;
    float           floorMul, floorOff;
    int             floorInterval[2];
    char*           ceilFunc;
    float           ceilMul, ceilOff;
    int             ceilInterval[2];
} sectortype_t;

#endif
