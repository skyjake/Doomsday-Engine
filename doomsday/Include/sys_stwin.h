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
 * sys_stwin.h: Startup Window
 */

#ifndef __DOOMSDAY_STARTUP_WINDOW_H__
#define __DOOMSDAY_STARTUP_WINDOW_H__

void	SW_Init(void);
void	SW_Shutdown(void);
int		SW_IsActive(void);
void	SW_Printf(const char *format, ...);
void	SW_DrawBar(void);
void	SW_SetBarPos(int pos);
void	SW_SetBarMax(int max);

#endif 
