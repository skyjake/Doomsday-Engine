// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//   Duh.
// 
//-----------------------------------------------------------------------------

#ifndef __G_GAME__
#define __G_GAME__

#include "doomdef.h"
#include "d_event.h"
#include "d_player.h"

//
// GAME
//
void            G_DeathMatchSpawnPlayer(int playernum);

boolean         G_ValidateMap(int *episode, int *map);

void            G_InitNew(skill_t skill, int episode, int map);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void            G_DeferedInitNew(skill_t skill, int episode, int map);

void            G_DeferedPlayDemo(char *demo);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel or W_EnterWorld.
void            G_LoadGame(char *name);

void            G_DoLoadGame(void);

// Called by M_Responder.
void            G_SaveGame(int slot, char *description);

// Only called by startup code.
void            G_RecordDemo(char *name);

void            G_BeginRecording(void);

void            G_PlayDemo(char *name);
void            G_TimeDemo(char *name);
void            G_StopDemo(void);
void            G_DemoEnds(void);
void            G_DemoAborted(void);

void            G_DoReborn(int playernum);
void            G_ExitLevel(void);
void            G_SecretExitLevel(void);

void            G_WorldDone(void);

void            G_Ticker(void);
boolean         G_Responder(event_t *ev);

void            G_ScreenShot(void);

void            G_PrepareWIData(void);

void            G_QueueBody(mobj_t *body);

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.7  2004/05/30 08:42:35  skyjake
// Tweaked indentation style
//
// Revision 1.6  2004/05/29 18:19:58  skyjake
// Refined indentation style
//
// Revision 1.5  2004/05/29 09:53:11  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.4  2004/05/28 17:16:35  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.2.2.2  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.2.2.1.2.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.2.2.1  2003/09/06 21:09:38  skyjake
// Improved player spawning with more intelligent spot selection
//
// Revision 1.2  2003/07/12 22:10:51  skyjake
// Added map validation routine
//
// Revision 1.1  2003/02/26 19:18:27  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:12  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------
