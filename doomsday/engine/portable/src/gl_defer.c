/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
 * gl_defer.c: Deferred GL Tasks
 */

// HEADER FILES ------------------------------------------------------------

#ifdef UNIX
#   include "de_platform.h"
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define NUM_RESERVED_NAMES  512

// TYPES -------------------------------------------------------------------

typedef struct deferred_s {
    texturecontent_t content;
    struct deferred_s* next;
} deferred_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

void        GL_ReserveNames(void);
deferred_t* GL_GetNextDeferred(void);
void        GL_DestroyDeferred(deferred_t* d);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mutex_t deferredMutex;
static DGLuint reservedNames[NUM_RESERVED_NAMES];
static volatile int reservedCount = 0;

static volatile deferred_t* deferredContentFirst = NULL;
static volatile deferred_t* deferredContentLast = NULL;

// CODE --------------------------------------------------------------------

void GL_InitDeferred(void)
{
    deferredMutex = Sys_CreateMutex("DGLDeferredMutex");
    GL_ReserveNames();
}

void GL_ShutdownDeferred(void)
{
    deferred_t *d;
    
    while((d = GL_GetNextDeferred()) != NULL)
    {    
        GL_DestroyDeferred(d);
    }
    
    Sys_DestroyMutex(deferredMutex);
    deferredMutex = 0;
    reservedCount = 0;
}

int GL_GetDeferredCount(void)
{
    int count = 0;
    deferred_t* i = 0;
    
    Sys_Lock(deferredMutex);
    for(i = (deferred_t*) deferredContentFirst; i; i = i->next, ++count);
    Sys_Unlock(deferredMutex);
    return count;
}

deferred_t* GL_GetNextDeferred(void)
{
    deferred_t* d = NULL;
    
    Sys_Lock(deferredMutex);
    if((d = (deferred_t*) deferredContentFirst) != NULL)
    {
        deferredContentFirst = d->next;
    }
    if(!deferredContentFirst) deferredContentLast = NULL;
    Sys_Unlock(deferredMutex);
    return d;
}

void GL_DestroyDeferred(deferred_t* d)
{
    M_Free(d->content.buffer);
    M_Free(d);
}

void GL_ReserveNames(void)
{
    int i;
    
    Sys_Lock(deferredMutex);
    for(i = reservedCount; i < NUM_RESERVED_NAMES; ++i)
    {
        reservedNames[i] = gl.NewTexture();
    }
    reservedCount = NUM_RESERVED_NAMES;
    Sys_Unlock(deferredMutex);
}

DGLuint GL_GetReservedName(void)
{
    DGLuint name;
    
    Sys_Lock(deferredMutex);
    
    if(!reservedCount)
    {
        Sys_Unlock(deferredMutex);
        while(reservedCount == 0) 
        {
            // Wait for someone to refill the names buffer.
            Con_Message("GL_GetReservedName: Sleeping until new names available.\n");
            Sys_Sleep(5);
        }
        Sys_Lock(deferredMutex);
    }

    name = reservedNames[0];
    memmove(reservedNames, reservedNames + 1, (NUM_RESERVED_NAMES - 1) * sizeof(DGLuint));
    reservedCount--;
    
    Sys_Unlock(deferredMutex);
    
    return name;
}

/**
 * Initializes a texture content struct with default params.
 */
void GL_InitTextureContent(texturecontent_t *content)
{
    memset(content, 0, sizeof(*content));
    content->minFilter = DGL_LINEAR;
    content->magFilter = DGL_LINEAR;
    content->wrap[0] = DGL_CLAMP;
    content->wrap[1] = DGL_CLAMP;
    content->grayMipmap = -1;
}

void GL_UploadTextureContent(texturecontent_t* content)
{
    int result = 0;
    
    if(content->flags & TXCF_EASY_UPLOAD)
    {
        GL_UploadTexture2(content);
    }
    else
    {
        if(content->grayMipmap >= 0)
        {
            gl.SetInteger(DGL_GRAY_MIPMAP, content->grayMipmap);
        }
        
        // The texture name must already be created.    
        gl.Bind(content->name);
        
        // Upload the texture.
        // No mipmapping or resizing is needed, upload directly.
        if(content->flags & TXCF_NO_COMPRESSION)
        {
            gl.Disable(DGL_TEXTURE_COMPRESSION);
        }
        
        result = gl.TexImage(content->format, content->width, content->height, 
                             (content->grayMipmap >= 0? DGL_GRAY_MIPMAP : 
                              (content->flags & TXCF_MIPMAP) != 0),
                             content->buffer);
        assert(result == DGL_OK);
        
        if(content->flags & TXCF_NO_COMPRESSION)
        {
            gl.Enable(DGL_TEXTURE_COMPRESSION);
        }
    }
    
    gl.TexParameter(DGL_MIN_FILTER, content->minFilter);
    gl.TexParameter(DGL_MAG_FILTER, content->magFilter);
    gl.TexParameter(DGL_WRAP_S, content->wrap[0]);
    gl.TexParameter(DGL_WRAP_T, content->wrap[1]);
}

DGLuint GL_NewTexture(texturecontent_t *content)
{
    // Calculate the size of the buffer automatically?
    if(!content->bufferSize)
    {
        int bytesPerPixel = 0;
        switch(content->format)
        {
            case DGL_COLOR_INDEX_8:
            case DGL_LUMINANCE:
            case DGL_R:
            case DGL_G:
            case DGL_B:
            case DGL_A:
            case DGL_DEPTH_COMPONENT:
                bytesPerPixel = 1;
                break;

            case DGL_COLOR_INDEX_8_PLUS_A8:
            case DGL_LUMINANCE_PLUS_A8:
                bytesPerPixel = 2;
                break;

            case DGL_RGB:
                bytesPerPixel = 3;
                break;
                
            case DGL_RGBA:
                bytesPerPixel = 4;
                break;

            default:
                Con_Error("GL_NewTexture: Unknown format %i, don't know pixel size.\n",
                          content->format);
        }
        content->bufferSize = content->width * content->height * bytesPerPixel;
    }
    
    if(content->flags & TXCF_GET_GRAY_MIPMAP)
    {
        // Use the current level of gray mipmap.
        content->grayMipmap = gl.GetInteger(DGL_GRAY_MIPMAP);
    }
    
    content->name = GL_GetReservedName();
    
    if((content->flags & TXCF_NEVER_DEFER) || !Con_IsBusy())
    {
        // Let's do this right away. No need to take a copy.
        GL_UploadTextureContent(content);
#ifdef _DEBUG
        Con_Message("GL_NewTexture: Uploading (%i:%ix%i) while not busy! Should be precached in busy mode?\n", content->name, content->width, content->height);
#endif
    }
    else
    {
        // Defer this operation. Need to make a copy.
        deferred_t* d = M_Calloc(sizeof(deferred_t));
        memcpy(&d->content, content, sizeof(*content));
        d->content.buffer = M_Malloc(content->bufferSize);
        memcpy(d->content.buffer, content->buffer, content->bufferSize);

        Sys_Lock(deferredMutex);
        if(deferredContentLast)
            deferredContentLast->next = d;
        if(!deferredContentFirst)
            deferredContentFirst = d;
        deferredContentLast = d;
        Sys_Unlock(deferredMutex);
    }
    
    return content->name;
}

DGLuint GL_NewTextureWithParams(int format, int width, int height, void* pixels, 
                                int flags)
{
    texturecontent_t c;
    
    GL_InitTextureContent(&c);
    c.format = format;
    c.width = width;
    c.height = height;
    c.buffer = pixels;
    c.flags = flags;
    return GL_NewTexture(&c);
}

DGLuint GL_NewTextureWithParams2(int format, int width, int height, void* pixels, 
                                 int flags, int minFilter, int magFilter, 
                                 int wrapS, int wrapT)
{
    texturecontent_t c;
    
    GL_InitTextureContent(&c);
    c.format = format;
    c.width = width;
    c.height = height;
    c.buffer = pixels;
    c.flags = flags;
    c.magFilter = magFilter;
    c.minFilter = minFilter;
    c.wrap[0] = wrapS;
    c.wrap[1] = wrapT;
    return GL_NewTexture(&c);
}

/**
 * @param timeOutMilliSeconds  Zero for no timeout.
 */
void GL_UploadDeferredContent(uint timeOutMilliSeconds)
{
    deferred_t *d;
    uint startTime = Sys_GetRealTime();
    
    // We'll reserve names multiple times, because the worker thread may be needing
    // new texture names while we are uploading.
    GL_ReserveNames();
    while((!timeOutMilliSeconds || Sys_GetRealTime() - startTime < timeOutMilliSeconds) &&
          (d = GL_GetNextDeferred()) != NULL)
    {
        GL_UploadTextureContent(&d->content);
        GL_DestroyDeferred(d);
        GL_ReserveNames();
    }
    GL_ReserveNames();
}
