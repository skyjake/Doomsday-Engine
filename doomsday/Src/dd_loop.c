/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�en <jaakko.keranen@iki.fi>
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

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void DD_RunTics(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int maxFrameRate = 0;	// Zero means 'unlimited'.

timespan_t sysTime, gameTime, demoTime, levelTime;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static double lastFrameTime;

static int lastfpstic = 0, fpsnum = 0, lastfc = 0;

// CODE --------------------------------------------------------------------

/*
 * This is the refresh thread (the main thread).
 * There is no return from here.
 */
void DD_GameLoop(void)
{
#ifdef WIN32
	MSG msg;
#endif
	
	// Now we've surely finished startup.
	Con_StartupDone();
	Sys_ShowWindow(true);

	// Start polling for input. The input thread will be stopped when 
	// the engine is shut down.
	DD_StartInput();

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
	if(novideo) return;

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
		if(GL_DrawFilter()) BorderNeedRefresh = true;
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
	S_StartFrame();
	if(gx.BeginFrame) gx.BeginFrame();
}

//===========================================================================
// DD_EndFrame
//===========================================================================
void DD_EndFrame (void)
{
	// Increment the frame counter.
	framecount++;

	// Count the frames.
	if(Sys_GetTime() - 35 >= lastfpstic)
	{
		lastfpstic = Sys_GetTime();
		fpsnum = framecount - lastfc;
		lastfc = framecount;
	}

	if(gx.EndFrame) gx.EndFrame();

	S_EndFrame();
}

//===========================================================================
// DD_GetFrameRate
//===========================================================================
int DD_GetFrameRate(void)
{
	return fpsnum;
}

/*
 * This is the main ticker of the engine. We'll call all the other tickers
 * from here.
 */
void DD_Ticker(timespan_t time)
{
	static trigger_t fixed = { 1/35.0 };

	// Client/server ticks.
	if(isClient) 
		Cl_Ticker(time); 
	else 
		Sv_Ticker(time);

	// Demo ticker. Does stuff like smoothing of view angles.
	Demo_Ticker(time);				
	P_Ticker(time);

	if(!ui_active || netgame)
	{
		if(M_CheckTrigger(&fixed, time))
			gx.Ticker(/*time*/); // Game DLL.

		Con_Ticker(time);		// Console.

		// We can't sent FixAngles messages to ourselves, so it's
		// done here.
		//Sv_FixLocalAngles();
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
	//systics++;
	sysTime += time;

	if(!ui_active || netgame) 
	{
		//if(availabletics > 0) availabletics--;

		// The difference between gametic and demotic is that demotic
		// is not altered at any point. Gametic changes at handshakes.
		/*gametic++;
		demotic++;*/
		gameTime += time;
		demoTime += time;

		// Leveltic is reset to zero at every map change.
		// The level time only advances when the game is not paused.
		if(!clientPaused) /*leveltic++;*/
		{
			levelTime += time;
		}
	}
}

/*
 * Run at least one tic.
 */
void DD_RunTics(void)
{
	static boolean firstTic = true;
	double nowTime, frameTime, ticLength;

	// Do a network update first.
	N_Update();
	Net_Update();

	// Check the clock. 
	if(firstTic)
	{
		// On the first tic, no time actually passes.
		firstTic = false;
		lastFrameTime = Sys_GetSeconds();
		return;
	}

	// We'll sleep until we go past the maxfps interval (the shortest
	// allowed interval between tics).
	if(maxFrameRate > 0)
	{
		while((nowTime = Sys_GetSeconds()) - lastFrameTime
			< 1.0/maxFrameRate)
		{
			// Wait for a short while.
			Sys_Sleep(2);
		}
	}
	else
	{
		// Unlimited FPS.
		nowTime = Sys_GetSeconds();
	}

	// How much time do we have for this frame?
	frameTime     = nowTime - lastFrameTime;
	lastFrameTime = nowTime;

	// Tic length is determined by the minfps rate.
	while(frameTime > 0)
	{
		ticLength = MIN_OF( MAX_FRAME_TIME, frameTime );
		frameTime -= ticLength;

		// Call all the tickers.
		DD_Ticker(ticLength);

		// The netcode gets to tick, too.
		Net_Ticker(ticLength);

		// Various global variables are used for counting time.
		DD_AdvanceTime(ticLength);
	}

	// Do one final network update. (Network needs its own thread...)
	Net_Update();
}

#if 0
//===========================================================================
// DD_TryRunTics
//	Run at least one tic.
//===========================================================================
void DD_TryRunTics(void)
{
	int counts;

	// Low-level network update.
	N_Update();

	// High-level network update.
	Net_Update();

	// Wait for at least one tic. (realtics >= availabletics)
	while(!(counts = (isDedicated || netgame || ui_active?
					  realtics : availabletics)))
	{
		if((!isDedicated && rend_camera_smooth)
			|| net_dontsleep 
			|| !net_ticsync) break;
		Sys_Sleep(3);
		Net_Update();
	}

	// If we are running in timedemo mode, always run only one tic.
	if(!net_ticsync) counts = 1;
				
	// We'll tick away all the tics we can.
	// Zero the real tics counter.
	realtics = 0;

	// Run the tics.
	while(counts--)
	{
		// Client/server ticks.
		if(isClient) Cl_Ticker(); else Sv_Ticker();

		// Demo ticker. Does stuff like smoothing of view angles.
		Demo_Ticker();				
		P_Ticker();

		if(!ui_active || netgame)
		{
			gx.Ticker();			// Game DLL.
			Con_Ticker();			// Console.
			// We can't sent FixAngles messages to ourselves, so it's
			// done here.
			Sv_FixLocalAngles();
		}
		if(ui_active)
		{
			// User interface ticks.
			UI_Ticker();
		}
		// The netcode gets to tick.
		Net_Ticker();
		Net_Update();

		// This tick is done, advance time.
		systics++;
		if(!ui_active || netgame) 
		{
			if(availabletics > 0) availabletics--;

			// The difference between gametic and demotic is that demotic
			// is not altered at any point. Gametic changes at handshakes.
			gametic++;
			demotic++;

			// Leveltic is reset to zero at every map change.
			// The level time only advances when the game is not paused.
			if(!clientPaused) leveltic++;
		}
	}
}

#endif
