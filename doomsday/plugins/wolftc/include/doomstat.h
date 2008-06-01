/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 * doomstat.h:
 *
 * All the global variables that store the internal state.
 * Theoretically speaking, the internal state of the engine
 * should be found by looking at the variables collected
 * here, and every relevant module will have to include
 * this header file.
 * In practice, things are a bit messy.
 */

#ifndef __D_STATE__
#define __D_STATE__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// We need globally shared data structures,
//  for defining the global state variables.
#include "doomdata.h"

// We need the playr data structure as well.
#include "d_player.h"

#ifdef __GNUG__
#pragma interface
#endif

// ------------------------
// Command line parameters.
//
extern int     verbose;

extern boolean  nomonsters;        // checkparm of -nomonsters
extern boolean  respawnparm;       // checkparm of -respawn
extern boolean  fastparm;          // checkparm of -fast

extern boolean  devparm;           // DEBUG: launched with -devparm

// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//
extern gamemode_t gamemode;
extern int      gamemodebits;
extern gamemission_t gamemission;


// -------------------------------------------
// Selected skill type, map etc.
//

// Defaults for menu, methinks.
extern skillmode_t  startskill;
extern int      startepisode;
extern int      startmap;

extern boolean  autostart;

// Selected by user.
extern skillmode_t  gameskill;
extern int      gameepisode;
extern int      gamemap;

// Nightmare mode flag, single player.
extern boolean  respawnmonsters;
extern boolean  monsterinfight;

// Flag: true only if started as net deathmatch.
// An enum might handle altdeath/cooperative better.
extern boolean  deathmatch;

// -------------------------
// Status flags for refresh.
//

// Depending on view size - no status bar?
// Note that there is no way to disable the
//  status bar explicitely.
extern boolean  statusbaractive;

extern boolean  paused;            // Game Pause?

extern boolean  viewActive;

extern boolean  nodrawers;
extern boolean  noblit;

#define viewwindowx     Get(DD_VIEWWINDOW_X)
#define viewwindowy     Get(DD_VIEWWINDOW_Y)

// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
extern int      viewangleoffset;

// Player taking events, and displaying.
#define CONSOLEPLAYER   Get(DD_CONSOLEPLAYER)
#define DISPLAYPLAYER   Get(DD_DISPLAYPLAYER)

// -------------------------------------
// Scores, rating.
// Statistics on a given map, for intermission.
//
extern int totalKills;
extern int totalItems;
extern int totalSecret;

// Timer, for scores.
extern int levelStartTic; // Game tic at level start.
extern int levelTime; // Tics in game play for par.
extern int actualLevelTime;

// --------------------------------------
// DEMO playback/recording related stuff.
// No demo, there is a human player in charge?
// Disable save/end game?
extern boolean  userGame;

// Quit after playing a demo from cmdline.
extern boolean  singleDemo;

//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//  according to user inputs. Partly load from
//  WAD, partly set at startup time.

#define GAMETIC     Get(DD_GAMETIC)

// Bookkeeping on players - state.
extern player_t players[MAXPLAYERS];

// Intermission stats.
// Parameters for world map / intermission.
extern wbstartstruct_t wmInfo;

// LUT of ammunition limits for each kind.
// This doubles with BackPack powerup item.
extern int      maxAmmo[NUM_AMMO_TYPES];

//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern char     baseDefault[1024];
extern FILE    *debugFile;

// if true, load all graphics at level load
extern boolean  precache;

// wipegamestate can be set to -1
//  to force a wipe on the next draw
extern gamestate_t wipeGameState;

extern int      bodyqueslot;

// Needed to store the number of the dummy sky flat.
// Used for rendering, as well as tracking projectiles etc.
#define SKYMASKMATERIAL     Get(DD_SKYMASKMATERIAL_NUM)
#define SKYFLATNAME         "F_SKY1"

extern int      rndindex;
extern int      prndindex;

#define maketic     Get(DD_MAKETIC)
#define ticdup      1

#endif
