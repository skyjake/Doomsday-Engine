/** @file progress.h  Simple wrapper for the Busy progress bar.
 * @ingroup ui
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2007-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CONSOLE_PROGRESSBAR_H
#define DE_CONSOLE_PROGRESSBAR_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void Con_InitProgress(int maxProgress);

/**
 * Initialize progress that only covers a part of the progress bar.
 *
 * @param maxProgress  Maximum logical progress, for Con_SetProgress().
 * @param start        Start of normalized progress (default: 0).
 * @param end          End of normalized progress (default: 1).
 */
void Con_InitProgress2(int maxProgress, float start, float end);

/**
 * Updates the progress indicator.
 */
void Con_SetProgress(int progress);

dd_bool Con_IsProgressAnimationCompleted(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_CONSOLE_PROGRESSBAR_H
