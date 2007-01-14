/**\file
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
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

#ifndef __X_STATE__
#define __X_STATE__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

// We need globally shared data structures,
//  for defining the global state variables.
//#include "doomdata.h"

// We need the playr data structure as well.
#include "x_player.h"

#ifdef __GNUG__
#pragma interface
#endif

// ------------------------
// Command line parameters.
//
extern int     verbose;

extern boolean  nomonsters;        // checkparm of -nomonsters

extern boolean  respawnparm;       // checkparm of -respawn

extern boolean  randomclass;       // checkparm of -randclass

extern boolean  debugmode;         // checkparm of -debug

extern boolean  nofullscreen;      // checkparm of -nofullscreen

extern boolean  usergame;          // ok to save / end game

extern boolean  devparm;           // checkparm of -devparm

extern boolean  altpal;            // checkparm to use an alternate palette routine

extern boolean  cdrom;             // true if cd-rom mode active ("-cdrom")

extern boolean  deathmatch;        // only if started as net death

extern boolean  netcheat;          // allow cheating during netgames

// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//
extern gamemode_t gamemode;
extern int      gamemodebits;

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

// -------------------------
// Status flags for refresh.
//

extern boolean  automapactive;     // In AutoMap mode?
extern boolean  menuactive;        // Menu overlayed?
extern boolean  paused;            // Game Pause?

extern boolean  viewactive;

extern boolean  nodrawers;
extern boolean  noblit;

// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
extern int      viewangleoffset;

// Player taking events, and displaying.
#define consoleplayer   Get(DD_CONSOLEPLAYER)
#define displayplayer   Get(DD_DISPLAYPLAYER)

// Timer, for scores.
extern int      levelstarttic;     // gametic at level start
extern int      leveltime;         // tics in game play for par

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
extern thing_t deathmatchstarts[MAX_DM_STARTS];
extern thing_t *deathmatch_p;

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
#define SKYFLATNAME  "F_SKY"

extern unsigned char rndtable[256];
extern int      prndindex;

#define maketic     Get(DD_MAKETIC)
#define ticdup      1

#endif
