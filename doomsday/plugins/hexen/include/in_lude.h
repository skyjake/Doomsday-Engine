/** @file in_lude.h  Hexen specific intermission screens.
 *
 * @authors Copyright © 2009-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHEXEN_IN_LUDE_H
#define LIBHEXEN_IN_LUDE_H
#ifdef __cplusplus

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "h2def.h"

extern dd_bool intermission;
extern int interState;

/// To be called to register the console commands and variables of this module.
void WI_ConsoleRegister();

void IN_Init();
void IN_Stop();
void IN_Ticker();
void IN_Drawer();
void IN_SkipToNext();

#endif // __cplusplus
#endif // LIBHEXEN_IN_LUDE_H
