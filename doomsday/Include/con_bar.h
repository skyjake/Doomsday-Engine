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
 * con_bar.h: Console Progress Bar
 */

#ifndef __DOOMSDAY_CONSOLE_BAR_H__
#define __DOOMSDAY_CONSOLE_BAR_H__

#define	PBARF_INIT			1
#define PBARF_SET			2
#define PBARF_DONTSHOW		4
#define PBARF_NOBACKGROUND	8
#define PBARF_NOBLIT		16

extern int progress_enabled;

void Con_InitProgress(const char *title, int full);
void Con_HideProgress(void);
void Con_Progress(int count, int flags);

#endif 
