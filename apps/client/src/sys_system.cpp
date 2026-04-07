/** @file sys_system.cpp  Abstract interfaces for platform specific services.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <process.h>
#endif

//#ifdef MACOSX
//#  include <QDir>
//#endif
//#ifdef WIN32
//#  include <QSettings>
//#endif
#include <doomsday/doomsdayapp.h>
#include <doomsday/console/exec.h>

#include <de/legacy/concurrency.h>
#include <de/legacy/timer.h>
#include <de/app.h>
#include <de/packageloader.h>
#include <de/loop.h>

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "ui/inputsystem.h"
#  include "gl/gl_main.h"
#endif

#ifdef __SERVER__
#  include <de/textapp.h>
#endif

#include "dd_main.h"
#include "dd_loop.h"
#include "network/net_main.h"
#include "network/net_buf.h"
#include "ui/nativeui.h"
#include "api_base.h"

#include <signal.h>

#if defined(WIN32) && !defined(_DEBUG)
#  define DE_CATCH_SIGNALS
#endif

int novideo;                // if true, stay in text mode for debugging

#ifdef DE_CATCH_SIGNALS
/**
 * Borrowed from Lee Killough.
 */
static void C_DECL handler(int s)
{
    signal(s, SIG_IGN);  // Ignore future instances of this signal.

    App_Error(s==SIGSEGV ? "Segmentation Violation\n" :
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
void Sys_Init()
{
    de::Time begunAt;

    LOG_VERBOSE("Setting up platform state...");

    App_AudioSystem().initPlayback();

#ifdef DE_CATCH_SIGNALS
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

    LOG_NET_VERBOSE("Initializing Network subsystem...");
    N_Init();

    LOGDEV_VERBOSE("Sys_Init completed in %.2f seconds") << begunAt.since();
}

bool Sys_IsShuttingDown()
{
    return DoomsdayApp::app().isShuttingDown();
}

/**
 * Return to default system state.
 */
void Sys_Shutdown()
{
    // We are now shutting down.
    DoomsdayApp::app().setShuttingDown(true);

    // Time to unload *everything*.
    if(App_GameLoaded())
        Con_Execute(CMDS_DDAY, "unload", true, false);

    // Unload all loaded packages.
    de::App::packageLoader().unloadAll();

    // Deinitialize all subsystems.
    Net_Shutdown();

#ifdef __CLIENT__
    if (ClientApp::hasAudio())
    {
        // Let's shut down sound first, so Windows' HD-hogging doesn't jam
        // the MUS player (would produce horrible bursts of notes).
        App_AudioSystem().deinitPlayback();
    }
    GL_Shutdown();
    if(ClientApp::hasInput())
    {
        ClientApp::input().clearEvents();
    }
#endif

    App_ClearGames();
}

static int showCriticalMessage(const char *msg)
{
    // This is going to be the end, I'm afraid.
    de::Loop::get().stop();

#if defined (__CLIENT__)
    Sys_MessageBox(MBT_WARNING, DOOMSDAY_NICENAME, msg, 0);
#else
    de::warning("%s", msg);
#endif
    return 0;
}

int Sys_CriticalMessage(const char* msg)
{
    return showCriticalMessage(msg);
}

int Sys_CriticalMessagef(const char* format, ...)
{
    static const char *unknownMsg = "Unknown critical issue occured.";
    const size_t       BUF_SIZE   = 655365;
    const char *       msg;
    char *             buf = 0;
    va_list            args;
    int                result;

    if (format && format[0])
    {
        va_start(args, format);
        buf = reinterpret_cast<char *>(calloc(1, BUF_SIZE));
        dd_vsnprintf(buf, BUF_SIZE, format, args);
        msg = buf;
        va_end(args);
    }
    else
    {
        msg = unknownMsg;
    }

    result = showCriticalMessage(msg);

    if (buf) free(buf);
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

void Sys_HideMouseCursor()
{
#ifdef WIN32
    if(novideo) return;

    ShowCursor(FALSE);
#else
    // The cursor is controlled using Qt in Canvas.
#endif
}

/**
 * Called when Doomsday should quit (will be deferred until convenient).
 */
#undef Sys_Quit
DE_EXTERN_C void Sys_Quit(void)
{
    auto &app = DoomsdayApp::app();

    if (app.busyMode().isActive())
    {
        // The busy worker is running; we cannot just stop it abruptly.
        Sys_MessageBox2(MBT_WARNING, DOOMSDAY_NICENAME, "Cannot quit while in busy mode.",
                        "Try again later after the current operation has finished.", 0);
        return;
    }

    app.setShuttingDown(true);

    // Unload the game currently loaded. This will ensure it is shut down gracefully.
    if (!app.game().isNull())
    {
#ifdef __CLIENT__
        ClientWindow::main().glActivate();
        BusyMode_FreezeGameForBusyMode();
#endif
        app.changeGame(GameProfiles::null(), DD_ActivateGameWorker);
    }

#ifdef __CLIENT__
    if (ClientWindow::mainExists())
    {
        ClientWindow::main().fadeContent(ClientWindow::FadeToBlack, 0.1);
        de::Loop::timer(0.1, []() { DE_GUI_APP->quit(DD_GameLoopExitCode()); });
    }
    else
#endif
    {
#ifdef __CLIENT__
        // It's time to stop the main loop.
        DE_GUI_APP->quit(DD_GameLoopExitCode());
#else
        DE_TEXT_APP->quit(DD_GameLoopExitCode());
#endif
    }
}
