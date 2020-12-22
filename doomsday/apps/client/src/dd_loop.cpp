/** @file dd_loop.cpp  Main loop and the core timer.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "dd_loop.h"

#include <de/legacy/timer.h>
#include <de/app.h>
#include <de/logbuffer.h>
#ifdef __SERVER__
#  include <de/textapp.h>
#endif
#include <doomsday/doomsdayapp.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>

#include "network/net_event.h"
#include "sys_system.h"
#include "world/p_ticker.h"

#ifdef __SERVER__
#  include "server/sv_def.h"
#endif

#ifdef __CLIENT__
#  include "client/cl_def.h"
#  include "clientapp.h"
#  include "network/net_demo.h"
#  include "render/rend_font.h"
#  include "render/viewports.h"
#  include "ui/busyvisual.h"
#  include "ui/clientwindow.h"
#  include "ui/inputsystem.h"
#endif

using namespace de;

// Development utility: on sharp tics, print player 0 movement state.
//#define DE_PLAYER0_MOVEMENT_ANALYSIS

/**
 * There needs to be at least this many tics per second. A smaller value
 * is likely to cause unpredictable changes in playsim.
 */
#define MIN_TIC_RATE 35

/**
 * The length of one tic can be at most this.
 */
#define MAX_FRAME_TIME (1.0/MIN_TIC_RATE)

/**
 * If the loop is stuck for more than this number of seconds, the elapsed time is
 * ignored. The assumption is that the app was suspended or was not able to run,
 * so no point in running tics.
 */
#define MAX_ELAPSED_TIME 5

dfloat frameTimePos;  ///< 0...1: fractional part for sharp game tics.

// Refresh frame count (independant of the viewport-specific frameCount).
dint rFrameCount;
byte devShowFrameTimeDeltas;
byte processSharpEventsAfterTickers = true;

timespan_t sysTime, gameTime, demoTime;
//timespan_t frameStartTime;

//dd_bool stopTime;          ///< @c true if the time counters won't be incremented
//dd_bool tickUI;            ///< @c true if the UI will be tick'd
dd_bool tickFrame = true;  ///< @c false if frame tickers won't be tick'd (unless netGame)

static dint gameLoopExitCode;

static ddouble lastRunTicsTime;
static dd_bool firstTic = true;
static dd_bool tickIsSharp;

#define NUM_FRAMETIME_DELTAS    200
static dint timeDeltas[NUM_FRAMETIME_DELTAS];
static dint timeDeltasIndex;

static dfloat realFrameTimePos;

void DD_SetGameLoopExitCode(dint code)
{
    ::gameLoopExitCode = code;
}

dint DD_GameLoopExitCode()
{
    return ::gameLoopExitCode;
}

dfloat DD_GetFrameRate()
{
#ifdef __CLIENT__
    return ClientWindow::main().frameRate();
#else
    return 0;
#endif
}

#undef DD_IsSharpTick
DE_EXTERN_C dd_bool DD_IsSharpTick()
{
    return ::tickIsSharp;
}

dd_bool DD_IsFrameTimeAdvancing()
{
    if(BusyMode_Active()) return false;
    return ::tickFrame || netState.netGame;
}

static void checkSharpTick(timespan_t time)
{
    // Sharp ticks are the ones that occur 35 per second. The rest are
    // interpolated (smoothed) somewhere in between.
    ::tickIsSharp = false;

    if(DD_IsFrameTimeAdvancing())
    {
        // @var realFrameTimePos will be reduced when new sharp world positions are
        // calculated, so that @var frameTime always stays within the range 0..1.
        ::realFrameTimePos += time * TICSPERSEC;

        // When one full tick has passed, it is time to do a sharp tick.
        if(::realFrameTimePos >= 1)
        {
            ::tickIsSharp = true;
        }
    }
}

/**
 * This is the main ticker of the engine. We'll call all the other tickers from here.
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
#ifdef __CLIENT__
        FR_Ticker(time);
#endif

        // InFine ticks whenever it's active.
        App_InFineSystem().runTicks(time);

        // Game logic.
        if(App_GameLoaded() && gx.Ticker)
        {
            gx.Ticker(time);
        }

#ifdef __CLIENT__
        // Windowing system ticks.
        for(dint i = 0; i < DDMAXPLAYERS; ++i)
        {
            R_ViewWindowTicker(i, time);
        }

        if(netState.isClient)
        {
            Cl_Ticker(time);
        }
#elif __SERVER__
        Sv_Ticker(time);
#endif

        if(DD_IsSharpTick())
        {
            // Set frametime back by one tick (to stay in the 0..1 range).
            ::realFrameTimePos -= 1;

#ifdef __CLIENT__
            // Camera smoothing: now that the world tic has occurred, the next sharp
            // position can be processed.
            R_NewSharpWorld();
#endif

#ifdef DE_PLAYER0_MOVEMENT_ANALYSIS
            if(DD_Player(0)->publicData().inGame && DD_Player(0)->publicData().mo)
            {
                static coord_t prevPos[3] = { 0, 0, 0 };
                static coord_t prevSpeed = 0;

                mobj_t *mob = DD_Player(0)->publicData().mo;

                coord_t speed        = V2d_Length(mob->mom);
                coord_t actualMom[2] = { mob->origin[0] - prevPos[0], mob->origin[1] - prevPos[1] };
                coord_t actualSpeed  = V2d_Length(actualMom);

                LOG_NOTE("%i,%f,%f,%f,%f")
                        << SECONDS_TO_TICKS(sysTime + time)
                    << DD_Player(0)->publicData().forwardMove
                        << speed
                        << actualSpeed
                        << speed - prevSpeed;

                V3d_Copy(prevPos, mob->origin);
                prevSpeed = speed;
            }
#endif
        }

#ifdef __CLIENT__
        // While paused, don't modify frametime so things keep still.
        if(!::clientPaused)
#endif
        {
            ::frameTimePos = ::realFrameTimePos;
        }
    }

    // Console is always ticking.
    Con_Ticker(time);
    if(::tickFrame)
    {
        Con_TransitionTicker(time);
    }

    // Plugins tick always.
    DoomsdayApp::plugins().callAllHooks(HOOK_TICKER, 0, &time);

    // The netcode gets to tick, too.
    Net_Ticker();
}

/**
 * Advance time counters.
 */
static void advanceTime(timespan_t delta)
{
    ::sysTime += delta;

    const dint oldGameTic = SECONDS_TO_TICKS(::gameTime);

    // The difference between gametic and demotic is that demotic
    // is not altered at any point. Gametic changes at handshakes.
    ::gameTime += delta;
    ::demoTime += delta;

    if(DD_IsSharpTick())
    {
        // When a new sharp tick begins, we want that the 35 Hz tick
        // calculated from gameTime also changes. If this is not the
        // case, we will adjust gameTime slightly so that it syncs again.
        if(oldGameTic == SECONDS_TO_TICKS(::gameTime))
        {
            LOGDEV_XVERBOSE("Syncing gameTime with sharp ticks (tic=%i pos=%f)",
                            oldGameTic << ::frameTimePos);

            // Realign.
            ::gameTime = (SECONDS_TO_TICKS(::gameTime) + 1) / 35.f;
        }
    }

    // World time always advances unless a local game is paused on client-side.
    world::World::get().advanceTime(delta);
}

void DD_ResetTimer()
{
    ::firstTic = true;
    Net_ResetTimer();
}

static void timeDeltaStatistics(dint deltaMs)
{
    ::timeDeltas[::timeDeltasIndex++] = deltaMs;
    if(::timeDeltasIndex == NUM_FRAMETIME_DELTAS)
    {
        ::timeDeltasIndex = 0;

        if(::devShowFrameTimeDeltas)
        {
            dint maxDelta = timeDeltas[0], minDelta = timeDeltas[0];
            dfloat average = 0, variance = 0;
            dint lateCount = 0;
            for(dint i = 0; i < NUM_FRAMETIME_DELTAS; ++i)
            {
                maxDelta = de::max(timeDeltas[i], maxDelta);
                minDelta = de::min(timeDeltas[i], minDelta);
                average += timeDeltas[i];
                variance += timeDeltas[i] * timeDeltas[i];
                if(timeDeltas[i] > 0) lateCount++;
            }
            average  /= NUM_FRAMETIME_DELTAS;
            variance /= NUM_FRAMETIME_DELTAS;

            LOGDEV_MSG("Time deltas [%i frames]: min=%-6i max=%-6i avg=%-11.7f late=%5.1f%% var=%12.10f")
                    << NUM_FRAMETIME_DELTAS << minDelta << maxDelta << average
                << lateCount / dfloat( NUM_FRAMETIME_DELTAS * 100 ) << variance;
        }
    }
}

/// @note All times are in milliseconds.
void DD_WaitForOptimalUpdateTime()
{
    static duint prevUpdateTime = 0;

    const int maxFrameRate = Config::get().geti("window.main.maxFps");

    /// @var optimalDelta is integer on purpose: we're measuring time at a 1 ms accuracy,
    /// so we can't use fractions of a millisecond.
    const duint optimalDelta = duint(maxFrameRate > 0 ? 1000 / maxFrameRate : 1);

    if (Sys_IsShuttingDown()) return; // No need for finesse.

    // This is when we would ideally like to make the update.
    const duint targetUpdateTime = prevUpdateTime + optimalDelta;

    // Check the current time.
    duint nowTime = Timer_RealMilliseconds();
    duint elapsed = nowTime - prevUpdateTime;

    if (elapsed < optimalDelta)
    {
        const duint needSleepMs = optimalDelta - elapsed;

        // We need to wait until the optimal time has passed.
        if (needSleepMs > 5)
        {
            // Longer sleep, yield to other threads.
            Sys_Sleep(needSleepMs - 3);  // Leave some room for inaccuracies.
        }

        // Attempt to make sure we really wait until the optimal time.
        Sys_BlockUntilRealTime(targetUpdateTime);

        nowTime = Timer_RealMilliseconds();
        elapsed = nowTime - prevUpdateTime;
    }

    // The time for this update.
    prevUpdateTime = nowTime;

    timeDeltaStatistics(dint(elapsed) - dint(optimalDelta));
}

timespan_t DD_LatestRunTicsStartTime()
{
    if(BusyMode_Active()) return Timer_Seconds();
    return lastRunTicsTime;
}

static ddouble ticLength;

timespan_t DD_CurrentTickDuration()
{
    return ::ticLength;
}

void Loop_RunTics()
{
    // Do a network update first.
    Net_Update();

    // Check the clock.
    if(::firstTic)
    {
        // On the first tic, no time actually passes.
        ::firstTic = false;
        ::lastRunTicsTime = Timer_Seconds();
        return;
    }

    // Let's see how much time has passed. This is affected by "settics".
    const ddouble nowTime = Timer_Seconds();

    ddouble elapsedTime = nowTime - ::lastRunTicsTime;
    if(elapsedTime > MAX_ELAPSED_TIME)
    {
        // It was too long ago, no point in running individual ticks. Just do one.
        elapsedTime = MAX_FRAME_TIME;
    }

    // Remember when this frame started.
    ::lastRunTicsTime = nowTime;

    // Tic until all the elapsed time has been processed.
    while(elapsedTime > 0)
    {
        ::ticLength = de::min(MAX_FRAME_TIME, elapsedTime);
        elapsedTime -= ::ticLength;

        // Will this be a sharp tick?
        checkSharpTick(::ticLength);

#ifdef __CLIENT__
        // Process input events.
        ClientApp::input().processEvents(::ticLength);
        if(!::processSharpEventsAfterTickers)
        {
            // We are allowed to process sharp events before tickers.
            ClientApp::input().processSharpEvents(::ticLength);
        }
#endif

        // Call all the tickers.
        baseTicker(::ticLength);

#ifdef __CLIENT__
        if(::processSharpEventsAfterTickers)
        {
            // This is done after tickers for compatibility with ye olde game logic.
            ClientApp::input().processSharpEvents(::ticLength);
        }
#endif

        // Various global variables are used for counting time.
        advanceTime(::ticLength);
    }
}

void DD_RegisterLoop()
{
    C_VAR_BYTE("input-sharp-lateprocessing", &::processSharpEventsAfterTickers, 0, 0, 1);
    C_VAR_INT ("rend-dev-framecount",        &::rFrameCount, CVF_NO_ARCHIVE | CVF_PROTECTED, 0, 0);
    C_VAR_BYTE("rend-info-deltas-frametime", &::devShowFrameTimeDeltas, CVF_NO_ARCHIVE, 0, 1);
}
