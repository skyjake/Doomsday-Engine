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
 * dd_pinit.h: Portable Engine Initialization
 */

#ifndef __DOOMSDAY_PORTABLE_INIT_H__
#define __DOOMSDAY_PORTABLE_INIT_H__

// Maximum allowed number of plugins.
#define MAX_PLUGS	32

void	DD_ShutdownAll (void);
int 	DD_CheckArg (char *tag, char **value);
void 	DD_ErrorBox (boolean error, char *format, ...);
void 	DD_MainWindowTitle (char *title);
void 	DD_InitAPI (void);
void 	DD_InitCommandLine (const char *cmdLine);

extern game_import_t gi;		
extern game_export_t gx;

#endif // __DOOMSDAY_PORTABLE_INIT_H__
