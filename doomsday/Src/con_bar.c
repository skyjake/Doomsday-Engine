
//**************************************************************************
//**
//** CON_BAR.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_ui.h"

#include "sys_stwin.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int progress_active = false;
int progress_enabled = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char progress_title[64];
static int progress_max;
static int progress, progress_shown;

// CODE --------------------------------------------------------------------

//==========================================================================
// DD_InitProgress
//==========================================================================
void Con_InitProgress(const char *title, int full)
{
	if(isDedicated || !progress_enabled) return;
	
	// Init startup window progress bar.
	SW_SetBarMax(full);

	strcpy(progress_title, title);
	
	if(full >= 0)
	{
		progress_active = true;
		progress = progress_shown = 0;
		if(!full) full = 1;
		progress_max = full;
		Con_Progress(0, PBARF_INIT);
	}
}

//===========================================================================
// DD_HideProgress
//===========================================================================
void Con_HideProgress(void)
{
	progress_active = false;
	// Clear the startup window progress bar.
	SW_SetBarMax(0);
}

//==========================================================================
// DD_Progress
//	Draws a progress bar. Flags consists of one or more PBARF_* flags.
//==========================================================================
void Con_Progress(int count, int flags)
{
	int x, y, w, h, bor = 2, bar = 10;
	int fonthgt;

	if(!progress_active 
		|| isDedicated 
		|| !progress_enabled) return;

	if(flags & PBARF_SET)
		progress = count;
	else
		progress += count;

	// Make sure it's in range.
	if(progress < 0) progress = 0;
	if(progress > progress_max) progress = progress_max;

	// Update the startup window progress bar.
	SW_SetBarPos(progress);
	
	// If GL is not available, we cannot proceed any further.
	if(!GL_IsInited()) return;

	if(flags & PBARF_DONTSHOW
		&& progress < progress_max
		&& progress_shown+5 >= progress) return; // Don't show yet.

	progress_shown = progress;

	if(!(flags & PBARF_NOBACKGROUND))
	{
		// This'll redraw the startup screen to this page (necessary
		// if page flipping is used by the display adapter).
		Con_DrawStartupScreen(false);

		// If we're in the User Interface, this'll redraw it.
		UI_Drawer();
	}

	fonthgt = FR_TextHeight("A");

	// Go into screen projection mode.
	gl.MatrixMode(DGL_PROJECTION);
	gl.PushMatrix();
	gl.LoadIdentity();
	// 1-to-1 mapping for the whole window.
	gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

	// Calculate the size and dimensions of the progress window.
	w = screenWidth - 30;
	if(w < 50) w = 50;			// An unusual occurance...
	if(w > 610) w = 610;		// Restrict width to the case of 640x480.
	x = (screenWidth - w) / 2;	// Center on screen.
	h = 2*bor + fonthgt + 15 + bar;
	y = screenHeight - 15 - h;
	
	/*x = 15;
	w = screenWidth - 2*x;
	h = 2*bor + fonthgt + 15 + bar;
	y = screenHeight - 15 - h;*/

	// Draw the (opaque black) shadow.
	UI_Gradient(x + 4, y + 4, w, h, UI_COL(UIC_SHADOW), 0, 1, 1);

	// Background.
	UI_DrawRect(x, y, w, h, bor, UI_COL(UIC_BRD_HI), UI_COL(UIC_BRD_MED), 
		UI_COL(UIC_BRD_LOW));
	x += bor;
	y += bor;
	w -= 2*bor;
	h -= 2*bor;
	UI_Gradient(x, y, w, h, UI_COL(UIC_BG_MEDIUM), UI_COL(UIC_BG_LIGHT), 1, 1);

	// Title.
	x += 5;
	y += 5;
	w -= 10;
	gl.Color4f(0, 0, 0, .5f);
	FR_TextOut(progress_title, x+2, y+2);
	gl.Color3f(1, 1, 1);
	FR_TextOut(progress_title, x, y);
	y += fonthgt + 5;

	// Bar.
	UI_Gradient(x, y, w, bar, UI_COL(UIC_SHADOW), 0, .5f, .5f);
	UI_DrawRect(x+1, y+1, 8 + (w-8)*progress/progress_max - 2, bar - 2, 4,
		UI_COL(UIC_TEXT), UI_COL(UIC_BG_DARK), UI_COL(UIC_BG_LIGHT)
		/*UI_COL(UIC_TEXT), UI_COL(UIC_BG_DARK), UI_COL(UIC_BRD_LOW)*/);

	// Show what was drawn.
	if(!(flags & PBARF_NOBLIT)) gl.Show();

	// Restore old projection matrix.
	gl.MatrixMode(DGL_PROJECTION);
	gl.PopMatrix();
}
