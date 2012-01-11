/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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
 * con_bar.h: Console Progress Bar
 */

#ifndef __DOOMSDAY_CONSOLE_BAR_H__
#define __DOOMSDAY_CONSOLE_BAR_H__

#include "dd_types.h"

void Con_InitProgress(int maxProgress);

/**
 * Initialize progress that only covers a part of the progress bar.
 *
 * @param maxProgress  Maximum logical progress, for Con_SetProgress().
 * @param start        Start of normalized progress (default: 0).
 * @param end          End of normalized progress (default: 1).
 */
void Con_InitProgress2(int maxProgress, float start, float end);

void Con_SetProgress(int progress);
float Con_GetProgress(void);
boolean Con_IsProgressAnimationCompleted(void);

#endif
