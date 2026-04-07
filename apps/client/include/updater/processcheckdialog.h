/**
 * @file processcheckdialog.h
 * Dialog for checking running processes on Windows. @ingroup updater
 *
 * Windows cannot overwrite files that are in use. Thus the updater needs
 * to ensure that all Snowberry is shut down before starting an update.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_PROCESSCHECKDIALOG_H
#define DE_CLIENT_PROCESSCHECKDIALOG_H

#include <de/legacy/types.h>

// This is only for Windows.
#ifdef WIN32

/**
 * Asks the user to stop a process if it is found to be running.
 *
 * @param processName  Name of the process to check for (e.g., "snowberry.exe").
 * @param message      Message to display to the user if the process is
 *                     running. Should describe why the process needs to be
 *                     stopped.
 *
 * @return  @c true, if the process has been stopped. Otherwise @c false,
 * the process is still running.
 */
dd_bool Updater_AskToStopProcess(const char *processName, const char *message);

#endif // WIN32

#endif // DE_CLIENT_PROCESSCHECKDIALOG_H
