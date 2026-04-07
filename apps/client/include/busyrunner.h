/**
 * @file busyrunner.h  Runs busy tasks in a background thread. @ingroup core
 *
 * The Busy Mode is intended for long-running tasks that would otherwise block
 * the main engine (UI) thread. During busy mode, a progress screen is shown by
 * the main thread while a background thread works on a long operation. The
 * normal Doomsday UI cannot be interacted with while the task is running. The
 * busy mode can be configured to show a progress bar, the console output,
 * and/or a description of the task being carried out.
 *
 * @todo Refactor: Remove the busy mode loop to prevent the app UI from
 * freezing while busy mode is running. Instead, busy mode should run in the
 * regular application event loop. During busy mode, the game loop callback
 * should not be called.
 *
 * @authors Copyright &copy; 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2009-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_BUSYRUNNER_H
#define CLIENT_BUSYRUNNER_H

#include <doomsday/busymode.h>

/**
 * Runs busy tasks in a background thread.
 */
class BusyRunner : public BusyMode::ITaskRunner
{
public:
    enum DeferredResult { AllTasksCompleted, TasksPending };

    /// Called during the busy loop from the main thread.
    /// @return @c true, if all deferred tasks have been completed. @c false, if
    /// deferred tasks still remain afterwards.
    DE_AUDIENCE(DeferredGLTask, DeferredResult performDeferredGLTask())

public:
    BusyRunner();

    bool isTransitionAnimated() const;
    bool inWorkerThread() const;

    /**
     * Runs an event loop in the main thread during busy mode. This keeps the UI
     * responsive while the background task runs the busy task.
     */
    Result runTask(BusyTask *task) override;

    void loop();

    /*
     * Called when the background thread has exited.
     */
    //void finishTask();

private:
    DE_PRIVATE(d)
};

/**
 * The busy loop callback function. Called periodically in the main (UI) thread
 * while the busy worker is running.
 */
void BusyMode_Loop();

#endif // CLIENT_BUSYRUNNER_H
