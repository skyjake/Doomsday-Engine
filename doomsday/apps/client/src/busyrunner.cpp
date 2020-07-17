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
#include "ui/home/homewidget.h"
#include "ui/clientwindow.h"
#include "ui/progress.h"
#include "clientapp.h"

#include <doomsday/doomsdayapp.h>
#include <de/config.h>
#include <de/eventloop.h>
#include <de/glinfo.h>
#include <de/log.h>
#include <de/loop.h>
#include <de/thread.h>

#include <atomic>

using namespace de;

static bool animatedTransitionActive(int busyMode)
{
    return (!novideo && !netState.netGame && !(busyMode & BUSYF_STARTUP) && rTransitionTics > 0 &&
            (busyMode & BUSYF_TRANSITION));
}

static BusyMode &busy()
{
    return DoomsdayApp::app().busyMode();
}

DE_PIMPL_NOREF(BusyRunner)
, DE_OBSERVES(BusyMode, Beginning)
, DE_OBSERVES(BusyMode, End)
, DE_OBSERVES(BusyMode, TaskWillStart)
//, DE_OBSERVES(BusyMode, Abort)
, DE_OBSERVES(Thread, Finished)
{
    class WorkThread : public Thread
    {
        BusyTask *task;
        
    public:
        int    result = 0;
        String abortMsg;
        
    public:
        WorkThread(BusyTask *task) : task(task)
        {}
        
        void run() override
        {
            try
            {
                result = task->worker(task->workerData);
            }
            catch (const Error &er)
            {
                abortMsg = er.asText();
            }
        }
    };
    
    EventLoop * eventLoop = nullptr;
    const BusyTask *task = nullptr;
    std::unique_ptr<WorkThread> busyThread;
    timespan_t  busyTime = 0;
    bool        busyWillAnimateTransition = false;
    bool        busyWasIgnoringInput = false;
    bool        fadeFromBlack = false;

    Impl()
    {
        busy().audienceForBeginning()     += this;
        busy().audienceForEnd()           += this;
        busy().audienceForTaskWillStart() += this;
//        busy().audienceForAbort()         += this;
    }

    ~Impl() override
    {
        busy().setTaskRunner(nullptr);
    }

    bool isTaskDone() const
    {
        return busyThread == nullptr || busyThread->isFinished();
    }
    
    void busyModeWillBegin(BusyTask &firstTask) override
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
        busyWasIgnoringInput = ClientApp::input().ignoreEvents();

        // Limit frame rate to 60, no point pushing it any faster while busy.
        ClientApp::app().loop().setRate(60);

        // Switch the window to busy mode UI.
        ClientWindow::main().setMode(ClientWindow::Busy);
    }

    void busyModeEnded() override
    {
        DE_ASSERT(!eventLoop);
        DE_ASSERT(isTaskDone());
        
        DD_ResetTimer();

        // Discard input events so that any and all accumulated input events are ignored.
        ClientApp::input().ignoreEvents(busyWasIgnoringInput);

        // Back to unlimited frame rate.
        ClientApp::app().loop().setRate(0);

        // Switch the window to normal UI.
        ClientWindow::main().setMode(ClientWindow::Normal);
    }

    void busyTaskWillStart(BusyTask &task) override
    {
        // Is the worker updating its progress?
        if (task.maxProgress > 0)
        {
            Con_InitProgress2(task.maxProgress, task.progressStart, task.progressEnd);
        }
    }

#if 0
    void busyModeAborted(const String &) override
    {
        Loop::mainCall([this] ()
        {
            if (busyThread)
            {
                debug("[BusyRunner] Killing the worker!");
                //Thread_KillAbnormally(busyThread);
                busyThread->terminate();
            }
            exitEventLoop();
        });
    }
#endif

    void threadFinished(Thread &) override
    {
        LOG_MSG("Busy work thread has finished");
        Loop::mainCall([this]()
        {
            if (busyThread->abortMsg)
            {
                busy().abort(busyThread->abortMsg);
            }
            exitEventLoop();
        });
    }
    
    /**
     * Exits the busy mode event loop. Called in the main thread, does not return
     * until the worker thread is stopped.
     */
    void exitEventLoop()
    {
        DE_ASSERT_IN_MAIN_THREAD();
        DE_ASSERT(eventLoop);

        // Make sure the worker finishes before we continue.
        //int result = busyThread->
//        busyThread = nullptr;

        eventLoop->quit(busyThread->result);

        if (fadeFromBlack)
        {
            ClientWindow::main().fadeContent(ClientWindow::FadeFromBlack, 2.0);
        }
    }

    DE_PIMPL_AUDIENCE(DeferredGLTask)
};

DE_AUDIENCE_METHOD(BusyRunner, DeferredGLTask)

//static BusyRunner &busyRunner()
//{
//    return *static_cast<BusyRunner *>(busy().taskRunner());
//}

BusyRunner::BusyRunner() : d(new Impl)
{
    busy().setTaskRunner(this);
}

/**
 * Callback that is called from the busy worker thread when it exists.
 *
 * @param status Exit status.
 */
/*static void busyWorkerTerminated(systhreadexitstatus_t status)
{
    DE_ASSERT(busy().isActive());

    if (status == DE_THREAD_STOPPED_WITH_EXCEPTION)
    {
        busy().abort("Uncaught exception from busy thread");
    }

    // This will tell the busy event loop to stop.
    busyRunner().finishTask();
}*/

BusyRunner::Result BusyRunner::runTask(BusyTask *task)
{
    // Let's get busy!
    BusyVisual_PrepareResources();

    ProgressWidget &prog = ClientWindow::main().busy().progress();
    prog.show((task->mode & BUSYF_PROGRESS_BAR) != 0);
    prog.setText(task->name);
    prog.setMode(task->mode & BUSYF_ACTIVITY? ProgressWidget::Indefinite :
                                              ProgressWidget::Ranged);

    DE_ASSERT(!d->eventLoop);
    d->eventLoop = new EventLoop;

    // Start the busy worker thread, which will process the task in the
    // background while we keep the user occupied with nice animations.
    //d->busyThread = Sys_StartThread(task->worker, task->workerData, busyWorkerTerminated);
    d->busyThread.reset(new Impl::WorkThread(task));
    d->busyThread->audienceForFinished() += d;
    d->busyThread->start();

    // Run a local event loop since the primary event loop is blocked while
    // we're busy. This event loop is able to handle window and input events
    // just like the primary loop.
    Result result(true, d->eventLoop->exec());
    delete d->eventLoop;
    d->eventLoop = nullptr;

    GLWindow::glActivateMain(); // after processing other events

#if 0
#  ifdef WIN32
    /*
     * Pretty big kludge here: it seems that with Qt 5.6-5.8 on Windows 10,
     * Nvidia drivers 376.33 (and many other versions), swap interval
     * behaves as if it has been set to 2 (30 FPS) whenever a game is started.
     * This could be due to some unknown behavior related to the secondary
     * event loop used during busy mode. Toggling the swap interval off and
     * back on appears to be a valid workaround.
     */
    Loop::timer(0.1, []() {
        ClientWindow::main().glActivate();
        GLInfo::setSwapInterval(0);
        ClientWindow::main().glDone();
    });
    Loop::timer(0.5, []() {
        ClientWindow::main().glActivate();
        if (Config::get().getb("window.main.vsync")) {
            GLInfo::setSwapInterval(1);
        }
        ClientWindow::main().glDone();
    });
#  endif
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

    const bool canUpload = !(busyTask->mode & BUSYF_NO_UPLOADS);

    // Post and discard all input events.
    ClientApp::input().processEvents(0);
    ClientApp::input().processSharpEvents(0);

    ClientWindow::main().glActivate();

    // Only perform pending tasks after Home has been hidden, as otherwise there
    // might be nasty stutters in the window refresh if one of the pending tasks
    // blocks the thread for a while.
    bool pendingRemain = false;
    if (ClientWindow::main().home().isHidden())
    {
        DE_NOTIFY(DeferredGLTask, i)
        {
            if (i->performDeferredGLTask() == TasksPending)
            {
                pendingRemain = true;
            }
        }
    }

    if (canUpload)
    {
        // Any deferred content needs to get uploaded.
        GL_ProcessDeferredTasks(15);
    }

//    if (!d->isTaskDone()
//        || pendingRemain
//        || (canUpload && GL_DeferredTaskCount() > 0)
//        || !Con_IsProgressAnimationCompleted())
//    {
//        // Let's keep running the busy loop.
//        return;
//    }
//
//    d->exitEventLoop();
}

//void BusyRunner::finishTask()
//{
//    // When BusyMode_Loop() is called, it will quit the event loop as soon as possible.
//    //d->busyDone = true;
//}

void BusyMode_Loop()
{
    static_cast<BusyRunner *>(busy().taskRunner())->loop();
}

bool BusyRunner::inWorkerThread() const
{
    return d->busyThread && Thread::currentThread() == d->busyThread.get();
}

#undef BusyMode_FreezeGameForBusyMode
void BusyMode_FreezeGameForBusyMode(void)
{
    // This is only possible from the main thread.
    if (ClientWindow::mainExists() &&
        DoomsdayApp::app().busyMode().taskRunner() &&
        App::inMainThread())
    {
#if !defined (DE_MOBILE)
        ClientWindow::main().busy().renderTransitionFrame();
#endif
    }
}

DE_DECLARE_API(Busy) =
{
    { DE_API_BUSY },
    BusyMode_FreezeGameForBusyMode
};
