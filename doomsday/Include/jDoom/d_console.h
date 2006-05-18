/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * d_console.h: jDoom console settings and commands.
 */

#ifndef __DCONSOLE_H__
#define __DCONSOLE_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

void            G_ConsoleRegistration();
void            D_ConsoleBg(int *width, int *height);

#endif
