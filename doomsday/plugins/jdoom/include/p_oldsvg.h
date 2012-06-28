/**
 * @file p_oldsvg.h
 * Doom ver 1.9 save game reader.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 1993-1996 by id Software, Inc.
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDOOM_OLD_SAVESTATE
#define LIBDOOM_OLD_SAVESTATE

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "p_saveio.h"

boolean SV_v19_Recognise(SaveInfo* saveInfo);

/**
 * @return  @c 0 on success else error code.
 */
int SV_v19_LoadGame(SaveInfo* saveInfo);

#endif /// LIBDOOM_OLD_SAVESTATE
