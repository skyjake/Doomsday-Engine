/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/*
 * All the global variables that store the internal state.
 * Theoretically speaking, the internal state of the engine
 * should be found by looking at the variables collected
 * here, and every relevant module will have to include
 * this header file.
 * In practice, things are a bit messy.
 */

#ifndef __H_STATE__
#define __H_STATE__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

// We need globally shared data structures,
//  for defining the global state variables.
#include "doomdata.h"

// We need the playr data structure as well.
#include "h_player.h"

#ifdef __GNUG__
#pragma interface
#endif

// ------------------------
// Command line parameters.
//
extern int     verbose;

extern boolean  nomonsters;        // checkparm of -nomonsters
extern boolean  respawnparm;       // checkparm of -respawn
extern boolean  devparm;           // checkparm of -devparm

extern boolean  debugmode;         // checkparm of -debug
extern boolean  altpal;            // checkparm to use an alternate palette routine

// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//

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
extern int      prevmap;

// Nightmare mode flag, single player.
extern boolean  respawnmonsters;
extern boolean  monsterinfight;

// Flag: true only if started as net deathmatch.
// An enum might handle altdeath/cooperative better.
extern boolean  deathmatch;        // only if started as net death

// -------------------------
// Status flags for refresh.
//

// Depending on view size - no status bar?
// Note that there is no way to disable the
//  status bar explicitely.

extern boolean  menuactive;        // Menu overlayed?
extern boolean  paused;            // Game Pause?

extern boolean  viewactive;

extern int      viewwindowx;
extern int      viewwindowy;

// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
extern int      viewangleoffset;

// Player taking events, and displaying.
#define consoleplayer   Get(DD_CONSOLEPLAYER)
#define displayplayer   Get(DD_DISPLAYPLAYER)

// -------------------------------------
// Scores, rating.
// Statistics on a given map, for intermission.
//
extern int      totalkills;
extern int      totalitems;
extern int      totalsecret;

// Timer, for scores.
extern int      levelstarttic;     // gametic at level start
extern int      leveltime;         // tics in game play for par
extern int      actual_leveltime;

// --------------------------------------
// DEMO playback/recording related stuff.
// No demo, there is a human player in charge?
// Disable save/end game?
extern boolean  usergame;

//?
//extern  boolean   demoplayback;
//extern  boolean   demorecording;

// Quit after playing a demo from cmdline.
extern boolean  singledemo;

//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//  according to user inputs. Partly load from
//  WAD, partly set at startup time.

#define gametic     Get(DD_GAMETIC)

// Bookkeeping on players - state.
extern player_t players[MAXPLAYERS];

// Player spawn spots for deathmatch.
#define MAX_DM_STARTS   16
extern thing_t  deathmatchstarts[16];
extern thing_t  *deathmatch_p;

// Intermission stats.
// Parameters for world map / intermission.

// LUT of ammunition limits for each kind.
// This doubles with BackPack powerup item.
extern int      maxammo[NUM_AMMO_TYPES];

//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern FILE    *debugfile;

// if true, load all graphics at level load
extern boolean  precache;

//?
// debug flag to cancel adaptiveness
extern boolean  singletics;

extern int      bodyqueslot;

// Needed to store the number of the dummy sky flat.
// Used for rendering, as well as tracking projectiles etc.
#define skyflatnum  Get(DD_SKYFLATNUM)
#define SKYFLATNAME  "F_SKY1"

extern int      rndindex;

#define maketic     Get(DD_MAKETIC)
#define ticdup      1

#endif
