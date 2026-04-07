/** @file gl_special.h  Special color effects for the game view.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCOMMON_REFRESH_SPECIAL_H
#define LIBCOMMON_REFRESH_SPECIAL_H

#include <de/libcore.h>

DE_EXTERN_C void R_SpecialFilterRegister(void);

DE_EXTERN_C void R_InitSpecialFilter(void);

/**
 * Draws a special filter over the screen (e.g. the Doom inversing filter used
 * when in god mode).
 */
DE_EXTERN_C void R_UpdateSpecialFilter(int player);

DE_EXTERN_C void R_UpdateSpecialFilterWithTimeDelta(int player, float delta);

#endif // LIBCOMMON_REFRESH_SPECIAL_H
