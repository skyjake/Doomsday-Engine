/** @file dd_loop.h Main loop and the core timer.
 * @ingroup base
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

DE_EXTERN_C float frameTimePos;      // 0...1: fractional part for sharp game tics
DE_EXTERN_C int rFrameCount;
DE_EXTERN_C timespan_t sysTime, gameTime, demoTime;
DE_EXTERN_C dd_bool tickFrame;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register console variables for main loop.
 */
void DD_RegisterLoop(void);

/**
 * Runs one or more tics depending on how much time has passed since the
 * previous call to this function. This gets called once per each main loop
 * iteration. Finishes as quickly as possible.
 */
void Loop_RunTics(void);

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
 * Determines whether frame time is advancing.
 */
dd_bool DD_IsFrameTimeAdvancing(void);

/**
 * Returns the real time in seconds when the latest iteration of runTics() was
 * started.
 */
timespan_t DD_LatestRunTicsStartTime(void);

/**
 * Returns how much time has elapsed during the current tick.
 */
timespan_t DD_CurrentTickDuration(void);

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
