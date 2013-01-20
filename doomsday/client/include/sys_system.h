/** @file sys_system.h
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * OS Specific Services Subsystem.
 */

#ifndef LIBDENG_FILESYS_SYSTEM_H
#define LIBDENG_FILESYS_SYSTEM_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int novideo;

void Sys_Init(void);
void Sys_Shutdown(void);

/// @return  @c true if shutdown is in progress.
boolean Sys_IsShuttingDown(void);

int Sys_CriticalMessage(const char* msg);
int Sys_CriticalMessagef(const char* format, ...) PRINTF_F(1,2);

void Sys_Sleep(int millisecs);

/**
 * Blocks the thread for a very short period of time. If attempting to wait
 * until a time in the past (or for more than 50 ms), returns immediately.
 *
 * @param realTimeMs  Block until this time is reached.
 *
 * @note Longer waits should use Sys_Sleep() -- this is a busy wait.
 */
void Sys_BlockUntilRealTime(uint realTimeMs);

void Sys_ShowCursor(boolean show);
void Sys_HideMouse(void);
#if 0
void Sys_MessageBox(const char* msg, boolean iserror);
void Sys_OpenTextEditor(const char* filename);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_SYSTEM_H */
