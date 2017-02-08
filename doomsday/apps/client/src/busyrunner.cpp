/** @file busyrunner.cpp Runs busy tasks in a background thread.
 * @ingroup base
 *
 * @authors Copyright © 2007-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "busyrunner.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "sys_system.h"
#include "network/net_main.h"
#include "clientapp.h"
#include "gl/gl_defer.h"
#include "ui/busyvisual.h"
#include "ui/inputsystem.h"
#include "ui/widgets/busywidget.h"
#include "ui/clientwindow.h"
#include "ui/progress.h"

#include <doomsday/doomsdayapp.h>
#include <de/concurrency.h>
#include <de/Config>
#include <de/GLInfo>
#include <de/Log>

#include <QEventLoop>

static bool animatedTransitionActive(int busyMode)
{
    return (!novideo && !isDedicated && !netGame && !(busyMode & BUSYF_STARTUP) &&
            rTransitionTics > 0 && (busyMode & BUSYF_TRANSITION));
}

static BusyMode &busy()
{
    return DoomsdayApp::app().busyMode();
}

DENG2_PIMPL_NOREF(BusyRunner)
, DENG2_OBSERVES(BusyMode, Beginning)
, DENG2_OBSERVES(BusyMode, End)
, DENG2_OBSERVES(BusyMode, TaskWillStart)
{
    QEventLoop *eventLoop = nullptr;

    thread_t    busyThread = nullptr;
    bool        busyDone = false;
    timespan_t  busyTime = 0;
    bool        busyWillAnimateTransition = false;
    bool        busyWasIgnoringInput = false;
    bool        fadeFromBlack = false;

    Impl()
    {
        busy().audienceForBeginning()     += this;
        busy().audienceForEnd()           += this;
        busy().audienceForTaskWillStart() += this;
    }

    ~Impl()
    {
        busy().setTaskRunner(nullptr);
    }

    void busyModeWillBegin(BusyTask &firstTask)
    {
        if (auto *fader = ClientWindow::main().contentFade())
        {
            fader->cancel();
        }

        // Are we doing a transition effect?
        busyWillAnimateTransition = animatedTransitionActive(firstTask.mode);
        if (busyWillAnimateTransition)
        {
            Con_TransitionConfigure();
        }

        fadeFromBlack        = (firstTask.mode & BUSYF_STARTUP) != 0;
        busyWasIgnoringInput = ClientApp::inputSystem().ignoreEvents();

        // Limit frame rate to 60, no point pushing it any faster while busy.
        ClientApp::app().loop().setRate(60);

        // Switch the window to busy mode UI.
        ClientWindow::main().setMode(ClientWindow::Busy);
    }

    void busyModeEnded()
    {
        DD_ResetTimer();

        // Discard input events so that any and all accumulated input events are ignored.
        ClientApp::inputSystem().ignoreEvents(busyWasIgnoringInput);

        // Back to unlimited frame rate.
        ClientApp::app().loop().setRate(0);

        // Switch the window to normal UI.
        ClientWindow::main().setMode(ClientWindow::Normal);
    }

    void busyTaskWillStart(BusyTask &task)
    {
        // Is the worker updating its progress?
        if (task.maxProgress > 0)
        {
            Con_InitProgress2(task.maxProgress, task.progressStart, task.progressEnd);
        }
    }

    /**
     * Exits the busy mode event loop. Called in the main thread, does not return
     * until the worker thread is stopped.
     */
    void exitEventLoop()
    {
        DENG_ASSERT_IN_MAIN_THREAD();
        DENG_ASSERT(eventLoop);

        busyDone = true;

        // Make sure the worker finishes before we continue.
        int result = Sys_WaitThread(busyThread, busy().endedWithError()? 100 : 5000, nullptr);
        busyThread = nullptr;

        eventLoop->exit(result);

        if (fadeFromBlack)
        {
            ClientWindow::main().fadeContent(ClientWindow::FadeFromBlack, 2);
        }
    }
};

static BusyRunner &busyRunner()
{
    return *static_cast<BusyRunner *>(busy().taskRunner());
}

BusyRunner::BusyRunner() : d(new Impl)
{
    busy().setTaskRunner(this);
}

/**
 * Callback that is called from the busy worker thread when it exists.
 *
 * @param status Exit status.
 */
static void busyWorkerTerminated(systhreadexitstatus_t status)
{
    DENG_ASSERT(busy().isActive());

    if (status == DENG_THREAD_STOPPED_WITH_EXCEPTION)
    {
        busy().abort("Uncaught exception from busy thread");
    }

    // This will tell the busy event loop to stop.
    busyRunner().finishTask();
}

BusyRunner::Result BusyRunner::runTask(BusyTask *task)
{
    // Let's get busy!
    BusyVisual_PrepareResources();

    de::ProgressWidget &prog = ClientWindow::main().busy().progress();
    prog.show((task->mode & BUSYF_PROGRESS_BAR) != 0);
    prog.setText(task->name);
    prog.setMode(task->mode & BUSYF_ACTIVITY? de::ProgressWidget::Indefinite :
                                              de::ProgressWidget::Ranged);

    // Start the busy worker thread, which will process the task in the
    // background while we keep the user occupied with nice animations.
    d->busyThread = Sys_StartThread(task->worker, task->workerData);
    Thread_SetCallback(d->busyThread, busyWorkerTerminated);

    DENG_ASSERT(!d->eventLoop);

    // Run a local event loop since the primary event loop is blocked while
    // we're busy. This event loop is able to handle window and input events
    // just like the primary loop.
    d->busyDone = false;
    d->eventLoop = new QEventLoop;
    Result result(true, d->eventLoop->exec());
    delete d->eventLoop;
    d->eventLoop = nullptr;

    ClientWindow::main().glActivate(); // after processing other events

#ifdef WIN32
    /*
     * Pretty big kludge here: it seems that with Qt 5.6-5.8 on Windows 10,
     * Nvidia drivers 376.33 (and many other versions), swap interval 
     * behaves as if it has been set to 2 (30 FPS) whenever a game is started. 
     * This could be due to some unknown behavior related to the secondary 
     * event loop used during busy mode. Toggling the swap interval off and 
     * back on appears to be a valid workaround.
     */
    de::Loop::timer(0.1, [] () {
        ClientWindow::main().glActivate();
        de::GLInfo::setSwapInterval(0);
        ClientWindow::main().glDone();
    });
    de::Loop::timer(0.5, [] () {
        ClientWindow::main().glActivate();
        if (de::Config::get().getb("window.main.vsync")) {
            de::GLInfo::setSwapInterval(1);
        }
        ClientWindow::main().glDone();
    });
#endif

    // Teardown.
    if (d->busyWillAnimateTransition)
    {
        Con_TransitionBegin();
    }

    // Make sure that any remaining deferred content gets uploaded.
    if (!(task->mode & BUSYF_NO_UPLOADS))
    {
        GL_ProcessDeferredTasks(0);
    }

    return result;
}

bool BusyRunner::isTransitionAnimated() const
{
    return d->busyWillAnimateTransition;
}

void BusyRunner::loop()
{
    BusyTask *const busyTask = busy().currentTask();
    if (!busyTask || !busy().isActive()) return;

    bool const canUpload = !(busyTask->mode & BUSYF_NO_UPLOADS);

    // Post and discard all input events.
    ClientApp::inputSystem().processEvents(0);
    ClientApp::inputSystem().processSharpEvents(0);

    if (canUpload)
    {
        ClientWindow::main().glActivate();

        // Any deferred content needs to get uploaded.
        GL_ProcessDeferredTasks(15);
    }

    if (!d->busyDone ||
        (canUpload && GL_DeferredTaskCount() > 0) ||
        !Con_IsProgressAnimationCompleted())
    {
        // Let's keep running the busy loop.
        return;
    }

    d->exitEventLoop();
}

void BusyRunner::finishTask()
{
    // When BusyMode_Loop() is called, it will quit the event loop as soon as possible.
    d->busyDone = true;
}

void BusyMode_Loop()
{
    static_cast<BusyRunner *>(busy().taskRunner())->loop();
}

bool BusyRunner::isWorkerThread(uint threadId) const
{
    if (!busy().isActive() || !d->busyThread) return false;

    return (Sys_ThreadId(d->busyThread) == threadId);
}

bool BusyRunner::inWorkerThread() const
{
    return isWorkerThread(Sys_CurrentThreadId());
}

#undef BusyMode_FreezeGameForBusyMode
void BusyMode_FreezeGameForBusyMode(void)
{
    // This is only possible from the main thread.
    if (ClientWindow::mainExists() &&
        DoomsdayApp::app().busyMode().taskRunner() &&
        de::App::inMainThread())
    {
        ClientWindow::main().busy().renderTransitionFrame();
    }
}

DENG_DECLARE_API(Busy) =
{
    { DE_API_BUSY },
    BusyMode_FreezeGameForBusyMode
};
