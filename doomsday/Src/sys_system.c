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
 * sys_system.c: OS Specific Services Subsystem
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#	include <windows.h>
#	include <process.h>
#endif

#ifdef UNIX
#	include <SDL.h>
#	include <SDL_thread.h>
#endif

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

#ifdef WIN32
extern HWND			hWndMain;
extern HINSTANCE	hInstApp;
#endif

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
#ifdef WIN32
	// Initialize COM.
	CoInitialize(NULL);
#endif

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

#ifdef WIN32
	CoUninitialize();
#endif
}

//===========================================================================
// Sys_CriticalMessage
//===========================================================================
int Sys_CriticalMessage(char *msg)
{
#ifdef WIN32
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
#else
	return 0;
#endif
}

//===========================================================================
// Sys_Sleep
//===========================================================================
void Sys_Sleep(int millisecs)
{
#ifdef WIN32
	Sleep(millisecs);
#endif
#ifdef UNIX
	// Not guaranteed to be very accurate...
	SDL_Delay(millisecs);
#endif
}

//===========================================================================
// Sys_ShowCursor
//===========================================================================
void Sys_ShowCursor(boolean show)
{
#ifdef WIN32
	ShowCursor(show);
#endif
#ifdef UNIX
	SDL_ShowCursor(show? SDL_ENABLE : SDL_DISABLE);
#endif
}

//===========================================================================
// Sys_HideMouse
//===========================================================================
void Sys_HideMouse(void)
{
	if(novideo || nofullscreen) return;
#ifdef WIN32
	ShowCursor(FALSE);
	ShowCursor(FALSE);
#endif
#ifdef UNIX
	Sys_ShowCursor(false);
#endif
}

//===========================================================================
// Sys_ShowWindow
//===========================================================================
void Sys_ShowWindow(boolean show)
{
	// Showing does not work in dedicated mode.
	if(isDedicated && show) return; 

#ifdef WIN32	
	SetWindowPos(hWndMain, HWND_TOP, 0, 0, 0, 0, 
		(show? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOSIZE | SWP_NOMOVE);
	if(show) SetActiveWindow(hWndMain);
#endif	
}

//==========================================================================
// Sys_Quit
//	Shut everything down and quit the program.
//==========================================================================
void Sys_Quit(void)
{
	// Quit netgame if one is in progress.
	if(netgame)
	{
		Con_Execute(isServer? "net server close" : "net disconnect",
					true);
	}

	Demo_StopPlayback();
	Con_SaveDefaults();
	Sys_Shutdown();
	B_Shutdown();
	Con_Shutdown();
	DD_Shutdown();

	// Stop the execution of the program.
	exit(0);
}

//===========================================================================
// Sys_MessageBox
//===========================================================================
void Sys_MessageBox(const char *msg, boolean iserror)
{
#ifdef WIN32	
	char title[300];

	GetWindowText(hWndMain, title, 300);
	MessageBox(hWndMain, msg, title, MB_OK | (iserror? MB_ICONERROR
		: MB_ICONINFORMATION));
#endif	
}

//===========================================================================
// Sys_OpenTextEditor
//	Opens the given file in a suitable text editor.
//===========================================================================
void Sys_OpenTextEditor(const char *filename)
{
#ifdef WIN32
	// Everybody is bound to have Notepad.
	spawnlp(P_NOWAIT, "notepad.exe", "notepad.exe", filename, 0);
#endif
}

/*
 * Priority can be -3...3, with zero being the normal priority.
 * Returns a handle to the started thread.
 */
int Sys_StartThread(systhreadfunc_t startPos, void *parm, int priority)
{
#ifdef WIN32
	HANDLE handle;
	DWORD id;

	// Use 512 Kb for the stack (too much/little?).
	handle = CreateThread(0, 0x80000, startPos, parm, 0, &id);
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
#else
	return (int) SDL_CreateThread(startpos, parm);
#endif
}

/*
 * Suspend or resume the execution of a thread.
 */
void Sys_SuspendThread(int handle, boolean dopause)
{
#ifdef WIN32	
	if(dopause)
	{
		SuspendThread( (HANDLE) handle);
	}
	else
	{
		ResumeThread( (HANDLE) handle);
	}
#endif	
}

#ifdef UNIX
/*
 * Returns the return value of the thread.
 */
int Sys_WaitThread(int handle)
{
	int result;

	SDL_WaitThread((SDL_Thread*)handle, &result);
	return result;
}

int Sys_CreateMutex(const char *name)
{
	return (int) SDL_CreateMutex();
}

void Sys_DestroyMutex(int handle)
{
	if(!handle) return;	
	SDL_DestroyMutex((SDL_mutex*)handle);
}

void Sys_Lock(int handle)
{
	if(!handle) return;	
	SDL_mutexP((SDL_mutex*)handle);
}

void Sys_Unlock(int handle)
{
	if(!handle) return;
	SDL_mutexV((SDL_mutex*)handle);
}

#endif

/*
 * Create a new semaphore. Returns a handle.
 */
semaphore_t Sem_Create(unsigned int initialValue)
{
	return (semaphore_t) SDL_CreateSemaphore(initialValue);
}

void Sem_Destroy(semaphore_t semaphore)
{
	if(semaphore)
	{
		SDL_DestroySemaphore((SDL_sem*)semaphore);
	}
}

/*
 * "Proberen" a semaphore. Blocks until the successful.
 */
void Sem_P(semaphore_t semaphore)
{
	if(semaphore)
	{
		SDL_SemWait((SDL_sem*)semaphore);
	}
}
/*
 * Return the exit code of the given thread. Returns true if the thread
 * has stopped, false if not.
 */
boolean Sys_GetThreadExitCode(int handle, uint *exitCode)
{
	DWORD code;

	if(!GetExitCodeThread((HANDLE)handle, &code))
	{
		// Bad handle?
		return false;
	}

	// Still going?
	if(code == STILL_ACTIVE) return false;

	if(exitCode) *exitCode = code;
	return true;
}

/*
 * Create a new mutex. Returns a handle with which the mutex can be acquired
 * and released.
 */
int Sys_CreateMutex(const char *name)
{
	return (int) CreateMutex(NULL, FALSE, name);
}

/*
 * Destroy the mutex object. 
 */
void Sys_DestroyMutex(int handle)
{
	CloseHandle((HANDLE)handle);
}

/*
 * Acquire a mutex. Blocks until ownership has been acquired.
 */
void Sys_Lock(int mutexHandle)
{
	// Five seconds is plenty.
	if(WaitForSingleObject((HANDLE)mutexHandle, 5000) == WAIT_TIMEOUT) 
	{
		// Couldn't lock it.
#ifdef _DEBUG
		Con_Error("Sys_AcquireMutex: Failed to acquire mutex.\n");
#endif
	}
}

/*
 * Release a mutex.
 */
void Sys_Unlock(int mutexHandle)
{
	ReleaseMutex((HANDLE)mutexHandle);
}
/*
 * "Verhogen" a semaphore. Returns immediately.
 */
void Sem_V(semaphore_t semaphore)
{
	if(semaphore)
	{
		SDL_SemPost((SDL_sem*)semaphore);
	}
}
