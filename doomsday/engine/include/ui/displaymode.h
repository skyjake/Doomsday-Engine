/**
 * @file displaymode.h
 * Changing and enumerating available display modes. @ingroup gl
 *
 * High-level logic for enumerating, selecting, and changing display modes. See
 * displaymode_native.h for the platform-specific low-level routines.
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

typedef struct displaycolortransfer_s {
    unsigned short table[3 * 256]; // 0-255:red, 256-511:green, 512-767:blue (range: 0..ffff)
} displaycolortransfer_t;

/**
 * Initializes the DisplayMode class. Enumerates all available display modes and
 * saves the current display mode. Must be called at engine startup.
 *
 * @return @c true, if display modes were initialized successfully.
 */
int DisplayMode_Init(void);

/**
 * Gets the current color transfer function and saves it as the one that will be
 * restored at shutdown.
 */
void DisplayMode_SaveOriginalColorTransfer(void);

/**
 * Shuts down the DisplayMode class. The current display mode is restored to what
 * it was at initialization time.
 */
void DisplayMode_Shutdown(void);

/**
 * Returns the display mode that was in use when DisplayMode_Init() was called.
 */
const DisplayMode* DisplayMode_OriginalMode(void);

/**
 * Returns the current display mode.
 */
const DisplayMode* DisplayMode_Current(void);

/**
 * Returns the number of available display modes.
 */
int DisplayMode_Count(void);

/**
 * Returns one of the available display modes. Use DisplayMode_Count() to
 * determine how many modes are available.
 *
 * @param index  Index of the mode, must be between 0 and DisplayMode_Count() - 1.
 */
const DisplayMode* DisplayMode_ByIndex(int index);

/**
 * Finds the closest available mode to the given criteria.
 *
 * @param width  Width in pixels.
 * @param height  Height in pixels.
 * @param depth  Color depth (bits per color component).
 * @param freq  Refresh rate. If zero, prefers rates closest to the mode at startup time.
 *
 * @return Mode that most closely matches the criteria. Always returns one of
 * the available modes; returns @c NULL only if DisplayMode_Init() has not yet
 * been called.
 */
const DisplayMode* DisplayMode_FindClosest(int width, int height, int depth, float freq);

/**
 * Determines if two display modes are equivalent.
 *
 * @param a  DisplayMode instance.
 * @param b  DisplayMode instance.
 *
 * @return  @c true or @c false.
 */
boolean DisplayMode_IsEqual(const DisplayMode* a, const DisplayMode* b);

/**
 * Changes the display mode.
 *
 * @param mode  Mode to change to. Must be one of the modes returned by the functions in displaymode.h.
 * @param shouldCapture  @c true, if the mode is intended to capture the entire display.
 *
 * @return @c true, if a mode change occurred. @c false, otherwise (bad mode or
 * when attempting to change to the current mode).
 */
int DisplayMode_Change(const DisplayMode* mode, boolean shouldCapture);

/**
 * Gets the current color transfer table.
 *
 * @param colors  Color transfer.
 */
void DisplayMode_GetColorTransfer(displaycolortransfer_t* colors);

/**
 * Sets the color transfer table.
 *
 * @param colors  Color transfer.
 */
void DisplayMode_SetColorTransfer(const displaycolortransfer_t* colors);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_DISPLAYMODE_H
