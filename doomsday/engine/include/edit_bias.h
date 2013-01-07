/**
 * @file edit_bias.h
 * Bias light source editor. @ingroup base
 *
 * @authors Copyright &copy; 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_BIAS_EDITOR
#define LIBDENG_BIAS_EDITOR

#include "edit_bias.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register all console commands and variables for this module.
 */
void SBE_Register(void);

void SBE_EndFrame(void);

void SBE_DrawCursor(void);

void SBE_DrawHUD(void);

/**
 * @return  @c true iff the console player is currently using the HueCircle.
 */
boolean SBE_UsingHueCircle(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_BIAS_EDITOR
