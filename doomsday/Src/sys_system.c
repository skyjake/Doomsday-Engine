
//**************************************************************************
//**
//** SYS_SYSTEM.C 
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <windows.h>
#include <process.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND			hWndMain;
extern HINSTANCE	hInstApp; 

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int			systics = 0;	// System tics (every game tic).
boolean		novideo;		// if true, stay in text mode for debugging

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
// Sys_Init
//	Initialize machine state.
//==========================================================================
void Sys_Init(void)
{
	// Initialize COM.
	CoInitialize(NULL);

	Con_Message("Sys_Init: Initializing keyboard, mouse and joystick.\n");
	if(!isDedicated)
	{
		if(!I_Init())
			Con_Error("Sys_Init: failed to initialize DirectInput.\n");
	}
	Sys_InitTimer();
	Sys_InitMixer();
	S_Init();
	Huff_Init();
	N_Init();
}

//==========================================================================
// Sys_Shutdown
//	Return to default system state.
//==========================================================================
void Sys_Shutdown(void)
{
	Sys_ShutdownTimer();

	if(gx.Shutdown) gx.Shutdown();

	Net_Shutdown();
	Huff_Shutdown();
	// Let's shut down sound first, so Windows' HD-hogging doesn't jam
	// the MUS player (would produce horrible bursts of notes).
	S_Shutdown();
	Sys_ShutdownMixer();
	GL_Shutdown();
	I_Shutdown();

	CoUninitialize();
}

//===========================================================================
// Sys_CriticalMessage
//===========================================================================
int Sys_CriticalMessage(char *msg)
{
	char buf[256];
	int ret;

	ShowCursor(TRUE);
	ShowCursor(TRUE);
	GetWindowText(hWndMain, buf, 255);
	ret = (MessageBox(hWndMain, msg, buf, MB_YESNO|MB_ICONEXCLAMATION) 
		== IDYES);
	ShowCursor(FALSE);
	ShowCursor(FALSE);
	return ret;
}

//===========================================================================
// Sys_Sleep
//===========================================================================
void Sys_Sleep(int millisecs)
{
	Sleep(millisecs);
}

//===========================================================================
// Sys_ShowCursor
//===========================================================================
void Sys_ShowCursor(boolean show)
{
	ShowCursor(show);
}

//===========================================================================
// Sys_HideMouse
//===========================================================================
void Sys_HideMouse(void)
{
	if(novideo || nofullscreen) return;
	ShowCursor(FALSE);
	ShowCursor(FALSE);
}

//===========================================================================
// Sys_ShowWindow
//===========================================================================
void Sys_ShowWindow(boolean show)
{
	// Showing does not work in dedicated mode.
	if(isDedicated && show) return; 

	SetWindowPos(hWndMain, HWND_TOP, 0, 0, 0, 0, 
		(show? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOSIZE | SWP_NOMOVE);
	if(show) SetActiveWindow(hWndMain);
}

//==========================================================================
// Sys_Quit
//	Shut everything down and quit the program.
//==========================================================================
void Sys_Quit(void)
{
	// Quit netgame if one is in progress.
	if(netgame)
		Con_Execute(isServer? "net server close" : "net disconnect", true);

	Demo_StopPlayback();
	Con_SaveDefaults();
	Sys_Shutdown();
	B_Shutdown();
	Con_Shutdown();
	DD_Shutdown();
	exit(0);
}

//===========================================================================
// superatol
//===========================================================================
long superatol(char *s)
{
	char *endptr;
	long val = strtol(s, &endptr, 0);

	if(*endptr == 'k' || *endptr == 'K')
		val *= 1024;
	else if(*endptr == 'm' || *endptr == 'M')
		val *= 1048576;
	return val;
}

//==========================================================================
// Sys_ZoneBase
//==========================================================================
byte *Sys_ZoneBase (size_t *size)
{
	size_t heap;
	byte *ptr;

	// Check for the -maxzone option.
	if(ArgCheckWith("-maxzone", 1)) maxzone = superatol(ArgNext());

	heap = maxzone;
	if(heap < MINIMUM_HEAP_SIZE) heap = MINIMUM_HEAP_SIZE;
	if(heap > MAXIMUM_HEAP_SIZE) heap = MAXIMUM_HEAP_SIZE;
	heap += 0x10000;
	
	do { // Until we get the memory (usually succeeds on the first try).
		heap -= 0x10000;                // leave 64k alone
		ptr = malloc (heap);
	} while(!ptr);

	Con_Message("  %.1f Mb allocated for zone.\n", heap/1024.0/1024.0);
	Con_Message("  ZoneBase: 0x%X.\n", (int)ptr);

	if (heap < 0x180000)
		Con_Error("  Insufficient memory!");

	*size = heap;
	return ptr;
}

//===========================================================================
// Sys_MessageBox
//===========================================================================
void Sys_MessageBox(const char *msg, boolean iserror)
{
	char title[300];

	GetWindowText(hWndMain, title, 300);
	MessageBox(hWndMain, msg, title, MB_OK | (iserror? MB_ICONERROR
		: MB_ICONINFORMATION));
}

//===========================================================================
// Sys_OpenTextEditor
//	Opens the given file in a suitable text editor.
//===========================================================================
void Sys_OpenTextEditor(const char *filename)
{
	// Everybody is bound to have Notepad.
	spawnlp(P_NOWAIT, "notepad.exe", "notepad.exe", filename, 0);
}

//===========================================================================
// Sys_StartThread
//	Priority can be -3...3, with zero being the normal priority.
//	Returns a handle to the started thread.
//===========================================================================
int Sys_StartThread(systhreadfunc_t startpos, void *parm, int priority)
{
	HANDLE handle;
	DWORD id;

	// Use 512 Kb for the stack (too much/little?).
	handle = CreateThread(0, 0x80000, startpos, parm, 0, &id);
	if(!handle)
	{
		Con_Message("Sys_StartThread: Failed to start new thread (%x).\n",
			GetLastError());
		return 0;
	}
	// Set thread priority, if needed.
	if(priority && priority <= 3 && priority >= -3)
	{
		int prios[] = {
			THREAD_PRIORITY_IDLE,
			THREAD_PRIORITY_LOWEST,
			THREAD_PRIORITY_BELOW_NORMAL,
			THREAD_PRIORITY_NORMAL,
			THREAD_PRIORITY_ABOVE_NORMAL,
			THREAD_PRIORITY_HIGHEST,
			THREAD_PRIORITY_TIME_CRITICAL
		};
		SetThreadPriority(handle, prios[priority + 3]);
	}
	return (int) handle;
}

//===========================================================================
// Sys_SuspendThread
//	Suspends or resumes the execution of a thread.
//===========================================================================
void Sys_SuspendThread(int handle, boolean dopause)
{
	if(dopause)
		SuspendThread( (HANDLE) handle);
	else
		ResumeThread( (HANDLE) handle);
}


