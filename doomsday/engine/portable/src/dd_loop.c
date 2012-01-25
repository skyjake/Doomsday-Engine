/**\file dd_loop.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Main Loop.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_ui.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

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

/**
 * Maximum number of milliseconds spent uploading textures at the beginning
 * of a frame. Note that non-uploaded textures will appear as pure white
 * until their content gets uploaded (you should precache them).
 */
#define FRAME_DEFERRED_UPLOAD_TIMEOUT 20

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void            Net_ResetTimer(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void            DD_RunTics(void);
void            DD_GameLoopCallback(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean appShutdown = false; // Set to true when we should exit (normally).
#ifdef WIN32
boolean suspendMsgPump = false; // Set to true to disable checking windows msgs.
#endif

int maxFrameRate = 200; // Zero means 'unlimited'.
// Refresh frame count (independant of the viewport-specific frameCount).
int rFrameCount = 0;

timespan_t sysTime, gameTime, demoTime, ddMapTime;
timespan_t frameStartTime;

boolean stopTime = false; // If true the time counters won't be incremented
boolean tickUI = false; // If true the UI will be tick'd
boolean tickFrame = true; // If false frame tickers won't be tick'd (unless netGame)

boolean drawGame = true; // If false the game viewport won't be rendered

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static double lastFrameTime;
static float fps;
static int lastFrameCount;
static boolean firstTic = true;
static boolean tickIsSharp = false;

// CODE --------------------------------------------------------------------

/**
 * Register console variables for main loop.
 */
void DD_RegisterLoop(void)
{
    C_VAR_INT("refresh-rate-maximum", &maxFrameRate, 0, 35, 1000);
    C_VAR_INT("rend-dev-framecount", &rFrameCount,
              CVF_NO_ARCHIVE | CVF_PROTECTED, 0, 0);
}

/**
 * This is the refresh thread (the main thread).
 */
int DD_GameLoop(void)
{
    // Limit the frame rate to 35 when running in dedicated mode.
    if(isDedicated)
    {
        maxFrameRate = 35;
    }

    // Start the deng2 event loop.
    return LegacyCore_RunEventLoop(de2LegacyCore, DD_GameLoopCallback);
}

/**
 * This gets called periodically from the deng2 application core.
 */
void DD_GameLoopCallback(void)
{
    int exitCode = 0;
#ifdef WIN32
    MSG msg;
#endif

    if(appShutdown)
    {
        // Time to stop the loop.
        LegacyCore_Stop(de2LegacyCore, exitCode);
        return;
    }

#ifdef WIN32
    /**
     * Start by checking Windows messages.
     * \note Must be in the same thread as that which registered the
     *       window it is handling messages for - DJS.
     */
    while(!suspendMsgPump &&
          PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0)
    {
        if(msg.message == WM_QUIT)
        {
            appShutdown = true;
            suspendMsgPump = true;
            exitCode = msg.wParam;
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if(appShutdown)
    {
        LegacyCore_Stop(de2LegacyCore, exitCode);
        return;
    }
#endif

    // Frame syncronous I/O operations.
    DD_StartFrame();

    // Run at least one tic. If no tics are available (maxfps interval
    // not reached yet), the function blocks.
    DD_RunTics();

    // Update clients.
    Sv_TransmitFrame();

    // Finish the refresh frame.
    DD_EndFrame();

    // Draw and show the current frame.
    DD_DrawAndBlit();

    // After the first frame, start timedemo.
    DD_CheckTimeDemo();
}

/**
 * Drawing anything outside this routine is frowned upon.
 * Seriously frowned!
 */
void DD_DrawAndBlit(void)
{
    if(novideo)
        return;

    if(Con_IsBusy())
    {
        Con_Error("DD_DrawAndBlit: Console is busy, can't draw!\n");
    }

    if(renderWireframe)
    {
        // When rendering is wireframe mode, we must clear the screen
        // before rendering a frame.
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if(drawGame)
    {
        if(DD_GameLoaded())
        {
            // Interpolate the world ready for drawing view(s) of it.
            R_BeginWorldFrame();
            R_RenderViewPorts();
        }
        else if(titleFinale == 0)
        {
            // Title finale is not playing. Lets do it manually.
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, SCREENWIDTH, SCREENHEIGHT, 0, -1, 1);

            R_RenderBlankView();

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
        }

        if(!(UI_IsActive() && UI_Alpha() >= 1.0))
        {
            UI2_Drawer();

            // Draw any full window game graphics.
            if(DD_GameLoaded() && gx.DrawWindow)
                gx.DrawWindow(&theWindow->geometry.size);
        }
    }

    if(Con_TransitionInProgress())
        Con_DrawTransition();

    if(drawGame)
    {
        // Debug information.
        Net_Drawer();
        S_Drawer();

        // Finish up any tasks that must be completed after view(s) have
        // been drawn.
        R_EndWorldFrame();
    }

    if(UI_IsActive())
    {
        // Draw user interface.
        UI_Drawer();
    }

    // Draw console.
    Rend_Console();

    // End any open DGL sequence.
    DGL_End();

    // Flush buffered stuff to screen (blits everything).
    GL_DoUpdate();
}

void DD_StartFrame(void)
{
    if(!isDedicated)
        GL_ProcessDeferredTasks(FRAME_DEFERRED_UPLOAD_TIMEOUT);

    frameStartTime = Sys_GetTimef();

    S_StartFrame();
    if(gx.BeginFrame)
    {
        gx.BeginFrame();
    }
}

void DD_EndFrame(void)
{
    static uint         lastFpsTime = 0;

    uint                nowTime = Sys_GetRealTime();

    // Increment the (local) frame counter.
    rFrameCount++;

    // Count the frames every other second.
    if(nowTime - 2000 >= lastFpsTime)
    {
        fps = (rFrameCount - lastFrameCount) /
            ((nowTime - lastFpsTime)/1000.0f);
        lastFpsTime = nowTime;
        lastFrameCount = rFrameCount;
    }

    if(gx.EndFrame)
        gx.EndFrame();

    S_EndFrame();
}

float DD_GetFrameRate(void)
{
    return fps;
}

/**
 * Determines whether it is time for tickers to run their 35 Hz actions.
 * Set at the beginning of a tick by DD_Ticker.
 */
boolean DD_IsSharpTick(void)
{
    return tickIsSharp;
}

/**
 * This is the main ticker of the engine. We'll call all the other tickers
 * from here.
 *
 * @param time  Duration of the tick. This will never be longer than 1.0/TICSPERSEC.
 */
void DD_Ticker(timespan_t time)
{
    static float realFrameTimePos = 0;

    // Sharp ticks are the ones that occur 35 per second. The rest are interpolated
    // (smoothed) somewhere in between.
    tickIsSharp = false;

    if(!Con_TransitionInProgress() && (tickFrame || netGame)) // Advance frametime?
    {
        /**
         * realFrameTimePos will be reduced when new sharp world positions are calculated,
         * so that frametime always stays within the range 0..1.
         */
        realFrameTimePos += time * TICSPERSEC;

        // When one full tick has passed, it is time to do a sharp tick.
        if(realFrameTimePos >= 1)
        {
            tickIsSharp = true;
        }

        // Demo ticker. Does stuff like smoothing of view angles.
        Demo_Ticker(time);
        P_Ticker(time);
        UI2_Ticker(time);

        // InFine ticks whenever it's active.
        FI_Ticker();

        // Game logic.
        if(DD_GameLoaded() && gx.Ticker)
        {
            gx.Ticker(time);
        }

        // Windowing system ticks.
        R_Ticker(time);

        if(isClient)
            Cl_Ticker(time);
        else
            Sv_Ticker(time);

        if(DD_IsSharpTick())
        {
            // A new 35 Hz tick begins.
            // Set frametime back by one tick (to stay in the 0..1 range).
            realFrameTimePos -= 1;
            //assert(realFrameTimePos < 1);

            // Camera smoothing: now that the world tic has occurred, the next sharp
            // position can be processed.
            R_NewSharpWorld();

#ifdef LIBDENG_PLAYER0_MOVEMENT_ANALYSIS
            if(ddPlayers[0].shared.inGame && ddPlayers[0].shared.mo)
            {
                mobj_t* mo = ddPlayers[0].shared.mo;
                static float prevPos[3] = { 0, 0, 0 };
                static float prevSpeed = 0;
                float speed = V2_Length(mo->mom);
                float actualMom[2] = { mo->pos[0] - prevPos[0], mo->pos[1] - prevPos[1] };
                float actualSpeed = V2_Length(actualMom);

                Con_Message("%i,%f,%f,%f,%f\n", SECONDS_TO_TICKS(sysTime + time),
                            ddPlayers[0].shared.forwardMove, speed, actualSpeed, speed - prevSpeed);

                V3_Copy(prevPos, mo->pos);
                prevSpeed = speed;
            }
#endif
        }

        // While paused, don't modify frametime so things keep still.
        if(!clientPaused)
            frameTimePos = realFrameTimePos;
    }

    // Console is always ticking.
    Con_Ticker(time);

    if(tickUI)
    {   // User interface ticks.
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
void DD_AdvanceTime(timespan_t time)
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
#ifdef _DEBUG
                VERBOSE2( Con_Message("DD_AdvanceTime: Syncing gameTime with sharp ticks (tic=%i pos=%f)\n",
                                      oldGameTic, frameTimePos) );
#endif
                // Realign.
                gameTime = (SECONDS_TO_TICKS(gameTime) + 1) / 35.f;
            }
        }

        // Leveltic is reset to zero at every map change.
        // The map time only advances when the game is not paused.
        if(!clientPaused)
        {
            ddMapTime += time;
        }
    }
}

/**
 * Reset the game time so that on the next frame, the effect will be
 * that no time has passed.
 */
void DD_ResetTimer(void)
{
    firstTic = true;
    Net_ResetTimer();
}

/**
 * Run at least one tic.
 */
void DD_RunTics(void)
{
    double              frameTime, ticLength;
    double              nowTime = Sys_GetSeconds();

    // Do a network update first.
    N_Update();
    Net_Update();

    // Check the clock.
    if(firstTic)
    {
        // On the first tic, no time actually passes.
        firstTic = false;
        lastFrameTime = nowTime;
        return;
    }

    // We'll sleep until we go past the maxfps interval (the shortest
    // allowed interval between tics).
    if(maxFrameRate > 0)
    {
        while((nowTime = Sys_GetSeconds()) - lastFrameTime < 1.0 / maxFrameRate)
        {
            // Wait for a short while.
            Sys_Sleep(3);

            N_Update();
            Net_Update();
        }
    }

    // How much time do we have for this frame?
    frameTime = nowTime - lastFrameTime;
    lastFrameTime = nowTime;

    // Tic length is determined by the minfps rate.
    while(frameTime > 0)
    {
        ticLength = MIN_OF(MAX_FRAME_TIME, frameTime);
        frameTime -= ticLength;

        // Process input events.
        DD_ProcessEvents(ticLength);

        // Call all the tickers.
        DD_Ticker(ticLength);

        // Various global variables are used for counting time.
        DD_AdvanceTime(ticLength);
    }
}
