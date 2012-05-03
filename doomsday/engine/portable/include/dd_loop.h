/**
 * @file dd_loop.h
 * Main loop and the core timer. @ingroup base
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef __DOOMSDAY_BASELOOP_H__
#define __DOOMSDAY_BASELOOP_H__

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int rFrameCount;
extern timespan_t sysTime, gameTime, demoTime, ddMapTime;
extern boolean tickFrame;

/**
 * Register console variables for main loop.
 */
void DD_RegisterLoop(void);

/**
 * Starts the game loop.
 */
int DD_GameLoop(void);

/**
 * Called periodically while the game loop is running.
 */
void DD_GameLoopCallback(void);

/**
 * Window drawing callback.
 *
 * Drawing anything outside this routine is frowned upon.
 * Seriously frowned! (Don't do it.)
 */
void DD_GameLoopDrawer(void);

/**
 * Waits until it's time to show the drawn frame on screen. The frame must be
 * ready before this is called. Ideally the updates would appear at a fixed
 * frequency; in practice, inaccuracies due to time measurement and background
 * processes may result in varying update intervals.
 *
 * Note that if the maximum refresh rate has been set to a value higher than
 * the vsync rate, this function does nothing but update the statistisc on
 * frame timing.
 */
void DD_WaitForOptimalUpdateTime(void);

/**
 * Returns the current frame rate.
 */
float DD_GetFrameRate(void);

/**
 * Reset the core timer so that on the next frame, it seems like be that no
 * time has passed.
 */
void DD_ResetTimer(void);

/**
 * Determines whether it is time for tickers to run their 35 Hz actions.
 * Set at the beginning of a tick by DD_Ticker.
 */
boolean DD_IsSharpTick(void);

/**
 * Determines whether frame time is advancing.
 */
boolean DD_IsFrameTimeAdvancing(void);

/**
 * Returns the real time in seconds when the latest iteration of runTics() was
 * started.
 */
timespan_t DD_LatestRunTicsStartTime(void);

/**
 * Sets the exit code for the main loop. Does not cause the main loop
 * to stop; you need to call Sys_Quit() to do that.
 */
void DD_SetGameLoopExitCode(int code);

/**
 * @return Game loop exit code.
 */
int DD_GameLoopExitCode(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
