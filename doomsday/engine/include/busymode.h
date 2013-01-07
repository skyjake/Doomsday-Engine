/**
 * @file busymode.h
 * Busy Mode @ingroup core
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
 * @authors Copyright &copy; 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_CORE_BUSYMODE_H
#define LIBDENG_CORE_BUSYMODE_H

#include "dd_share.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @return  @c true if specified thread is the current busy task worker.
boolean BusyMode_IsWorkerThread(uint threadId);

/// @return  @c true if the current thread is the busy task worker.
boolean BusyMode_InWorkerThread(void);

/// @return  Current busy task, else @c NULL.
BusyTask* BusyMode_CurrentTask(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_CORE_BUSYMODE_H
