/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * def_share.h: Shared Definition Data Structures and Constants
 */

#ifndef __DOOMSDAY_GAME_DATA_H__
#define __DOOMSDAY_GAME_DATA_H__

#include "dd_types.h"

#define NUM_MOBJ_FLAGS	3
#define NUM_MOBJ_MISC	4
#define NUM_STATE_MISC	3

typedef struct {
	char name[5];
} sprname_t;

typedef void (C_DECL *acfnptr_t)();

typedef struct state_s {
	int sprite;
	int	flags;
	int frame;
	int tics;
	acfnptr_t action;
	int nextstate;
	int misc[NUM_STATE_MISC];
	void *model;
	void *light;
	void *ptrigger;
} state_t;

typedef struct {
    int	doomednum;
    int	spawnstate;
    int	spawnhealth;
    int	seestate;
    int	seesound;
    int	reactiontime;
    int	attacksound;
    int	painstate;
    int	painchance;
    int	painsound;
    int	meleestate;
    int	missilestate;
	int crashstate;
    int	deathstate;
    int	xdeathstate;
    int	deathsound;
    int	speed;
    int	radius;
    int	height;
    int	mass;
    int	damage;
    int	activesound;
    int	flags;
	int flags2;
	int flags3;
    int	raisestate;
	int misc[NUM_MOBJ_MISC];
} mobjinfo_t;

typedef struct {
	char lumpname[9];
	int lumpnum;
	char *extfile;
	void *data;
} musicinfo_t;

typedef struct {
	char *text;				// Pointer to the text (don't modify).	
} ddtext_t;

typedef struct {
	char *name;
	char *author;
	int music;
	int flags;
	float ambient;
	float gravity;
	float partime;
} ddmapinfo_t;

typedef struct {
	char *after;
	char *before;
	int game;
	char *script;
} ddfinale_t;

typedef ddfinale_t finalescript_t;

#define DDLT_MAX_APARAMS	10
#define DDLT_MAX_PARAMS		20
#define DDLT_MAX_SPARAMS	5

typedef struct {
	int id;
	int flags;
	int flags2;
	int flags3;
	int line_class;
	int act_type;
	int act_count;
	float act_time;
	int act_tag;
	int aparm[DDLT_MAX_APARAMS];
	float ticker_start, ticker_end;
	int ticker_interval;
	int act_sound, deact_sound;
	int ev_chain, act_chain, deact_chain;
	int wallsection, act_tex, deact_tex;
	char *act_msg, *deact_msg;
	float texmove_angle;
	float texmove_speed;
	int iparm[DDLT_MAX_PARAMS];
	float fparm[DDLT_MAX_PARAMS];
	char *sparm[DDLT_MAX_SPARAMS];
} linetype_t;

#define DDLT_MAX_CHAINS		5

typedef struct {
	int id;
	int flags;
	int act_tag;
	int chain[DDLT_MAX_CHAINS];
	int chain_flags[DDLT_MAX_CHAINS];
	float start[DDLT_MAX_CHAINS];
	float end[DDLT_MAX_CHAINS];
	float interval[DDLT_MAX_CHAINS][2];
	int count[DDLT_MAX_CHAINS];
	int ambient_sound;
	float sound_interval[2];	// min,max
	float texmove_angle[2];	// floor, ceil
	float texmove_speed[2];	// floor, ceil
	float wind_angle;
	float wind_speed;
	float vertical_wind;
	float gravity;
	float friction;
	char *lightfunc;
	int light_interval[2];
	char *colfunc[3];	// RGB
	int col_interval[3][2];
	char *floorfunc;
	float floormul, flooroff;
	int floor_interval[2];
	char *ceilfunc;
	float ceilmul, ceiloff;
	int ceil_interval[2];
} sectortype_t;

#endif

