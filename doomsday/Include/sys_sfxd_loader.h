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
 * sys_sfxd_loader.h: Sound Driver DLL Loader
 */

#ifndef __DOOMSDAY_SYSTEM_SFX_LOADER_H__
#define __DOOMSDAY_SYSTEM_SFX_LOADER_H__

#include "sys_sfxd.h"

sfxdriver_t* DS_Load(const char *name);

#endif 
