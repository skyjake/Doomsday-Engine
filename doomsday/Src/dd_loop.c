
//**************************************************************************
//**
//** DD_LOOP.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int lastfpstic = 0, fpsnum = 0, lastfc = 0;

// CODE --------------------------------------------------------------------

//==========================================================================
// DD_GameLoop
//==========================================================================
void DD_GameLoop(void)
{
	MSG msg;
	
	// Now we've surely finished startup.
	Con_StartupDone();
	Sys_ShowWindow(true);
	//GL_RuntimeMode();

	if(ArgCheck("-debugfile"))
	{
		char filename[20];
		sprintf(filename, "debug%i.txt", consoleplayer);
		debugfile = fopen(filename, "w");
	}
	
	while(1)
	{
		// Start by checking Windows messages. 
		// This is the message pump.
		// Could be in a separate thread?
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Frame syncronous I/O operations.
		DD_StartFrame();
		// Will run at least one tic.
		DD_TryRunTics();
		// Update clients.
		Sv_TransmitFrame();
		DD_EndFrame();		
		if(!novideo) 
		{
			// Send out new accumulation.
			Net_Update();
			DD_DrawAndBlit();
		}
		// Send out new accumulation.
		Net_Update();

		// After the first frame, start timedemo.
		DD_CheckTimeDemo();
	}
}

//===========================================================================
// DD_DrawAndBlit
//	Drawing anything outside this routine is frowned upon. Seriously
//	frowned!
//===========================================================================
void DD_DrawAndBlit(void)
{
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
/*	if(use_jtSound) 
		I2_BeginSoundFrame(); 
#ifdef A3D_SUPPORT
	else 
		I3_BeginSoundFrame();
#endif*/
	S_StartFrame();
	if(gx.BeginFrame) gx.BeginFrame();
	// Song check.
	/* I_CheckSong(); */
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

//==========================================================================
// DD_StartTic
//	Called before processing each tic in a frame.
//==========================================================================
void DD_StartTic (void)
{
}

//===========================================================================
// DD_TryRunTics
//	Run at least one tic.
//===========================================================================
void DD_TryRunTics (void)
{
	int counts;

	// Low-level network update.
	N_Update();

	// High-level network update.
	Net_Update();

	// Wait for at least one tic. (realtics >= availabletics)
	while(!(counts = (netgame||ui_active? realtics : availabletics)))
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

