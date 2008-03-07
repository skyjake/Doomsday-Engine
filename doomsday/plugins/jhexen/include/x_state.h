/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/**
 * x_state.h: All the global variables that store the internal state.
 */

#ifndef __X_STATE__
#define __X_STATE__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

// We need the playr data structure as well.
#include "x_player.h"

#ifdef __GNUG__
#pragma interface
#endif

// ------------------------
// Command line parameters.
//
extern int     verbose;


// -------------------------
// Status flags for refresh.
//

extern boolean  paused;            // Game Pause?

extern boolean  viewActive;

// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
extern int      viewangleoffset;

// Timer, for scores.
extern int levelStartTic; // Game tic at level start.
extern int levelTime; // Tics in game play for par.
extern int actualLevelTime;

// Quit after playing a demo from cmdline.
extern boolean singledemo;

//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//  according to user inputs. Partly load from
//  WAD, partly set at startup time.

#define gametic     Get(DD_GAMETIC)

// Bookkeeping on players - state.
extern player_t players[MAXPLAYERS];

//-----------------------------------------
// Internal parameters, used for engine.
//


// if true, load all graphics at level load
extern boolean  precache;

extern int      bodyqueslot;

// Needed to store the number of the dummy sky flat.
// Used for rendering, as well as tracking projectiles etc.
#define SKYMASKMATERIAL     Get(DD_SKYMASKMATERIAL_NUM)
#define SKYFLATNAME         "F_SKY"

#endif
