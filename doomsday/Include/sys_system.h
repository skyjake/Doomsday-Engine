/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sys_system.h: OS Specific Services Subsystem
 */

#ifndef __DOOMSDAY_SYSTEM_H__
#define __DOOMSDAY_SYSTEM_H__

#include "dd_types.h"

typedef int (*systhreadfunc_t)(void *parm);
typedef int semaphore_t;

extern boolean novideo;
//extern int systics;

void	Sys_Init(void);
void	Sys_Shutdown(void);
void	Sys_Quit(void);
int		Sys_CriticalMessage(char *msg);
void	Sys_Sleep(int millisecs);
void	Sys_ShowCursor(boolean show);
void	Sys_HideMouse(void);
void	Sys_MessageBox(const char *msg, boolean iserror);
void	Sys_OpenTextEditor(const char *filename);
void	Sys_ShowWindow(boolean hide);

// Threads:
int		Sys_StartThread(systhreadfunc_t startpos, void *parm, int priority);
void	Sys_SuspendThread(int handle, boolean dopause);
int		Sys_WaitThread(int handle);

int 	Sys_CreateMutex(const char *name); // returns the mutex handle
void 	Sys_DestroyMutex(int mutexHandle);
void 	Sys_Lock(int mutexHandle); 
void 	Sys_Unlock(int mutexHandle);

semaphore_t Sem_Create(unsigned int initialValue); // returns handle
void	Sem_Destroy(semaphore_t semaphore);
void	Sem_P(semaphore_t semaphore);
void	Sem_V(semaphore_t semaphore);

#endif
