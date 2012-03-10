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

extern int rFrameCount;
extern timespan_t sysTime, gameTime, demoTime, ddMapTime;
extern boolean tickFrame;

/**
 * Register console variables for main loop.
 */
void DD_RegisterLoop(void);

/**
 * This is the refresh thread (the main thread).
 */
int DD_GameLoop(void);

/**
 * Waits until it's time to show the drawn frame on screen. The frame must be
 * ready before this is called. Ideally the updates would appear at a fixed
 * frequency; in practice, inaccuracies due to time measurement and background
 * processes may result in varying update intervals.
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
 * Sets the exit code for the main loop. Does not cause the main loop
 * to stop; you need to call Sys_Quit() to do that.
 */
void DD_SetGameLoopExitCode(int code);

#endif
