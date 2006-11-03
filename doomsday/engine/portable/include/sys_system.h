/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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

/*
 * sys_system.h: OS Specific Services Subsystem
 */

#ifndef __DOOMSDAY_SYSTEM_H__
#define __DOOMSDAY_SYSTEM_H__

#include "dd_types.h"
#include <SDL_thread.h>

typedef int     (*systhreadfunc_t) (void *parm);

extern boolean  novideo;

//extern int systics;

void            Sys_Init(void);
void            Sys_Shutdown(void);
void            Sys_Quit(void);
int             Sys_CriticalMessage(char *msg);
void            Sys_Sleep(int millisecs);
void            Sys_ShowCursor(boolean show);
void            Sys_HideMouse(void);
void            Sys_MessageBox(const char *msg, boolean iserror);
void            Sys_OpenTextEditor(const char *filename);
void            Sys_ShowWindow(boolean hide);

// Threads:
SDL_Thread *    Sys_StartThread(systhreadfunc_t startpos, void *parm );
void            Sys_SuspendThread(int handle, boolean dopause);
int             Sys_WaitThread(SDL_Thread *handle);

intptr_t        Sys_CreateMutex(const char *name);	// returns the mutex handle
void            Sys_DestroyMutex(intptr_t mutexHandle);
void            Sys_Lock(intptr_t mutexHandle);
void            Sys_Unlock(intptr_t mutexHandle);

long		Sem_Create(uint32_t initialValue);	// returns handle
void            Sem_Destroy(long semaphore);
void            Sem_P(long semaphore);
void            Sem_V(long semaphore);

#endif
