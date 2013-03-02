/** @file dd_loop.cpp Main loop and the core timer.
 * @ingroup base
 *
 * @authors Copyright Â© 2003-2013 Jaakko KerÃ¤nen <jaakko.keranen@iki.fi>
 * @authors Copyright Â© 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_render.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_ui.h"
#include "de_misc.h"

#include <de/App>

#ifdef __SERVER__
#  include <de/TextApp>
#endif

#ifdef __CLIENT__
#  include "ui/busyvisual.h"
#  include "ui/window.h"
#endif

/// Development utility: on sharp tics, print player 0 movement state.
//#define LIBDENG_PLAYER0_MOVEMENT_ANALYSIS

/**
 * There needs to be at least this many tics per second. A smaller value
 * is likely to cause unpredictable changes in playsim.
 */
#define MIN_TIC_RATE 35

/**
 * The length of one tic can be at most this.
 */
#define MAX_FRAME_TIME (1.0/MIN_TIC_RATE)

int maxFrameRate = 120; // Zero means 'unlimited'.
// Refresh frame count (independant of the viewport-specific frameCount).
int rFrameCount = 0;
byte devShowFrameTimeDeltas = false;
byte processSharpEventsAfterTickers = false;

timespan_t sysTime, gameTime, demoTime, ddMapTime;
//timespan_t frameStartTime;

boolean stopTime = false; // If true the time counters won't be incremented
boolean tickUI = false; // If true the UI will be tick'd
boolean tickFrame = true; // If false frame tickers won't be tick'd (unless netGame)

static int gameLoopExitCode = 0;

static double lastRunTicsTime;
static boolean firstTic = true;
static boolean tickIsSharp = false;

#define NUM_FRAMETIME_DELTAS    200
static int timeDeltas[NUM_FRAMETIME_DELTAS];
static int timeDeltasIndex = 0;

static float realFrameTimePos = 0;

void DD_RegisterLoop(void)
{
    C_VAR_BYTE("input-sharp-lateprocessing", &processSharpEventsAfterTickers, 0, 0, 1);
    C_VAR_INT ("refresh-rate-maximum",       &maxFrameRate, 0, 35, 1000);
    C_VAR_INT ("rend-dev-framecount",        &rFrameCount, CVF_NO_ARCHIVE | CVF_PROTECTED, 0, 0);
    C_VAR_BYTE("rend-info-deltas-frametime", &devShowFrameTimeDeltas, CVF_NO_ARCHIVE, 0, 1);
}

void DD_SetGameLoopExitCode(int code)
{
    gameLoopExitCode = code;
}

int DD_GameLoopExitCode(void)
{
    return gameLoopExitCode;
}

float DD_GetFrameRate(void)
{
#ifdef __CLIENT__
    return Window_CanvasWindow(Window_Main())->frameRate();
#else
    return 0;
#endif
}

#undef DD_IsSharpTick
DENG_EXTERN_C boolean DD_IsSharpTick(void)
{
    return tickIsSharp;
}

boolean DD_IsFrameTimeAdvancing(void)
{
    if(BusyMode_Active()) return false;
#ifdef __CLIENT__
    if(Con_TransitionInProgress())
    {
        return false;
    }
#endif
    return tickFrame || netGame;
}

void DD_CheckSharpTick(timespan_t time)
{
    // Sharp ticks are the ones that occur 35 per second. The rest are
    // interpolated (smoothed) somewhere in between.
    tickIsSharp = false;

    if(DD_IsFrameTimeAdvancing())
    {
        /**
         * realFrameTimePos will be reduced when new sharp world positions are
         * calculated, so that frametime always stays within the range 0..1.
         */
        realFrameTimePos += time * TICSPERSEC;

        // When one full tick has passed, it is time to do a sharp tick.
        if(realFrameTimePos >= 1)
        {
            tickIsSharp = true;
        }
    }
}

/**
 * This is the main ticker of the engine. We'll call all the other tickers
 * from here.
 *
 * @param time  Duration of the tick. This will never be longer than 1.0/TICSPERSEC.
 */
static void baseTicker(timespan_t time)
{
    if(DD_IsFrameTimeAdvancing())
    {
#ifdef __CLIENT__
        // Demo ticker. Does stuff like smoothing of view angles.
        Demo_Ticker(time);
#endif
        P_Ticker(time);
        UI2_Ticker(time);

        // InFine ticks whenever it's active.
        FI_Ticker();

        // Game logic.
        if(App_GameLoaded() && gx.Ticker)
        {
            gx.Ticker(time);
        }

#ifdef __CLIENT__
        // Windowing system ticks.
        R_Ticker(time);

        if(isClient)
        {
            Cl_Ticker(time);
        }
#elif __SERVER__
        Sv_Ticker(time);
#endif

        if(DD_IsSharpTick())
        {
            // Set frametime back by one tick (to stay in the 0..1 range).
            realFrameTimePos -= 1;

            // Camera smoothing: now that the world tic has occurred, the next sharp
            // position can be processed.
            R_NewSharpWorld();

#ifdef LIBDENG_PLAYER0_MOVEMENT_ANALYSIS
            if(ddPlayers[0].shared.inGame && ddPlayers[0].shared.mo)
            {
                mobj_t* mo = ddPlayers[0].shared.mo;
                static coord_t prevPos[3] = { 0, 0, 0 };
                static coord_t prevSpeed = 0;
                coord_t speed = V2d_Length(mo->mom);
                coord_t actualMom[2] = { mo->origin[0] - prevPos[0], mo->origin[1] - prevPos[1] };
                coord_t actualSpeed = V2d_Length(actualMom);

                Con_Message("%i,%f,%f,%f,%f", SECONDS_TO_TICKS(sysTime + time),
                            ddPlayers[0].shared.forwardMove, speed, actualSpeed, speed - prevSpeed);

                V3d_Copy(prevPos, mo->origin);
                prevSpeed = speed;
            }
#endif
        }

#ifdef __CLIENT__
        // While paused, don't modify frametime so things keep still.
        if(!clientPaused)
#endif
        {
            frameTimePos = realFrameTimePos;
        }
    }

    // Console is always ticking.
    Con_Ticker(time);

    // User interface ticks.
    if(tickUI)
    {
        UI_Ticker(time);
    }

    // Plugins tick always.
    DD_CallHooks(HOOK_TICKER, 0, &time);

    // The netcode gets to tick, too.
    Net_Ticker(time);
}

/**
 * Advance time counters.
 */
static void advanceTime(timespan_t time)
{
    int oldGameTic = 0;

    sysTime += time;

    if(!stopTime || netGame)
    {
        oldGameTic = SECONDS_TO_TICKS(gameTime);

        // The difference between gametic and demotic is that demotic
        // is not altered at any point. Gametic changes at handshakes.
        gameTime += time;
        demoTime += time;

        if(DD_IsSharpTick())
        {
            // When a new sharp tick begins, we want that the 35 Hz tick
            // calculated from gameTime also changes. If this is not the
            // case, we will adjust gameTime slightly so that it syncs again.
            if(oldGameTic == SECONDS_TO_TICKS(gameTime))
            {
                DEBUG_VERBOSE2_Message(("DD_AdvanceTime: Syncing gameTime with sharp ticks (tic=%i pos=%f)\n",
                                        oldGameTic, frameTimePos));

                // Realign.
                gameTime = (SECONDS_TO_TICKS(gameTime) + 1) / 35.f;
            }
        }

        // Leveltic is reset to zero at every map change.
        // The map time only advances when the game is not paused.
#ifdef __CLIENT__
        if(!clientPaused)
#endif
        {
            ddMapTime += time;
        }
    }
}

void DD_ResetTimer(void)
{
    firstTic = true;
    Net_ResetTimer();
}

static void timeDeltaStatistics(int deltaMs)
{
    timeDeltas[timeDeltasIndex++] = deltaMs;
    if(timeDeltasIndex == NUM_FRAMETIME_DELTAS)
    {
        timeDeltasIndex = 0;

        if(devShowFrameTimeDeltas)
        {
            int maxDelta = timeDeltas[0], minDelta = timeDeltas[0];
            float average = 0, variance = 0;
            int lateCount = 0;
            int i;
            for(i = 0; i < NUM_FRAMETIME_DELTAS; ++i)
            {
                maxDelta = MAX_OF(timeDeltas[i], maxDelta);
                minDelta = MIN_OF(timeDeltas[i], minDelta);
                average += timeDeltas[i];
                variance += timeDeltas[i] * timeDeltas[i];
                if(timeDeltas[i] > 0) lateCount++;
            }
            average /= NUM_FRAMETIME_DELTAS;
            variance /= NUM_FRAMETIME_DELTAS;
            Con_Message("Time deltas [%i frames]: min=%-6i max=%-6i avg=%-11.7f late=%5.1f%% var=%12.10f",
                        NUM_FRAMETIME_DELTAS, minDelta, maxDelta, average,
                        lateCount/(float)NUM_FRAMETIME_DELTAS*100,
                        variance);
        }
    }
}

void DD_WaitForOptimalUpdateTime(void)
{
    // All times are in milliseconds.
    static uint prevUpdateTime = 0;
    uint nowTime, elapsed = 0;
    uint targetUpdateTime;

    // optimalDelta is integer on purpose: we're measuring time at a 1 ms
    // accuracy, so we can't use fractions of a millisecond.
    const uint optimalDelta = (maxFrameRate > 0? 1000/maxFrameRate : 1);

    if(Sys_IsShuttingDown()) return; // No need for finesse.

    // This is when we would ideally like to make the update.
    targetUpdateTime = prevUpdateTime + optimalDelta;

    // Check the current time.
    nowTime = Timer_RealMilliseconds();
    elapsed = nowTime - prevUpdateTime;

    if(elapsed < optimalDelta)
    {
        uint needSleepMs = optimalDelta - elapsed;

        // We need to wait until the optimal time has passed.
        if(needSleepMs > 5)
        {
            // Longer sleep, yield to other threads.
            Sys_Sleep(needSleepMs - 3); // Leave some room for inaccuracies.
        }

        // Attempt to make sure we really wait until the optimal time.
        Sys_BlockUntilRealTime(targetUpdateTime);

        nowTime = Timer_RealMilliseconds();
        elapsed = nowTime - prevUpdateTime;
    }

    // The time for this update.
    prevUpdateTime = nowTime;

    timeDeltaStatistics((int)elapsed - (int)optimalDelta);
}

timespan_t DD_LatestRunTicsStartTime(void)
{
    if(BusyMode_Active()) return Timer_Seconds();
    return lastRunTicsTime;
}

void Loop_RunTics(void)
{
    double elapsedTime, ticLength, nowTime;

    // Do a network update first.
    N_Update();
    Net_Update();

    // Check the clock.
    if(firstTic)
    {
        // On the first tic, no time actually passes.
        firstTic = false;
        lastRunTicsTime = Timer_Seconds();
        return;
    }

    // Let's see how much time has passed. This is affected by "settics".
    nowTime = Timer_Seconds();
    elapsedTime = nowTime - lastRunTicsTime;

    // Remember when this frame started.
    lastRunTicsTime = nowTime;

    // Tic until all the elapsed time has been processed.
    while(elapsedTime > 0)
    {
        ticLength = MIN_OF(MAX_FRAME_TIME, elapsedTime);
        elapsedTime -= ticLength;

        // Will this be a sharp tick?
        DD_CheckSharpTick(ticLength);

#ifdef __CLIENT__
        // Process input events.
        DD_ProcessEvents(ticLength);
        if(!processSharpEventsAfterTickers)
        {
            // We are allowed to process sharp events before tickers.
            DD_ProcessSharpEvents(ticLength);
        }
#endif

        // Call all the tickers.
        baseTicker(ticLength);

#ifdef __CLIENT__
        if(processSharpEventsAfterTickers)
        {
            // This is done after tickers for compatibility with ye olde game logic.
            DD_ProcessSharpEvents(ticLength);
        }
#endif

        // Various global variables are used for counting time.
        advanceTime(ticLength);
    }
}
