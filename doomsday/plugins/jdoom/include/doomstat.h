/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * doomstat.h: All the global variables that store the internal state.
 *
 * Theoretically speaking, the internal state of the game can be found by
 * looking at the variables collected here, and every relevant module will
 * have to include this header file. In practice, things are a bit messy.
 */

#ifndef __D_STATE__
#define __D_STATE__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "doomdata.h"
#include "d_player.h"

#ifdef __GNUG__
#pragma interface
#endif

// Command line parameters.
extern int     verbose;

extern boolean nomonsters;  // checkparm of -nomonsters
extern boolean respawnparm; // checkparm of -respawn
extern boolean fastparm; // checkparm of -fast
extern boolean devparm; // checkparm of -devparm (for debug)

// Game Mode - identify IWAD as shareware, retail etc.
extern gamemode_t gamemode;
extern int gamemodebits;
extern gamemission_t gamemission;
extern skillmode_t startskill;
extern int startepisode;
extern int startmap;
extern boolean autostart;
extern skillmode_t gameskill;
extern int gameepisode;
extern int gamemap;
extern boolean respawnmonsters;
extern boolean monsterinfight;
extern boolean deathmatch;

// Status flags for refresh.
extern boolean statusbaractive;
extern boolean paused; // Game Pause?
extern boolean viewactive;
extern boolean nodrawers;
extern boolean noblit;

#define viewwindowx         Get(DD_VIEWWINDOW_X)
#define viewwindowy         Get(DD_VIEWWINDOW_Y)

// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
extern int viewangleoffset;

// Player taking events, and displaying.
#define consoleplayer       Get(DD_CONSOLEPLAYER)
#define displayplayer       Get(DD_DISPLAYPLAYER)

// Statistics on a given map, for intermission.
extern int totalkills;
extern int totalitems;
extern int totalsecret;

// Timer, for scores.
extern int levelstarttic; // gametic at level start
extern int leveltime; // tics in game play for par

// No demo, there is a human player in charge? Disable save/end game?
extern boolean  usergame;

// Quit after playing a demo from cmdline.
extern boolean  singledemo;

#define gametic             Get(DD_GAMETIC)

// Bookkeeping on players - state.
extern player_t players[MAXPLAYERS];

// Player spawn spots for deathmatch.
#define MAX_DM_STARTS       16

extern spawnspot_t deathmatchstarts[MAX_DM_STARTS];
extern spawnspot_t *deathmatch_p;

// Intermission stats.
extern wbstartstruct_t wminfo;

// LUT of ammunition limits for each kind.
// This doubles with BackPack powerup item.
extern int maxammo[NUM_AMMO_TYPES];

// File handling stuff.
extern char basedefault[1024];
extern FILE *debugfile;

// if true, load all graphics at level load
extern boolean precache;

// wipegamestate can be set to -1 to force a wipe on the next draw
extern gamestate_t wipegamestate;

// debug flag to cancel adaptiveness
extern boolean singletics;

extern int bodyqueslot;

#define skyMaskMaterial     Get(DD_SKYFLATNUM)
#define SKYFLATNAME         "F_SKY1"

extern int rndindex;
extern int prndindex;

#define maketic             Get(DD_MAKETIC)
#define ticdup              1

#endif
