/**
 * @file displaymode.h
 * Changing and enumerating available display modes. @ingroup gl
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_DISPLAYMODE_H
#define LIBDENG_DISPLAYMODE_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct displaymode_s {
    int width;
    int height;
    float refreshRate; // might be zero
    int depth;

    // Calculated automatically:
    int ratioX;
    int ratioY;
} DisplayMode;

/**
 * Initializes the DisplayMode class. Enumerates all available display modes and
 * saves the current display mode.
 */
int DisplayMode_Init(void);

/**
 * Shuts down the DisplayMode class. The current display mode is restored to what
 * it was at initialization time.
 */
void DisplayMode_Shutdown(void);

const DisplayMode* DisplayMode_OriginalMode(void);

const DisplayMode* DisplayMode_Current(void);

int DisplayMode_Count(void);

const DisplayMode* DisplayMode_ByIndex(int index);

const DisplayMode* DisplayMode_FindClosest(int width, int height, int depth, float freq);

boolean DisplayMode_IsEqual(const DisplayMode* a, const DisplayMode* b);

int DisplayMode_Change(const DisplayMode* mode, boolean shouldCapture);

#ifdef __cplusplus
}
#endif

#endif // LIBDENG_DISPLAYMODE_H
