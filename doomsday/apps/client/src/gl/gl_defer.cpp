/** @file gl_defer.cpp  Implementation of deferred GL tasks.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#define DE_DISABLE_DEFERRED_GL_API // using regular GL API calls

#include "de_platform.h"
#include "gl/gl_defer.h"

#include <de/legacy/concurrency.h>
#include <de/legacy/timer.h>
#include <doomsday/doomsdayapp.h>
#include <de/glinfo.h>
#include "dd_main.h"
#include "sys_system.h"  // Sys_Sleep(), novideo

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "gl/texturecontent.h"

#include <atomic>

using namespace de;

#define NUM_RESERVED_TEXTURENAMES  512

typedef enum {
    DEFERREDTASK_TYPES_FIRST = 0,

    // Higher-level or non-OpenGL operations:
    DTT_UPLOAD_TEXTURECONTENT = DEFERREDTASK_TYPES_FIRST,
    DTT_SET_VSYNC,

    // OpenGL API calls:
    DTT_FUNC_PTR_BEGIN,
        DTT_FUNC_PTR_E = DTT_FUNC_PTR_BEGIN,
        DTT_FUNC_PTR_EI,
        DTT_FUNC_PTR_EF,
        DTT_FUNC_PTR_EFV4,
        DTT_FUNC_PTR_UINT_ARRAY,
    DTT_FUNC_PTR_END,

    DEFERREDTASK_TYPES_LAST
} deferredtask_type_t;

#define VALID_DEFERREDTASK_TYPE(t)      ((t) >= DEFERREDTASK_TYPES_FIRST || (t) < DEFERREDTASK_TYPES_LAST)

typedef struct deferredtask_s {
    struct deferredtask_s* next;
    deferredtask_type_t type;
    void* data;
} deferredtask_t;

typedef struct apifunc_s {
    union {
        void (GL_CALL *ptr_e)(GLenum);
        void (GL_CALL *ptr_ei)(GLenum, GLint i);
        void (GL_CALL *ptr_ef)(GLenum, GLfloat f);
        void (GL_CALL *ptr_efv4)(GLenum, const GLfloat* fv4);
        void (GL_CALL *ptr_uintArray)(GLsizei, const GLuint*);
    } func;
    union {
        GLenum e;
        struct {
            GLenum e;
            GLint i;
        } ei;
        struct {
            GLenum e;
            GLfloat f;
        } ef;
        struct {
            GLenum e;
            GLfloat fv4[4];
        } efv4;
        struct {
            GLsizei count;
            GLuint* values;
        } uintArray;
    } param;
} apifunc_t;

static dd_bool deferredInited = false;
static mutex_t deferredMutex;
static DGLuint reservedTextureNames[NUM_RESERVED_TEXTURENAMES];
static std::atomic_int reservedCount;
static volatile deferredtask_t* deferredTaskFirst = NULL;
static volatile deferredtask_t* deferredTaskLast = NULL;

static deferredtask_t* allocTask(deferredtask_type_t type, void* data)
{
    deferredtask_t* dt = 0;

    assert(deferredInited);
    dt = (deferredtask_t*) malloc(sizeof(*dt));
    if(!dt)
    {
        App_Error("allocDeferredTask: Failed on allocation of %lu bytes.",
                  (unsigned long) sizeof(*dt));
        return 0; // Unreachable.
    }
    dt->type = type;
    dt->data = data;
    dt->next = 0;
    return dt;
}

static void enqueueTask(deferredtask_type_t type, void* data)
{
    deferredtask_t* d;

    if(!deferredInited)
        App_Error("enqueueTask: Deferred GL task system not initialized.");

    d = allocTask(type, data);
    Sys_Lock(deferredMutex);
    if(deferredTaskLast)
    {
        deferredTaskLast->next = d;
    }
    if(!deferredTaskFirst)
    {
        deferredTaskFirst = d;
    }
    deferredTaskLast = d;
    Sys_Unlock(deferredMutex);
}

static deferredtask_t* nextTask(void)
{
    assert(deferredInited);
    {
    deferredtask_t* d = NULL;
    if(NULL != (d = (deferredtask_t*) deferredTaskFirst))
    {
        deferredTaskFirst = d->next;
    }
    if(!deferredTaskFirst)
        deferredTaskLast = NULL;
    return d;
    }
}

DE_GL_DEFER1(e, GLenum e)
{
    DE_ASSERT(ptr != nullptr);

    apifunc_t* api = (apifunc_t *) M_Malloc(sizeof(apifunc_t));
    api->func.ptr_e = ptr;
    api->param.e = e;

    enqueueTask(DTT_FUNC_PTR_E, api);
}

DE_GL_DEFER2(i, GLenum e, GLint i)
{
    DE_ASSERT(ptr != nullptr);

    apifunc_t* api = (apifunc_t *) M_Malloc(sizeof(apifunc_t));
    api->func.ptr_ei = ptr;
    api->param.ei.e = e;
    api->param.ei.i = i;
    enqueueTask(DTT_FUNC_PTR_EI, api);
}

DE_GL_DEFER2(f, GLenum e, GLfloat f)
{
    DE_ASSERT(ptr != nullptr);

    apifunc_t* api = (apifunc_t *) M_Malloc(sizeof(apifunc_t));
    api->func.ptr_ef = ptr;
    api->param.ef.e = e;
    api->param.ef.f = f;
    enqueueTask(DTT_FUNC_PTR_EF, api);
}

DE_GL_DEFER2(fv4, GLenum e, const GLfloat* floatArrayWithFourValues)
{
    DE_ASSERT(ptr != nullptr);

    apifunc_t* api = (apifunc_t *) M_Malloc(sizeof(apifunc_t));
    api->func.ptr_efv4 = ptr;
    api->param.efv4.e = e;
    memcpy(api->param.efv4.fv4, floatArrayWithFourValues, sizeof(GLfloat) * 4);
    enqueueTask(DTT_FUNC_PTR_EFV4, api);
}

DE_GL_DEFER2(uintArray, GLsizei s, const GLuint* v)
{
    DE_ASSERT(ptr != nullptr);

    apifunc_t* api = (apifunc_t *) M_Malloc(sizeof(apifunc_t));
    api->func.ptr_uintArray = ptr;
    api->param.uintArray.count = s;
    api->param.uintArray.values = (GLuint *) M_MemDup(v, sizeof(GLuint) * s);
    enqueueTask(DTT_FUNC_PTR_UINT_ARRAY, api);
}

static void processTask(deferredtask_t *task)
{
    apifunc_t *api = (apifunc_t *) task->data;

    switch(task->type)
    {
    case DTT_UPLOAD_TEXTURECONTENT:
        DE_ASSERT(task->data);
        GL_UploadTextureContent(*reinterpret_cast<texturecontent_t *>(task->data),
                                gfx::Immediate);
        break;

    case DTT_SET_VSYNC:
        GL_SetVSync(*(dd_bool*)task->data);
        break;

    case DTT_FUNC_PTR_E:
        api->func.ptr_e(api->param.e);
        break;

    case DTT_FUNC_PTR_EI:
        api->func.ptr_ei(api->param.ei.e, api->param.ei.i);
        break;

    case DTT_FUNC_PTR_EF:
        api->func.ptr_ef(api->param.ef.e, api->param.ef.f);
        break;

    case DTT_FUNC_PTR_EFV4:
        api->func.ptr_efv4(api->param.efv4.e, api->param.efv4.fv4);
        break;

    case DTT_FUNC_PTR_UINT_ARRAY:
        api->func.ptr_uintArray(api->param.uintArray.count, api->param.uintArray.values);
        break;

    default:
        App_Error("Unknown deferred GL task type %i.", (int) task->type);
        break;
    }
}

static void destroyTaskData(deferredtask_t* d)
{
    apifunc_t* api = (apifunc_t*) d->data;

    assert(VALID_DEFERREDTASK_TYPE(d->type));

    // Free data allocated for the task.
    switch(d->type)
    {
    case DTT_UPLOAD_TEXTURECONTENT:
        GL_DestroyTextureContent((texturecontent_s *) d->data);
        break;

    case DTT_SET_VSYNC:
        M_Free(d->data);
        break;

    case DTT_FUNC_PTR_UINT_ARRAY:
        M_Free(api->param.uintArray.values);
        break;

    default:
        break;
    }

    if(d->type >= DTT_FUNC_PTR_BEGIN && d->type < DTT_FUNC_PTR_END)
    {
        // Free the apifunc_t.
        free(d->data);
    }
}

static void destroyTask(deferredtask_t* d)
{
    assert(deferredInited && d);
    destroyTaskData(d);
    free(d);
}

void GL_InitDeferredTask(void)
{
    if(deferredInited)
        return; // Been here already...

    deferredInited = true;
    deferredMutex = Sys_CreateMutex("DGLDeferredMutex");
    GL_ReserveNames();
}

void GL_ShutdownDeferredTask(void)
{
    if(!deferredInited)
        return;

    GL_ReleaseReservedNames();
    GL_PurgeDeferredTasks();

    Sys_DestroyMutex(deferredMutex);
    deferredMutex = 0;

    deferredInited = false;
}

int GL_DeferredTaskCount(void)
{
    deferredtask_t* i = 0;
    int count = 0;

    if(!deferredInited)
        return 0;

    Sys_Lock(deferredMutex);
    for(i = (deferredtask_t*) deferredTaskFirst; i; i = i->next, ++count) {}
    Sys_Unlock(deferredMutex);
    return count;
}

void GL_ReserveNames(void)
{
    if(!deferredInited)
        return; // Just ignore.

    Sys_Lock(deferredMutex);
    if(reservedCount < NUM_RESERVED_TEXTURENAMES)
    {
        DE_ASSERT_IN_MAIN_THREAD();
        DE_ASSERT_GL_CONTEXT_ACTIVE();

        glGenTextures(NUM_RESERVED_TEXTURENAMES - reservedCount,
            (GLuint*) &reservedTextureNames[reservedCount]);
        reservedCount = NUM_RESERVED_TEXTURENAMES;
    }
    Sys_Unlock(deferredMutex);
}

void GL_ReleaseReservedNames(void)
{
    if(!deferredInited)
        return; // Just ignore.

    DE_ASSERT_IN_MAIN_THREAD(); // not deferring here
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    Sys_Lock(deferredMutex);
    glDeleteTextures(reservedCount, (const GLuint*) reservedTextureNames);
    memset(reservedTextureNames, 0, sizeof(reservedTextureNames));
    reservedCount = 0;
    Sys_Unlock(deferredMutex);
}

DGLuint GL_GetReservedTextureName(void)
{
    DGLuint name;

    LOG_AS("GL_GetReservedTextureName");

    DE_ASSERT(deferredInited);

    Sys_Lock(deferredMutex);

    if(!reservedCount)
    {
        Sys_Unlock(deferredMutex);
        while(reservedCount == 0)
        {
            // Wait for someone to refill the names buffer.
            LOGDEV_GL_MSG("Sleeping until new names available");
            Sys_Sleep(5);
        }
        Sys_Lock(deferredMutex);
    }

    name = reservedTextureNames[0];
    memmove(reservedTextureNames, reservedTextureNames + 1,
            (NUM_RESERVED_TEXTURENAMES - 1) * sizeof(DGLuint));
    reservedCount--;

    Sys_Unlock(deferredMutex);

    return name;
}

void GL_PurgeDeferredTasks(void)
{
    if(!deferredInited)
        return;

    Sys_Lock(deferredMutex);
    { deferredtask_t* d;
    while(NULL != (d = nextTask()))
        destroyTask(d);
    }
    Sys_Unlock(deferredMutex);
}

static deferredtask_t* GL_NextDeferredTask(void)
{
    deferredtask_t* d = NULL;
    if(!deferredInited)
    {
        return NULL;
    }
    Sys_Lock(deferredMutex);
    d = nextTask();
    Sys_Unlock(deferredMutex);
    return d;
}

void GL_ProcessDeferredTasks(uint timeOutMilliSeconds)
{
    deferredtask_t* d;
    uint startTime;

    if(novideo || !deferredInited) return;

    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    startTime = Timer_RealMilliseconds();

    // We'll reserve names multiple times, because the worker thread may be
    // needing new texture names while we are uploading.
    GL_ReserveNames();

    while((!timeOutMilliSeconds ||
           Timer_RealMilliseconds() - startTime < timeOutMilliSeconds) &&
          (d = GL_NextDeferredTask()) != NULL)
    {
        processTask(d);
        destroyTask(d);
        GL_ReserveNames();
    }

    GL_ReserveNames();
}

gfx::UploadMethod GL_ChooseUploadMethod(const struct texturecontent_s *content)
{
    DE_ASSERT(content != 0);

    // Must the operation be carried out immediately?
    if((content->flags & TXCF_NEVER_DEFER) || !DoomsdayApp::busyMode().isActive())
    {
        return gfx::Immediate;
    }
    // We can defer.
    return gfx::Deferred;
}

void GL_DeferTextureUpload(const struct texturecontent_s *content)
{
    if(novideo) return;

    // Defer this operation. Need to make a copy.
    enqueueTask(DTT_UPLOAD_TEXTURECONTENT, GL_ConstructTextureContentCopy(content));
}

void GL_DeferSetVSync(dd_bool enableVSync)
{
    enqueueTask(DTT_SET_VSYNC, M_MemDup(&enableVSync, sizeof(enableVSync)));
}
