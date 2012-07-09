/**
 * @file busytask.h
 * Busy Mode task @ingroup core
 *
 * @authors Copyright &copy; 2007-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_BUSYTASK_H
#define LIBDENG_BUSYTASK_H

#ifdef __cplusplus
extern "C" {
#endif

/// Busy mode worker function.
typedef int (C_DECL *busyworkerfunc_t) (void* parm);

/// POD structure for defining a task processable in busy mode.
typedef struct {
    busyworkerfunc_t worker; ///< Worker thread that does processing while in busy mode.
    void* workerData; ///< Data context for the worker thread.

    int mode; ///< Busy mode flags @ref busyModeFlags
    const char* name; ///< Optional task name (drawn with the progress bar).

    /// Used with task lists:
    int maxProgress;
    float progressStart;
    float progressEnd;

    // Internal state:
    timespan_t _startTime;
    boolean _willAnimateTransition;
    boolean _wasIgnoringInput;
} BusyTask;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_BUSYTASK_H */
