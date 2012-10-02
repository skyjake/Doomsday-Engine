/**
 * @file nativeui.h
 * Native GUI functionality. @ingroup base
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_NATIVEUI_H
#define LIBDENG_NATIVEUI_H

#ifdef __cplusplus
extern "C" {
#endif

// Message box types.
typedef enum {
    MBT_GENERIC,        ///< No icon.
    MBT_INFORMATION,
    MBT_QUESTION,
    MBT_WARNING,
    MBT_ERROR
} messageboxtype_t;

/**
 * Shows a native modal message dialog.
 *
 * @param type   Type of the dialog (which icon to show).
 * @param title  Title for the dialog.
 * @param msg    Message.
 * @param detailedMsg  Optional, possibly long extra content to show. Can be @c NULL.
 */
void Sys_MessageBox(messageboxtype_t type, const char* title, const char* msg, const char* detailedMsg);

void Sys_MessageBox2(messageboxtype_t type, const char* title, const char* msg, const char* informativeMsg, const char* detailedMsg);

int Sys_MessageBox3(messageboxtype_t type, const char* title, const char* msg, const char* informativeMsg, const char* detailedMsg,
                    const char** buttons);

void Sys_MessageBoxf(messageboxtype_t type, const char* title, const char* format, ...);

int Sys_MessageBoxWithButtons(messageboxtype_t type, const char* title, const char* msg,
                              const char* informativeMsg, const char** buttons);

/**
 * Shows a native modal message dialog. The "more detail" content is read from a file.
 *
 * @param type   Type of the dialog (which icon to show).
 * @param title  Title for the dialog.
 * @param msg    Message.
 * @param detailsFileName  Name of the file where to read details.
 */
void Sys_MessageBoxWithDetailsFromFile(messageboxtype_t type, const char* title, const char* msg,
                                       const char* informativeMsg, const char* detailsFileName);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_NATIVEUI_H
