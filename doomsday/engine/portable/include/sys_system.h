/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * sys_system.h: OS Specific Services Subsystem
 */

#ifndef __DOOMSDAY_SYSTEM_H__
#define __DOOMSDAY_SYSTEM_H__

#include "dd_types.h"

typedef void*   thread_t;
typedef intptr_t mutex_t;
typedef intptr_t sem_t;
typedef int     (C_DECL *systhreadfunc_t) (void* parm);

extern int  novideo;

void            Sys_Init(void);
void            Sys_Shutdown(void);
void            Sys_Quit(void);
int             Sys_CriticalMessage(char* msg);
void            Sys_Sleep(int millisecs);
void            Sys_ShowCursor(boolean show);
void            Sys_HideMouse(void);
void            Sys_MessageBox(const char* msg, boolean iserror);
void            Sys_OpenTextEditor(const char* filename);

thread_t        Sys_StartThread(systhreadfunc_t startpos, void* parm);
void            Sys_SuspendThread(thread_t handle, boolean dopause);
int             Sys_WaitThread(thread_t handle);
uint            Sys_ThreadID(void);

mutex_t         Sys_CreateMutex(const char* name);
void            Sys_DestroyMutex(mutex_t mutexHandle);
void            Sys_Lock(mutex_t mutexHandle);
void            Sys_Unlock(mutex_t mutexHandle);

sem_t           Sem_Create(uint32_t initialValue);  // returns handle
void            Sem_Destroy(sem_t semaphore);
void            Sem_P(sem_t semaphore);
void            Sem_V(sem_t semaphore);

#endif
