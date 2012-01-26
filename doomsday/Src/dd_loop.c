/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Ker�en <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * dd_loop.c: Main Loop
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_ui.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define LIBDENG_PLAYER0_MOVEMENT_ANALYSIS

/*
 * There needs to be at least this many tics per second. A smaller value
 * is likely to cause unpredictable changes in playsim.
 */
#define MIN_TIC_RATE 35

/*
 * The length of one tic can be at most this.
 */
#define MAX_FRAME_TIME (1.0/MIN_TIC_RATE)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    Net_ResetTimer(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    DD_RunTics(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     maxFrameRate = 200;		// Zero means 'unlimited'.

timespan_t sysTime, gameTime, demoTime, levelTime;
timespan_t frameStartTime;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static double lastFrameTime;

static float fps;
static int lastFrameCount;

// CODE --------------------------------------------------------------------

/*
 * Register console variables for main loop.
 */
void DD_RegisterLoop(void)
{
    C_VAR_INT("refresh-rate-maximum", &maxFrameRate, 0, 35, 1000,
        "Maximum limit for the frame rate (default: 200).");
}

/*
 * This is the refresh thread (the main thread).
 * There is no return from here.
 */
void DD_GameLoop(void)
{
#ifdef WIN32
	MSG     msg;
#endif

	// Now we've surely finished startup.
	Con_StartupDone();
	Sys_ShowWindow(true);

	// Limit the frame rate to 35 when running in dedicated mode.
	if(isDedicated)
	{
		maxFrameRate = 35;
	}

	while(true)
	{
#ifdef WIN32
		// Start by checking Windows messages. 
		// This is the message pump.
		// Could be in a separate thread?
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
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

		// Send out new accumulation. Drawing will take the longest.
		Net_Update();
		DD_DrawAndBlit();
		Net_Update();

		// After the first frame, start timedemo.
		DD_CheckTimeDemo();
	}
}

/*
 * Drawing anything outside this routine is frowned upon. 
 * Seriously frowned!
 */
void DD_DrawAndBlit(void)
{
	if(novideo)
		return;

	// This'll let DGL know that some serious rendering is about to begin.
	// OpenGL doesn't need it, but Direct3D will do the BeginScene call.
	// If rendering happens outside The Sequence, DGL is forced, in 
	// Direct3D's case, to call BeginScene/EndScene before and after the
	// aforementioned operation.
	gl.Begin(DGL_SEQUENCE);

	if(ui_active)
	{
		// Draw user interface.
		UI_Drawer();
		UpdateState = I_FULLSCRN;
	}
	else
	{
		// Draw the game graphics.
		gx.G_Drawer();
		// The colored filter. 
		if(GL_DrawFilter())
			BorderNeedRefresh = true;
		// Draw Menu
		gx.MN_Drawer();
		// Debug information.
		Net_Drawer();
		S_Drawer();
		// Draw console.
		Con_Drawer();
	}

	// End the sequence.
	gl.End();

	// Flush buffered stuff to screen (blits everything).
	GL_DoUpdate();
}

//==========================================================================
// DD_StartFrame
//==========================================================================
void DD_StartFrame(void)
{
	frameStartTime = Sys_GetTimef();

	S_StartFrame();
	if(gx.BeginFrame)
	{
		gx.BeginFrame();
	}
}

//===========================================================================
// DD_EndFrame
//===========================================================================
void DD_EndFrame(void)
{
	uint nowTime = Sys_GetRealTime();
	static uint lastFpsTime = 0;
	
	// Increment the frame counter.
	framecount++;

	// Count the frames every other second.
	if(nowTime - 2000 >= lastFpsTime)
	{
		fps = (framecount - lastFrameCount) /
			((nowTime - lastFpsTime)/1000.0f);
		lastFpsTime = nowTime;
		lastFrameCount = framecount;
	}

	if(gx.EndFrame)
		gx.EndFrame();

	S_EndFrame();
}

//===========================================================================
// DD_GetFrameRate
//===========================================================================
float DD_GetFrameRate(void)
{
	return fps;
}

/*
 * This is the main ticker of the engine. We'll call all the other tickers
 * from here.
 */
void DD_Ticker(timespan_t time)
{
	static trigger_t fixed = { 1 / 35.0 };
	static float realFrameTimePos = 0;

	// Demo ticker. Does stuff like smoothing of view angles.
	Net_BuildLocalCommands(time);
	Demo_Ticker(time);
	P_Ticker(time);

	if(!ui_active || netgame)
	{
		// Advance frametime.  It will be reduced when new sharp world
		// positions are calculated, so that frametime always stays within
		// the range 0..1.
		realFrameTimePos += time * TICSPERSEC;
	
		if(M_CheckTrigger(&fixed, time))
		{
			gx.Ticker( /* time */ );	// Game DLL.

			// Server ticks.  These are placed here because
			// they still rely on fixed ticks and thus it's best to
			// keep them in sync with the fixed game ticks.
			if(isClient)
				Cl_Ticker( /* time */ );
			else
				Sv_Ticker( /* time */ );

			// This is needed by camera smoothing.  It needs to know
			// when the world tic has occured so the next sharp
			// position can be processed.

			// Frametime will be set back by one tick.
			realFrameTimePos -= 1;

			R_NewSharpWorld();

#ifdef LIBDENG_PLAYER0_MOVEMENT_ANALYSIS
            if(ddplayers[0].ingame && ddplayers[0].mo)
            {
                mobj_t* mo = ddplayers[0].mo;
                static float prevPos[2] = { 0, 0 };
                static float prevSpeed = 0;
                float mom[2] = { FIX2FLT(mo->momx), FIX2FLT(mo->momy) };
                float speed = V2_Length(mom);
                float actualMom[2] = { FIX2FLT(mo->x) - prevPos[0], FIX2FLT(mo->y) - prevPos[1] };
                float actualSpeed = V2_Length(actualMom);

                Con_Message("%i,%f,%f,%f,%f\n", SECONDS_TO_TICKS(sysTime + time),
                            0.f, speed, actualSpeed, speed - prevSpeed);

                prevPos[0] = FIX2FLT(mo->x);
                prevPos[1] = FIX2FLT(mo->y);
                prevSpeed = speed;
            }
#endif
		}

		// While paused, don't modify frametime so things keep still.
		if(!clientPaused)
			frameTimePos = realFrameTimePos;

		Con_Ticker(time);		// Console.

		// We can't sent FixAngles messages to ourselves, so it's
		// done here.
		Sv_FixLocalAngles();
	}

	if(ui_active)
	{
		// User interface ticks.
		UI_Ticker(time);
	}
}

/*
 * Advance time counters.
 */
void DD_AdvanceTime(timespan_t time)
{
	sysTime += time;

	if(!ui_active || netgame)
	{
		// The difference between gametic and demotic is that demotic
		// is not altered at any point. Gametic changes at handshakes.
		gameTime += time;
		demoTime += time;

		// Leveltic is reset to zero at every map change.
		// The level time only advances when the game is not paused.
		if(!clientPaused)
		{
			levelTime += time;
		}
	}
}

static boolean firstTic = true;

/*
 * Reset the game time so that on the next frame, the effect will be
 * that no time has passed.
 */
void DD_ResetTimer(void)
{
	firstTic = true;
	Net_ResetTimer();
}

/*
 * Run at least one tic.
 */
void DD_RunTics(void)
{
	double  frameTime, ticLength;
	double  nowTime = Sys_GetSeconds();

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
			Sys_Sleep(2);
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
		DD_ProcessEvents();
		
		// Call all the tickers.
		DD_Ticker(ticLength);

		// The netcode gets to tick, too.
		Net_Ticker(ticLength);

		// Various global variables are used for counting time.
		DD_AdvanceTime(ticLength);
	}

	// Clients send commands periodically, not on every frame.
	if(!isClient)
	{
		Net_SendCommands();
	}
}
