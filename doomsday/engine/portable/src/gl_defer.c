/**\file gl_defer.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * Deferred GL Tasks.
 */

#ifdef UNIX
#   include "de_platform.h"
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"

#include "texturecontent.h"

#define NUM_RESERVED_TEXTURENAMES  512

typedef struct deferredtask_s {
    struct deferredtask_s* next;
    deferredtask_type_t type;
    void* data;
} deferredtask_t;

deferredtask_t* GL_GetNextDeferredTask(void);
void GL_DestroyDeferredTask(deferredtask_t* d);

static boolean inited = false;

static mutex_t deferredMutex;
static DGLuint reservedTextureNames[NUM_RESERVED_TEXTURENAMES];
static volatile int reservedCount = 0;
static volatile deferredtask_t* deferredTaskFirst = NULL;
static volatile deferredtask_t* deferredTaskLast = NULL;

static deferredtask_t* allocDeferredTask(deferredtask_type_t type, void* data)
{
    deferredtask_t* dt;
    if(NULL == (dt = (deferredtask_t*)malloc(sizeof(*dt))))
    {
        Con_Error("allocDeferredTask: Failed on allocation of %lu bytes.", sizeof(*dt));
        return 0; // Unreachable.
    }
    dt->type = type;
    dt->data = data;
    dt->next = 0;
    return dt;
}

void GL_InitDeferredTask(void)
{
    if(inited)
        return; // Been here already...

    inited = true;
    deferredMutex = Sys_CreateMutex("DGLDeferredMutex");
    GL_ReserveNames();
}

void GL_ShutdownDeferredTask(void)
{
    deferredtask_t* d;

    if(!inited)
        return;

    GL_ReleaseReservedNames();

    while((d = GL_GetNextDeferredTask()) != NULL)
    {
        GL_DestroyDeferredTask(d);
    }

    Sys_DestroyMutex(deferredMutex);
    deferredMutex = 0;
    reservedCount = 0;
}

int GL_GetDeferredTaskCount(void)
{
    deferredtask_t* i = 0;
    int count = 0;

    if(!inited)
        return 0;

    Sys_Lock(deferredMutex);
    for(i = (deferredtask_t*) deferredTaskFirst; i; i = i->next, ++count);
    Sys_Unlock(deferredMutex);
    return count;
}

deferredtask_t* GL_GetNextDeferredTask(void)
{
    deferredtask_t* d = NULL;

    if(!inited)
        return NULL;

    Sys_Lock(deferredMutex);
    if((d = (deferredtask_t*) deferredTaskFirst) != NULL)
    {
        deferredTaskFirst = d->next;
    }

    if(!deferredTaskFirst) deferredTaskLast = NULL;
    Sys_Unlock(deferredMutex);
    return d;
}

void GL_DestroyDeferredTask(deferredtask_t* d)
{
    assert(d);
    switch(d->type)
    {
    case DTT_UPLOAD_TEXTURECONTENT:
        GL_DestroyTextureContent(d->data);
        break;
    default:
        Con_Error("GL_DestroyDeferredTask: Unknown task type %i.", (int) d->type);
        break; // Unreachable.
    }
    free(d);
}

void GL_ReserveNames(void)
{
    if(!inited)
        return; // Just ignore.

    Sys_Lock(deferredMutex);
    if(reservedCount < NUM_RESERVED_TEXTURENAMES)
    {
        glGenTextures(NUM_RESERVED_TEXTURENAMES - reservedCount,
            (GLuint*) &reservedTextureNames[reservedCount]);
        reservedCount = NUM_RESERVED_TEXTURENAMES;
    }
    Sys_Unlock(deferredMutex);
}

void GL_ReleaseReservedNames(void)
{
    if(!inited)
        return; // Just ignore.

    Sys_Lock(deferredMutex);
    glDeleteTextures(reservedCount, (const GLuint*) reservedTextureNames);
    memset(reservedTextureNames, 0, sizeof(reservedTextureNames));
    reservedCount = 0;
    Sys_Unlock(deferredMutex);
}

DGLuint GL_GetReservedTextureName(void)
{
    DGLuint name;

    if(!inited)
        Con_Error("GL_GetReservedTextureName: Deferred GL task system not initialized.");

    Sys_Lock(deferredMutex);

    if(!reservedCount)
    {
        Sys_Unlock(deferredMutex);
        while(reservedCount == 0)
        {
            // Wait for someone to refill the names buffer.
            Con_Message("GL_GetReservedTextureName: Sleeping until new names available.\n");
            Sys_Sleep(5);
        }
        Sys_Lock(deferredMutex);
    }

    name = reservedTextureNames[0];
    memmove(reservedTextureNames, reservedTextureNames + 1, (NUM_RESERVED_TEXTURENAMES - 1) * sizeof(DGLuint));
    reservedCount--;

    Sys_Unlock(deferredMutex);

    return name;
}

void GL_EnqueueDeferredTask(deferredtask_type_t type, void* data)
{
    deferredtask_t* d;

    if(!inited)
        Con_Error("GL_EnqueueDeferredTask: Deferred GL task system not initialized.");

    d = allocDeferredTask(type, data);
    Sys_Lock(deferredMutex);
    if(deferredTaskLast)
        deferredTaskLast->next = d;
    if(!deferredTaskFirst)
        deferredTaskFirst = d;
    deferredTaskLast = d;
    Sys_Unlock(deferredMutex);
}

void GL_RunDeferredTasks(uint timeOutMilliSeconds)
{
    deferredtask_t* d;
    uint startTime;

    if(!inited)
        Con_Error("GL_RunDeferredTasks: Deferred GL task system not initialized.");

    startTime = Sys_GetRealTime();

    // We'll reserve names multiple times, because the worker thread may be
    // needing new texture names while we are uploading.
    GL_ReserveNames();
    while((!timeOutMilliSeconds ||
           Sys_GetRealTime() - startTime < timeOutMilliSeconds) &&
          (d = GL_GetNextDeferredTask()) != NULL)
    {
        switch(d->type)
        {
        case DTT_UPLOAD_TEXTURECONTENT:
            GL_UploadTextureContent(d->data);
            break;
        default:
            Con_Error("GL_RunDeferredTasks: Unknown task type %i.", (int) d->type);
            break; // Unreachable.
        }
        GL_DestroyDeferredTask(d);
        GL_ReserveNames();
    }
    GL_ReserveNames();
}
