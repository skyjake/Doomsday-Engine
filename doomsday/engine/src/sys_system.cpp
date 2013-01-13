/**\file sys_system.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <process.h>
#endif

#include <signal.h>

#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_misc.h"

#include "dd_main.h"
#include "dd_loop.h"
#ifdef __CLIENT__
#  include "gl/gl_main.h"
#endif
#include "ui/nativeui.h"
#include "network/net_main.h"
#include "audio/s_main.h"

int novideo;                // if true, stay in text mode for debugging

static boolean appShutdown = false; ///< Set to true when we should exit (normally).

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
    uint startTime;

    Con_Message("Setting up platform state...\n");

    startTime = (verbose >= 2? Timer_RealMilliseconds() : 0);

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
    N_Init();

    VERBOSE2( Con_Message("Sys_Init: Done in %.2f seconds.\n", (Timer_RealMilliseconds() - startTime) / 1000.0f) );
}

boolean Sys_IsShuttingDown(void)
{
    return appShutdown;
}

/**
 * Return to default system state.
 */
void Sys_Shutdown(void)
{
    // We are now shutting down.
    appShutdown = true;

    // Time to unload *everything*.
    if(DD_GameLoaded())
        Con_Execute(CMDS_DDAY, "unload", true, false);

    B_Shutdown();
    Net_Shutdown();
    // Let's shut down sound first, so Windows' HD-hogging doesn't jam
    // the MUS player (would produce horrible bursts of notes).
    S_Shutdown();
#ifdef __CLIENT__
    GL_Shutdown();
#endif
    DD_ClearEvents();
    I_ShutdownInputDevices();
    I_Shutdown();

    DD_DestroyGames();
}

static int showCriticalMessage(const char* msg)
{
    Sys_MessageBox(MBT_WARNING, DOOMSDAY_NICENAME, msg, 0);
    return 0;

#if 0
#ifdef WIN32
#ifdef UNICODE
    wchar_t buf[256];
#else
    char buf[256];
#endif
    int ret;
    HWND hWnd = (HWND) Window_NativeHandle(Window_Main());

    if(!hWnd)
    {
        DD_Win32_SuspendMessagePump(true);
        MessageBox(HWND_DESKTOP, TEXT("Sys_CriticalMessage: Main window not available."),
                   NULL, MB_ICONERROR | MB_OK);
        DD_Win32_SuspendMessagePump(false);
        return false;
    }

    ShowCursor(TRUE);
    ShowCursor(TRUE);
    DD_Win32_SuspendMessagePump(true);
    GetWindowText(hWnd, buf, 255);
    ret = (MessageBox(hWnd, WIN_STRING(msg), buf, MB_OK | MB_ICONEXCLAMATION) == IDYES);
    DD_Win32_SuspendMessagePump(false);
    ShowCursor(FALSE);
    ShowCursor(FALSE);
    return ret;
#else
    fprintf(stderr, "--- %s\n", msg);
    return 0;
#endif
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
    /*
#ifdef WIN32
    Sleep(millisecs);
#endif
*/
    Thread_Sleep(millisecs);
}

void Sys_BlockUntilRealTime(uint realTimeMs)
{
    uint remaining = realTimeMs - Timer_RealMilliseconds();
    if(remaining > 50)
    {
        // Target time is in the past; or the caller is attempting to wait for
        // too long a time.
        return;
    }

    while(Timer_RealMilliseconds() < realTimeMs)
    {
        // Do nothing; don't yield execution. We want to exit here at the
        // precise right moment.
    }
}

void Sys_ShowCursor(boolean show)
{
#ifdef WIN32
    ShowCursor(show);
#endif
/*#ifdef UNIX
    SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
#endif*/
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
 * Called when Doomsday should quit (will be deferred until convenient).
 */
#undef Sys_Quit
DENG_EXTERN_C void Sys_Quit(void)
{
    if(BusyMode_Active())
    {
        // The busy worker is running; we cannot just stop it abruptly.
        Sys_MessageBox2(MBT_WARNING, DOOMSDAY_NICENAME, "Cannot quit while in busy mode.",
                        "Try again later after the current operation has finished.", 0);
        return;
    }

    appShutdown = true;

    // It's time to stop the main loop.
    LegacyCore_Stop(DD_GameLoopExitCode());
}
