/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

deferred_t* GL_GetNextDeferred(void);
void        GL_DestroyDeferred(deferred_t* d);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mutex_t deferredMutex;
static DGLuint reservedNames[NUM_RESERVED_NAMES];
static volatile int reservedCount = 0;

static volatile deferred_t* deferredContentFirst = NULL;
static volatile deferred_t* deferredContentLast = NULL;
static boolean deferredInited = false;

// CODE --------------------------------------------------------------------

void GL_InitDeferred(void)
{
    if(deferredInited)
        return; // Been here already...

    deferredInited = true;
    deferredMutex = Sys_CreateMutex("DGLDeferredMutex");
    GL_ReserveNames();
}

void GL_ShutdownDeferred(void)
{
    deferred_t*         d;

    if(!deferredInited)
        return;

    GL_ReleaseReservedNames();

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
    int                 count = 0;
    deferred_t*         i = 0;

    if(!deferredInited)
        return 0;

    Sys_Lock(deferredMutex);
    for(i = (deferred_t*) deferredContentFirst; i; i = i->next, ++count);
    Sys_Unlock(deferredMutex);
    return count;
}

deferred_t* GL_GetNextDeferred(void)
{
    deferred_t*         d = NULL;

    if(!deferredInited)
        return NULL;

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
    if(!deferredInited)
        return; // Just ignore.

    Sys_Lock(deferredMutex);
    if(reservedCount < NUM_RESERVED_NAMES)
    {
        glGenTextures(NUM_RESERVED_NAMES - reservedCount,
                      (GLuint*) &reservedNames[reservedCount]);
        reservedCount = NUM_RESERVED_NAMES;
    }
    Sys_Unlock(deferredMutex);
}

void GL_ReleaseReservedNames(void)
{
    if(!deferredInited)
        return; // Just ignore.

    Sys_Lock(deferredMutex);
    glDeleteTextures(reservedCount, (const GLuint*) reservedNames);
    memset(reservedNames, 0, sizeof(reservedNames));
    reservedCount = 0;
    Sys_Unlock(deferredMutex);
}

DGLuint GL_GetReservedName(void)
{
    DGLuint             name;

    if(!deferredInited)
        Con_Error("GL_GetReservedName: Deferred GL task system not initialized.");

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
void GL_InitTextureContent(texturecontent_t* content)
{
    memset(content, 0, sizeof(*content));
    content->palette = 0; // Use the default.
    content->minFilter = GL_LINEAR;
    content->magFilter = GL_LINEAR;
    content->anisoFilter = -1; // Best.
    content->wrap[0] = GL_CLAMP;
    content->wrap[1] = GL_CLAMP;
    content->grayMipmap = -1;
}

void GL_UploadTextureContent(texturecontent_t* content)
{
    if(content->flags & TXCF_EASY_UPLOAD)
    {
        GL_UploadTexture2(content);
    }
    else
    {
        byte*               allocatedTempBuffer = NULL;
        DGLuint             palid;

        if(content->grayMipmap >= 0)
        {
            GL_SetGrayMipmap(content->grayMipmap);
        }

        // The texture name must already be created.
        glBindTexture(GL_TEXTURE_2D, content->name);

        // Upload the texture.
        // No mipmapping or resizing is needed, upload directly.
        if(content->flags & TXCF_NO_COMPRESSION)
        {
            GL_SetTextureCompression(false);
        }

        if((content->flags & TXCF_CONVERT_8BIT_TO_ALPHA) &&
           (content->format == DGL_LUMINANCE ||
            content->format == DGL_COLOR_INDEX_8 ||
            content->format == DGL_DEPTH_COMPONENT))
        {
            int                 total = content->width * content->height;

            allocatedTempBuffer = M_Malloc(total * 2);

            // Move the average color to the alpha channel, make the
            // actual color white.
            memcpy(allocatedTempBuffer + total, content->buffer, total);
            memset(allocatedTempBuffer, 255, total);

            content->format = DGL_LUMINANCE_PLUS_A8;
        }

        // Do we need to locate a color palette?
        if(content->format == DGL_COLOR_INDEX_8 ||
           content->format == DGL_COLOR_INDEX_8_PLUS_A8)
        {
            palid = R_GetColorPalette(content->palette);
        }
        else
            palid = 0;

        if(!GL_TexImage(content->format, palid, content->width,
                        content->height,
                        (content->grayMipmap >= 0? DDMAXINT :
                         (content->flags & TXCF_MIPMAP) != 0),
                        allocatedTempBuffer? allocatedTempBuffer : content->buffer))
            Con_Error("GL_UploadTextureContent: TexImage failed "
                      "(%u:%ix%i fmt%i).", content->name, content->width,
                      content->height, (int) content->format);

        if(content->flags & TXCF_NO_COMPRESSION)
        {
            GL_SetTextureCompression(true);
        }

        if(allocatedTempBuffer)
        {
            M_Free(allocatedTempBuffer);
        }
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, content->minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, content->magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, content->wrap[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, content->wrap[1]);
    if(GL_state.useAnisotropic)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        GL_GetTexAnisoMul(content->anisoFilter));
#ifdef _DEBUG
    Sys_CheckGLError();
#endif
}

/**
 * @return              @c true, iff this operation was deferred.
 */
DGLuint GL_NewTexture(texturecontent_t* content, boolean* result)
{
    boolean             deferred = true;

    // Calculate the size of the buffer automatically?
    if(content->bufferSize == 0)
    {
        int                 bytesPerPixel = 0;

        switch(content->format)
        {
        case DGL_LUMINANCE:
            if(content->flags & TXCF_CONVERT_8BIT_TO_ALPHA)
                bytesPerPixel = 2; // We'll need a larger buffer.
            else
                bytesPerPixel = 1;
            break;

        case DGL_COLOR_INDEX_8:
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
            Con_Error("GL_NewTexture: Unknown format %i, "
                      "don't know pixel size.\n", content->format);
        }
        content->bufferSize = content->width * content->height *
            bytesPerPixel;
    }

    if(content->flags & TXCF_GRAY_MIPMAP)
    {
        content->grayMipmap = ((content->flags & TXCF_GRAY_MIPMAP_LEVEL_MASK)
                               >> TXCF_GRAY_MIPMAP_LEVEL_SHIFT);
    }

    content->name = GL_GetReservedName();

    if((content->flags & TXCF_NEVER_DEFER) || !Con_IsBusy())
    {
        // Let's do this right away. No need to take a copy.
        GL_UploadTextureContent(content);
#ifdef _DEBUG
        VERBOSE2(Con_Message("GL_NewTexture: Uploading (%i:%ix%i) while not busy! "
            "Should be precached in busy mode?\n", content->name,
            content->width, content->height));
#endif
        deferred = false; // We haven't deferred.
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

    if(result)
        *result = deferred;

    return content->name;
}

DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width,
                                int height, const void* pixels, int flags)
{
    texturecontent_t c;

    GL_InitTextureContent(&c);
    c.format = format;
    c.width = width;
    c.height = height;
    c.buffer = (void*)pixels;
    c.flags = flags;
    return GL_NewTexture(&c, NULL);
}

DGLuint GL_NewTextureWithParams2(dgltexformat_t format, int width,
                                 int height, const void* pixels, int flags,
                                 int minFilter, int magFilter,
                                 int anisoFilter, int wrapS, int wrapT)
{
    texturecontent_t c;

    GL_InitTextureContent(&c);
    c.format = format;
    c.width = width;
    c.height = height;
    c.buffer = (void*)pixels;
    c.flags = flags;
    c.magFilter = magFilter;
    c.minFilter = minFilter;
    c.anisoFilter = anisoFilter;
    c.wrap[0] = wrapS;
    c.wrap[1] = wrapT;
    return GL_NewTexture(&c, NULL);
}

/**
 * Same as above except this version is part of the public API and thus some
 * of the paramaters use the DGL counterparts.
 */
DGLuint GL_NewTextureWithParams3(dgltexformat_t format, int width,
                                 int height, const void* pixels, int flags,
                                 int minFilter, int magFilter,
                                 int anisoFilter, int wrapS, int wrapT)
{
    return GL_NewTextureWithParams2(format, width, height, pixels, flags,
        (minFilter == DGL_LINEAR? GL_LINEAR :
         minFilter == DGL_NEAREST? GL_NEAREST :
         minFilter == DGL_NEAREST_MIPMAP_NEAREST? GL_NEAREST_MIPMAP_NEAREST :
         minFilter == DGL_LINEAR_MIPMAP_NEAREST? GL_LINEAR_MIPMAP_NEAREST :
         minFilter == DGL_NEAREST_MIPMAP_LINEAR? GL_NEAREST_MIPMAP_LINEAR :
         GL_LINEAR_MIPMAP_LINEAR),
        (magFilter == DGL_LINEAR? GL_LINEAR : GL_NEAREST), anisoFilter,
        (wrapS == DGL_CLAMP? GL_CLAMP :
         wrapS == DGL_CLAMP_TO_EDGE? GL_CLAMP_TO_EDGE : GL_REPEAT),
        (wrapT == DGL_CLAMP? GL_CLAMP :
         wrapT == DGL_CLAMP_TO_EDGE? GL_CLAMP_TO_EDGE : GL_REPEAT));
}

/**
 * @param timeOutMilliSeconds  Zero for no timeout.
 */
void GL_UploadDeferredContent(uint timeOutMilliSeconds)
{
    deferred_t*         d;
    uint                startTime;

    if(!deferredInited)
        Con_Error("GL_UploadDeferredContent: Deferred GL task system "
                  "not initialized.");

    startTime = Sys_GetRealTime();

    // We'll reserve names multiple times, because the worker thread may be
    // needing new texture names while we are uploading.
    GL_ReserveNames();
    while((!timeOutMilliSeconds ||
           Sys_GetRealTime() - startTime < timeOutMilliSeconds) &&
          (d = GL_GetNextDeferred()) != NULL)
    {
        GL_UploadTextureContent(&d->content);
        GL_DestroyDeferred(d);
        GL_ReserveNames();
    }
    GL_ReserveNames();
}
