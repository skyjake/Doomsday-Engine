/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * con_busy.c: Console Busy Mode
 *
 * Draws the screen while the main engine thread is working a long
 * operation. The busy mode can be configured to be displaying a progress
 * bar, the console output, or a more generic "please wait" message.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_ui.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void Con_BusyLoop(void);
static void Con_BusyDrawer(void);
static void Con_BusyLoadTextures(void);
static void Con_BusyDeleteTextures(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean busyInited;
static int busyMode;
/*
static char* busyTaskName;
static thread_t busyThread;
static timespan_t busyTime;
static volatile boolean busyDone;
static volatile boolean busyDoneCopy;
static volatile const char* busyError = NULL;
static int busyFont = 0;
static int busyFontHgt; // Height of the font.
static mutex_t busy_Mutex; // To prevent Data races in the busy thread.

static DGLuint texLoading[2];
static DGLuint texScreenshot; // Captured screenshot of the latest frame.
*/

// CODE --------------------------------------------------------------------

/**
 * Busy mode.
 *
 * @param flags         Busy mode flags (see BUSYF_PROGRESS_BAR and others).
 * @param taskName      Optional task name (drawn with the progress bar).
 * @param worker        Worker thread that does processing while in busy mode.
 * @param workerData    Data context for the worker thread.
 *
 * @return              Return value of the worker.
 */
int Con_Busy(int flags, const char* taskName, busyworkerfunc_t worker,
             void* workerData)
{
#if 0
    int                 result = 0;

    if(!busyInited)
    {
        busy_Mutex = Sys_CreateMutex("BUSY_MUTEX");
    }

    if(busyInited)
    {
        Con_Error("Con_Busy: Already busy.\n");
    }

    busyMode = flags;
    Sys_Lock(busy_Mutex);
    busyDone = false;
    if(taskName && taskName[0])
    {   // Take a copy of the task name.
        size_t              len = strlen(taskName);

        busyTaskName = M_Calloc(len + 1);
        snprintf(busyTaskName, len, "%s", taskName);
    }
    Sys_Unlock(busy_Mutex);

    // Load any textures needed in this mode.
    Con_BusyLoadTextures();

    busyInited = true;

    // Start the busy worker thread, which will proces things in the
    // background while we keep the user occupied with nice animations.
    busyThread = Sys_StartThread(worker, workerData);
#endif

    int result = worker(workerData);
    if(!(isDedicated || (busyMode & BUSYF_NO_UPLOADS)))
    {
        GL_UploadDeferredContent(0);
    }
    return result;
    
#if 0
    // Wait for the busy thread to stop.
    Con_BusyLoop();

    // Free resources.
    Con_BusyDeleteTextures();
    if(busyTaskName)
        M_Free(busyTaskName);
    busyTaskName = NULL;

    if(busyError)
    {
        Con_AbnormalShutdown((const char*) busyError);
    }

    // Make sure the worker finishes before we continue.
    result = Sys_WaitThread(busyThread);
    busyThread = NULL;
    busyInited = false;

    // Make sure that any remaining deferred content gets uploaded.
    if(!(isDedicated || (busyMode & BUSYF_NO_UPLOADS)))
    {
        GL_UploadDeferredContent(0);
    }
    return result;
#endif
}

/**
 * Called by the busy worker to shutdown the engine immediately.
 *
 * @param message       Message, expected to exist until the engine closes.
 */
void Con_BusyWorkerError(const char* message)
{
    //busyError = message;
    //Con_BusyWorkerEnd();
    Con_Error(message);
}

/**
 * Called by the busy worker thread when it has finished processing,
 * to end busy mode.
 */
void Con_BusyWorkerEnd(void)
{
#if 0
    if(!busyInited)
        return;

    Sys_Lock(busy_Mutex);
    busyDone = true;
    Sys_Unlock(busy_Mutex);
#endif
}

boolean Con_IsBusy(void)
{
    return busyInited;
}

#if 0
static void Con_BusyLoadTextures(void)
{
    image_t             image;

    if(isDedicated)
        return;

    if(!(busyMode & BUSYF_STARTUP))
    {
        // Not in startup, so take a copy of the current frame contents.
        Con_AcquireScreenshotTexture();
    }

    // Need to load the progress indicator?
    if(busyMode & (BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR))
    {
        // These must be real files in the base dir because virtual files haven't
        // been loaded yet when engine startup is done.
        if(GL_LoadImage(&image, "}data/graphics/loading1.png"))
        {
            texLoading[0] = GL_NewTextureWithParams(DGL_RGBA, image.width, image.height,
                                                    image.pixels, TXCF_NEVER_DEFER);
            GL_DestroyImage(&image);
        }

        if(GL_LoadImage(&image, "}data/graphics/loading2.png"))
        {
            texLoading[1] = GL_NewTextureWithParams(DGL_RGBA, image.width, image.height,
                                                    image.pixels, TXCF_NEVER_DEFER);
            GL_DestroyImage(&image);
        }
    }

    if((busyMode & BUSYF_CONSOLE_OUTPUT) || busyTaskName)
    {
        const char*         fontName;

        if(!(theWindow->width > 640))
            fontName = "normal12";
        else
            fontName = "normal18";

        if(FR_PrepareFont(fontName))
        {
            busyFont = FR_GetCurrent();
            busyFontHgt = FR_TextHeight("A");
        }
    }
}

static void Con_BusyDeleteTextures(void)
{
    if(isDedicated)
        return;

    // Destroy the font.
/*    if(busyFont)
    {
        FR_DestroyFont(busyFont);
    }*/

    glDeleteTextures(2, (const GLuint*) texLoading);
    texLoading[0] = texLoading[1] = 0;

    busyFont = 0;

    // Don't release this yet if doing a wipe.
    Con_ReleaseScreenshotTexture();
}
#endif

/**
 * Take a screenshot and store it as a texture.
 */
void Con_AcquireScreenshotTexture(void)
{
#if 0
    int                 oldMaxTexSize = GL_state.maxTexSize;
    byte*               frame;
#ifdef _DEBUG
    timespan_t          startTime;
#endif

    if(texScreenshot)
    {
        Con_ReleaseScreenshotTexture();
    }

#ifdef _DEBUG
    startTime = Sys_GetRealSeconds();
#endif

    frame = M_Malloc(theWindow->width * theWindow->height * 3);
    GL_Grab(0, 0, theWindow->width, theWindow->height, DGL_RGB, frame);
    GL_state.maxTexSize = 512; // A bit of a hack, but don't use too large a texture.
    texScreenshot = GL_UploadTexture(frame, theWindow->width, theWindow->height,
                                     false, false, true, false, true,
                                     GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
                                     GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                     TXCF_NEVER_DEFER|TXCF_NO_COMPRESSION);
    GL_state.maxTexSize = oldMaxTexSize;
    M_Free(frame);

#ifdef _DEBUG
    printf("Con_AcquireScreenshotTexture: Took %.2f seconds.\n",
           Sys_GetRealSeconds() - startTime);
#endif
#endif
}

void Con_ReleaseScreenshotTexture(void)
{
#if 0
    glDeleteTextures(1, (const GLuint*) &texScreenshot);
    texScreenshot = 0;
#endif
}

#if 0
/**
 * The busy thread main function. Runs until busyDone set to true.
 */
static void Con_BusyLoop(void)
{
    boolean             canDraw = !isDedicated;
    boolean             canUpload =
        !(isDedicated || (busyMode & BUSYF_NO_UPLOADS));
    timespan_t          startTime = Sys_GetRealSeconds();

    if(canDraw)
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);
    }

    Sys_Lock(busy_Mutex);
    busyDoneCopy = busyDone;
    Sys_Unlock(busy_Mutex);

    while(!busyDoneCopy || (canUpload && GL_GetDeferredCount() > 0))
    {
        Sys_Lock(busy_Mutex);
        busyDoneCopy = busyDone;
        Sys_Unlock(busy_Mutex);

        Sys_Sleep(20);

        if(canUpload)
        {   // Make sure that any deferred content gets uploaded.
            GL_UploadDeferredContent(15);
        }

        // Update the time.
        busyTime = Sys_GetRealSeconds() - startTime;

        // Time for an update?
        if(canDraw)
            Con_BusyDrawer();
    }

    if(canDraw)
    {
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }

#if _DEBUG
    Sys_CheckGLError();
#endif

    if(verbose)
    {
        Con_Message("Con_Busy: Was busy for %.2lf seconds.\n", busyTime);
    }

#if _DEBUG
    Sys_CheckGLError();
#endif
}

/**
 * Draws the captured screenshot as a background, or just clears the screen if no
 * screenshot is available.
 */
static void Con_DrawScreenshotBackground(float x, float y, float width,
                                         float height)
{
    if(texScreenshot)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texScreenshot);

        glColor3ub(255, 255, 255);
        glBegin(GL_QUADS);
            glTexCoord2f(0, 1);
            glVertex2f(x, y);
            glTexCoord2f(1, 1);
            glVertex2f(x + width, y);
            glTexCoord2f(1, 0);
            glVertex2f(x + width, y + height);
            glTexCoord2f(0, 0);
            glVertex2f(x, y + height);
        glEnd();
    }
    else
    {
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

/**
 * @param 0...1, to indicate how far things have progressed.
 */
static void Con_BusyDrawIndicator(float x, float y, float radius, float pos)
{
    float               col[4] = {1.f, 1.f, 1.f, .2f};
    int                 i = 0;
    int                 edgeCount = 0;
    int                 backW, backH;

    backW = backH = (radius * 2);

    pos = MINMAX_OF(0, pos, 1);
    edgeCount = MAX_OF(1, pos * 30);

    // Draw a background.
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    GL_BlendMode(BM_NORMAL);

    glBegin(GL_TRIANGLE_FAN);
        glColor4ub(0, 0, 0, 140);
        glVertex2f(x, y);
        glColor4ub(0, 0, 0, 0);
        glVertex2f(x, y - backH);
        glVertex2f(x + backW*.8f, y - backH*.8f);
        glVertex2f(x + backW, y);
        glVertex2f(x + backW*.8f, y + backH*.8f);
        glVertex2f(x, y + backH);
        glVertex2f(x - backW*.8f, y + backH*.8f);
        glVertex2f(x - backW, y);
        glVertex2f(x - backW*.8f, y - backH*.8f);
        glVertex2f(x, y - backH);
    glEnd();

    // Draw the frame.
    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, texLoading[0]);
    GL_DrawRect(x - radius, y - radius, radius*2, radius*2, col[0], col[1], col[2], col[3]);

    // Rotate around center.
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(.5f, .5f, 0.f);
    glRotatef(-busyTime * 20, 0.f, 0.f, 1.f);
    glTranslatef(-.5f, -.5f, 0.f);

    // Draw a fan.
    glColor4f(col[0], col[1], col[2], .66f);
    glBindTexture(GL_TEXTURE_2D, texLoading[1]);
    glBegin(GL_TRIANGLE_FAN);
    // Center.
    glTexCoord2f(.5f, .5f);
    glVertex2f(x, y);
    // Vertices along the edge.
    for(i = 0; i <= edgeCount; ++i)
    {
        float               angle = 2 * PI * pos *
            (i / (float)edgeCount) + PI/2;

        glTexCoord2f(.5f + cos(angle)*.5f, .5f + sin(angle)*.5f);
        glVertex2f(x + cos(angle)*radius*1.105f, y + sin(angle)*radius*1.105f);
    }
    glEnd();

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    // Draw the task name.
    if(busyTaskName)
    {
        glColor4f(1.f, 1.f, 1.f, .66f);
        FR_TextOut(busyTaskName, x+radius, y - busyFontHgt/2);
    }
}

#define LINE_COUNT 4

/**
 * @return              Number of new lines since the old ones.
 */
static int GetBufLines(cbuffer_t* buffer, cbline_t const **oldLines)
{
    cbline_t const*     bufLines[LINE_COUNT + 1];
    int                 count;
    int                 newCount = 0;
    int                 i, k;

    count = Con_BufferGetLines(buffer, LINE_COUNT, -LINE_COUNT, bufLines);
    for(i = 0; i < count; ++i)
    {
        for(k = 0; k < 2 * LINE_COUNT - newCount; ++k)
        {
            if(oldLines[k] == bufLines[i])
            {
                goto lineIsNotNew;
            }
        }

        newCount++;
        for(k = 0; k < (2 * LINE_COUNT - 1); ++k)
            oldLines[k] = oldLines[k+1];

        //memmove(oldLines, oldLines + 1, sizeof(cbline_t*) * (2 * LINE_COUNT - 1));
        oldLines[2 * LINE_COUNT - 1] = bufLines[i];

lineIsNotNew:;
    }
    return newCount;
}

/**
 * Draws a number of console output to the bottom of the screen.
 * \fixme: Wow. I had some weird time hacking the smooth scrolling. Cleanup would be
 *         good some day. -jk
 */
void Con_BusyDrawConsoleOutput(void)
{
    cbuffer_t*          buffer;
    static cbline_t const* visibleBusyLines[2 * LINE_COUNT];
    static float        scroll = 0;
    static float        scrollStartTime = 0;
    static float        scrollEndTime = 0;
    static double       lastNewTime = 0;
    static double       timeSinceLastNew = 0;
    double              nowTime = 0;
    float               y, topY;
    uint                i, newCount;

    buffer = Con_GetConsoleBuffer();
    newCount = GetBufLines(buffer, visibleBusyLines);
    nowTime = Sys_GetRealSeconds();
    if(newCount > 0)
    {
        timeSinceLastNew = nowTime - lastNewTime;
        lastNewTime = nowTime;
        if(nowTime < scrollEndTime)
        {
            // Abort last scroll.
            scroll = 0;
            scrollStartTime = nowTime;
            scrollEndTime = nowTime;
        }
        else
        {
            double              interval =
                MIN_OF(timeSinceLastNew/2, 1.3);

            // Begin new scroll.
            scroll = newCount;
            scrollStartTime = nowTime;
            scrollEndTime = nowTime + interval;
        }
    }

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    GL_BlendMode(BM_NORMAL);

    // Dark gradient as background.
    glBegin(GL_QUADS);
    glColor4ub(0, 0, 0, 0);
    y = theWindow->height - (LINE_COUNT + 3) * busyFontHgt;
    glVertex2f(0, y);
    glVertex2f(theWindow->width, y);
    glColor4ub(0, 0, 0, 128);
    glVertex2f(theWindow->width, theWindow->height);
    glVertex2f(0, theWindow->height);
    glEnd();

    glEnable(GL_TEXTURE_2D);

    // The text lines.
    topY = y = theWindow->height - busyFontHgt * (2 * LINE_COUNT + .5f);
    if(newCount > 0 ||
       (nowTime >= scrollStartTime && nowTime < scrollEndTime && scrollEndTime > scrollStartTime))
    {
        if(scrollEndTime - scrollStartTime > 0)
            y += scroll * (scrollEndTime - nowTime) / (scrollEndTime - scrollStartTime) *
                busyFontHgt;
    }

    for(i = 0; i < 2 * LINE_COUNT; ++i, y += busyFontHgt)
    {
        float               color = 1;//lineAlpha[i];
        const cbline_t*     line = visibleBusyLines[i];

        if(!line)
            continue;

        color = ((y - topY) / busyFontHgt) - (LINE_COUNT - 1);
        if(color < LINE_COUNT)
            color = MINMAX_OF(0, color/2, 1);
        else
            color = 1 - (color - LINE_COUNT);

        if(!(line->flags & CBLF_RULER)) // ignore rulers.
        {
            if(!line->text)
                continue;

            glColor4f(1.f, 1.f, 1.f, color);
            FR_TextOut(line->text, (theWindow->width - FR_TextWidth(line->text))/2, y);
        }
    }

#undef LINE_COUNT
}

/**
 * Busy drawer function. The entire frame is drawn here.
 */
static void Con_BusyDrawer(void)
{
    float               pos = 0;

#if _DEBUG
    Sys_CheckGLError();
#endif

    Con_DrawScreenshotBackground(0, 0, theWindow->width, theWindow->height);

#if _DEBUG
    Sys_CheckGLError();
#endif

    // Indefinite activity?
    if(busyMode & BUSYF_ACTIVITY)
    {
        pos = 1;
    }
    else if(busyMode & BUSYF_PROGRESS_BAR)
    {
        // The progress is animated elsewhere.
        pos = Con_GetProgress();
    }
    Con_BusyDrawIndicator(theWindow->width/2, theWindow->height/2, theWindow->height/12, pos);

#if _DEBUG
    Sys_CheckGLError();
#endif

    // Output from the console?
    if(busyMode & BUSYF_CONSOLE_OUTPUT)
    {
        Con_BusyDrawConsoleOutput();
    }

#if _DEBUG
    Sys_CheckGLError();
#endif

    Sys_UpdateWindow(windowIDX);
}
#endif
