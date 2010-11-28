/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * dd_loop.c: Main Loop
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

trigger_t sharedFixedTrigger = { 1 / 35.0, 0 };

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static double lastFrameTime;

static float fps;
static int lastFrameCount;
static boolean firstTic = true;

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
    int                 exitCode = 0;
#ifdef WIN32
    MSG                 msg;
#endif

    // Limit the frame rate to 35 when running in dedicated mode.
    if(isDedicated)
    {
        maxFrameRate = 35;
    }

    while(!appShutdown)
    {
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
            continue;
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

        // Send out new accumulation. Drawing will take the longest.
        //Net_Update();
        DD_DrawAndBlit();
        //Net_Update();

        // After the first frame, start timedemo.
        DD_CheckTimeDemo();
    }

    return exitCode;
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
        if(!DD_IsNullGameInfo(DD_GameInfo()))
        {   // Interpolate the world ready for drawing view(s) of it.
            R_BeginWorldFrame();
            R_RenderViewPorts();
        }
        else if(titleFinale == 0)
        {   // No loaded title finale. Lets do it manually.
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

            // Draw any over/outside view window game graphics (e.g. fullscreen menus and other displays).
            if(!DD_IsNullGameInfo(DD_GameInfo()) && gx.G_Drawer2)
                gx.G_Drawer2();
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
        GL_UploadDeferredContent(FRAME_DEFERRED_UPLOAD_TIMEOUT);

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
 * This is the main ticker of the engine. We'll call all the other tickers
 * from here.
 */
void DD_Ticker(timespan_t time)
{
    static float realFrameTimePos = 0;

    if(!Con_TransitionInProgress())
    {
        // Demo ticker. Does stuff like smoothing of view angles.
        Net_BuildLocalCommands(time);
        Demo_Ticker(time);
        P_Ticker(time);

        if(tickFrame || netGame)
        {
            /**
             * It will be reduced when new sharp world positions are calculated,
             * so that frametime always stays within the range 0..1.
             */
            realFrameTimePos += time * TICSPERSEC;

            UI2_Ticker(time);

            // InFine ticks whenever it's active.
            FI_Ticker(time);

            // Game logic.
            if(!DD_IsNullGameInfo(DD_GameInfo()) && gx.Ticker)
            {
                gx.Ticker(time);
            }

            // Advance global fixed time (35Hz).
            if(M_RunTrigger(&sharedFixedTrigger, time))
            {   // A new 35 Hz tick has begun.
                // Clear the player fixangles flags which have been in effect
                // for any fractional ticks since they were set.
                //Sv_FixLocalAngles(true /* just clear flags; don't apply */);

                /**
                 * Server ticks.
                 *
                 * These are placed here because they still rely on fixed ticks
                 * and thus it's best to keep them in sync with the fixed game
                 * ticks.
                 */
                if(isClient)
                    Cl_Ticker( /* time */ );
                else
                    Sv_Ticker( /* time */ );

                // This is needed by camera smoothing.  It needs to know
                // when the world tic has occured so the next sharp
                // position can be processed.

                // Frametime will be set back by one tick.
                realFrameTimePos -= 1;

                // We can't sent FixAngles messages to ourselves, so it's
                // done here.
                //Sv_FixLocalAngles(false /* apply only; don't clear flag */);

                R_NewSharpWorld();
            }

            // While paused, don't modify frametime so things keep still.
            if(!clientPaused)
                frameTimePos = realFrameTimePos;

            // We can't sent FixAngles messages to ourselves, so it's
            // done here.
            //Sv_FixLocalAngles(false /* apply only; don't clear flag */);
        }
    }

    // Console is always ticking.
    Con_Ticker(time);

    if(tickUI)
    {   // User interface ticks.
        UI_Ticker(time);
    }

    // Plugins tick always.
    Plug_DoHook(HOOK_TICKER, 0, &time);
}

/**
 * Advance time counters.
 */
void DD_AdvanceTime(timespan_t time)
{
    sysTime += time;

    if(!stopTime || netGame)
    {
        // The difference between gametic and demotic is that demotic
        // is not altered at any point. Gametic changes at handshakes.
        gameTime += time;
        demoTime += time;

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
        while((nowTime =
               Sys_GetSeconds()) - lastFrameTime < 1.0 / maxFrameRate)
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

        // The netcode gets to tick, too.
        Net_Ticker(/*ticLength*/);

        // Various global variables are used for counting time.
        DD_AdvanceTime(ticLength);
    }

    // Clients send commands periodically, not on every frame.
    if(!isClient)
    {
        Net_SendCommands();
    }
}
