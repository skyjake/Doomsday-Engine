/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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
 * in_lude.h: Intermission/stat screens
 */

#ifndef __IN_LUDE_H__
#define __IN_LUDE_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "h_player.h"

extern boolean intermission;
extern int interState;
extern int interTime;

/// To be called to register the console commands and variables of this module.
void WI_Register(void);

void            IN_Init(wbstartstruct_t* wbstartstruct);
void            IN_SkipToNext(void);
void            IN_Stop(void);

void            IN_Ticker(void);
void            IN_Drawer(void);

void            IN_WaitStop(void);
void            IN_LoadPics(void);
void            IN_UnloadPics(void);
void            IN_CheckForSkip(void);
void            IN_InitStats(void);
void            IN_InitDeathmatchStats(void);
void            IN_InitNetgameStats(void);

#endif
