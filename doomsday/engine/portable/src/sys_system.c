/**\file sys_system.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * Abstract interfaces to platform-level services.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  include <windows.h>
#  include <process.h>
#endif

#include <signal.h>
#include <SDL.h>
#include <SDL_thread.h>

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

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//int       systics = 0;    // System tics (every game tic).
int novideo;                // if true, stay in text mode for debugging

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

#ifdef WIN32
/**
 * Borrowed from Lee Killough.
 */
static void C_DECL handler(int s)
{
    signal(s, SIG_IGN);  // Ignore future instances of this signal.

    Con_Error(s==SIGSEGV ? "Segmentation Violation\n" :
              s==SIGINT  ? "Interrupted by User\n" :
              s==SIGILL  ? "Illegal Instruction\n" :
              s==SIGFPE  ? "Floating Point Exception\n" :
              s==SIGTERM ? "Killed\n" : "Terminated by signal\n");
}
#endif

/**
 * Initialize platform level services.
 *
 * \note This must be called from the main thread due to issues with the devices
 * we use via the WINAPI, MCI (cdaudio, mixer etc) on the WIN32 platform.
 */
void Sys_Init(void)
{
    uint startTime = (verbose >= 2? Sys_GetRealTime() : 0);

    Con_Message("Setting up platform state...\n");

    VERBOSE( Con_Message("Initializing Timing subsystem...\n") )
    Sys_InitTimer();

    if(!isDedicated)
    {
        VERBOSE( Con_Message("Initializing Input subsystem...\n") )
        if(!I_Init())
            Con_Error("Failed to initialize Input subsystem.\n");
    }

    // Virtual devices need to be created even in dedicated mode.
    I_InitVirtualInputDevices();

    VERBOSE( Con_Message("Initializing Audio subsystem...\n") )
    S_Init();

#if defined(WIN32) && !defined(_DEBUG)
    // Register handler for abnormal situations (in release build).
    signal(SIGSEGV, handler);
    signal(SIGTERM, handler);
    signal(SIGILL, handler);
    signal(SIGFPE, handler);
    signal(SIGILL, handler);
    signal(SIGABRT, handler);
#endif

#ifndef WIN32
    // We are not worried about broken pipes. When a TCP connection closes,
    // we prefer to receive an error code instead of a signal.
    signal(SIGPIPE, SIG_IGN);
#endif

    VERBOSE( Con_Message("Initializing Network subsystem...\n") )
    Huff_Init();
    N_Init();

    VERBOSE2( Con_Message("Sys_Init: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
}

/**
 * Return to default system state.
 */
void Sys_Shutdown(void)
{
    // Time to unload *everything*.
    if(DD_GameLoaded())
        Con_Execute(CMDS_DDAY, "unload", true, false);

    B_Shutdown();
    Sys_ShutdownTimer();

    Net_Shutdown();
    Huff_Shutdown();
    // Let's shut down sound first, so Windows' HD-hogging doesn't jam
    // the MUS player (would produce horrible bursts of notes).
    S_Shutdown();
    GL_Shutdown();
    DD_ClearEvents();
    I_ShutdownInputDevices();
    I_Shutdown();

    DD_DestroyGames();
}

static int showCriticalMessage(const char* msg)
{
#ifdef WIN32
#ifdef UNICODE
    wchar_t buf[256];
#else
    char buf[256];
#endif
    int ret;
    HWND hWnd = Sys_GetWindowHandle(windowIDX);

    if(!hWnd)
    {
        suspendMsgPump = true;
        MessageBox(HWND_DESKTOP, TEXT("Sys_CriticalMessage: Main window not available."),
                   NULL, MB_ICONERROR | MB_OK);
        suspendMsgPump = false;
        return false;
    }

    ShowCursor(TRUE);
    ShowCursor(TRUE);
    suspendMsgPump = true;
    GetWindowText(hWnd, buf, 255);
    ret = (MessageBox(hWnd, WIN_STRING(msg), buf, MB_OK | MB_ICONEXCLAMATION) == IDYES);
    suspendMsgPump = false;
    ShowCursor(FALSE);
    ShowCursor(FALSE);
    return ret;
#else
    fprintf(stderr, "--- %s\n", msg);
    return 0;
#endif
}

int Sys_CriticalMessage(const char* msg)
{
    return showCriticalMessage(msg);
}

int Sys_CriticalMessagef(const char* format, ...)
{
    static const char* unknownMsg = "Unknown critical issue occured.";
    const size_t BUF_SIZE = 655365;
    const char* msg;
    char* buf = 0;
    va_list args;
    int result;

    if(format && format[0])
    {
        va_start(args, format);
        buf = (char*) calloc(1, BUF_SIZE);
        dd_vsnprintf(buf, BUF_SIZE, format, args);
        msg = buf;
        va_end(args);
    }
    else
    {
        msg = unknownMsg;
    }

    result = showCriticalMessage(msg);

    if(buf) free(buf);
    return result;
}

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

void Sys_ShowCursor(boolean show)
{
#ifdef WIN32
    ShowCursor(show);
#endif
#ifdef UNIX
    SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
#endif
}

void Sys_HideMouse(void)
{
#ifdef WIN32
    if(novideo)
        return;

    ShowCursor(FALSE);
#endif
#ifdef UNIX
    Sys_ShowCursor(false);
#endif
}

/**
 * Called when Doomsday should quit (will be deferred until convienent).
 */
void Sys_Quit(void)
{
    appShutdown = true;
}

void Sys_MessageBox(const char *msg, boolean iserror)
{
#ifdef WIN32
#ifdef UNICODE
    wchar_t title[300];
#else
    char    title[300];
#endif
    HWND    hWnd = Sys_GetWindowHandle(windowIDX);

    if(!hWnd)
    {
        suspendMsgPump = true;
        MessageBox(HWND_DESKTOP,
                   TEXT("Sys_MessageBox: Main window not available."), NULL,
                   MB_ICONERROR | MB_OK);
        suspendMsgPump = false;
        return;
    }

    suspendMsgPump = true;
    GetWindowText(hWnd, title, 300);
    MessageBox(hWnd, WIN_STRING(msg), title,
               MB_OK | (iserror ? MB_ICONERROR : MB_ICONINFORMATION));
    suspendMsgPump = false;
#endif
#ifdef UNIX
    fprintf(stderr, "%s %s\n", iserror ? "**ERROR**" : "---", msg);
#endif
}

/**
 * Opens the given file in a suitable text editor.
 */
void Sys_OpenTextEditor(const char *filename)
{
#ifdef WIN32
    // Everybody is bound to have Notepad.
    spawnlp(P_NOWAIT, "notepad.exe", "notepad.exe", filename, 0);
#endif
}

/**
 * Utilises SDL Threads on ALL systems.
 * Returns the SDL_thread structure as handle to the started thread.
 */
thread_t Sys_StartThread(systhreadfunc_t startpos, void *parm)
{
    SDL_Thread* thread = SDL_CreateThread(startpos, parm);

    if(thread == NULL)
    {
        Con_Message("Sys_StartThread: Failed to start new thread (%s).\n",
                    SDL_GetError());
        return 0;
    }

    return thread;
}

/**
 * Suspends or resumes the execution of a thread.
 */
void Sys_SuspendThread(thread_t handle, boolean dopause)
{
    Con_Error("Sys_SuspendThread: Not implemented.\n");
}

/**
 * @return              The return value of the thread.
 */
int Sys_WaitThread(thread_t thread)
{
    int             result = 0;

    SDL_WaitThread(thread, &result);
    return result;
}

/**
 * @return              The identifier of the current thread.
 */
uint Sys_ThreadID(void)
{
    return SDL_ThreadID();
}

mutex_t Sys_CreateMutex(const char *name)
{
    return (mutex_t) SDL_CreateMutex();
}

void Sys_DestroyMutex(mutex_t handle)
{
    if(!handle)
        return;

    SDL_DestroyMutex((SDL_mutex *) handle);
}

void Sys_Lock(mutex_t handle)
{
    if(!handle)
        return;
    SDL_mutexP((SDL_mutex *) handle);
}

void Sys_Unlock(mutex_t handle)
{
    if(!handle)
        return;
    SDL_mutexV((SDL_mutex *) handle);
}

sem_t Sem_Create(uint32_t initialValue)
{
    return (sem_t) SDL_CreateSemaphore(initialValue);
}

void Sem_Destroy(sem_t semaphore)
{
    if(semaphore)
    {
        SDL_DestroySemaphore((SDL_sem *) semaphore);
    }
}

void Sem_P(sem_t semaphore)
{
    if(semaphore)
    {
        SDL_SemWait((SDL_sem *) semaphore);
    }
}

void Sem_V(sem_t semaphore)
{
    if(semaphore)
    {
        SDL_SemPost((SDL_sem *) semaphore);
    }
}

uint32_t Sem_Value(sem_t semaphore)
{
    if(semaphore)
    {
        return (uint32_t)SDL_SemValue((SDL_sem*)semaphore);
    }
    return 0;
}
