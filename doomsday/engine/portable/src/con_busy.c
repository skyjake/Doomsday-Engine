/**
 * @file con_busy.c
 * Console Busy Mode @ingroup console
 *
 * @authors Copyright © 2007-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_ui.h"
#include "de_misc.h"
#include "de_network.h"

#include "s_main.h"
#include "image.h"
#include "texturecontent.h"
#include "cbuffer.h"
#include "font.h"

#if 0
#define SCREENSHOT_TEXTURE_SIZE 512
#endif

#define DOOMWIPESINE_NUMSAMPLES 320

static void BusyTask_Loop(void);
static void BusyTask_Drawer(void);
static void Con_BusyPrepareResources(void);
static void deleteBusyTextures(void);
static void seedDoomWipeSine(void);

int rTransition = (int) TS_CROSSFADE; ///< cvar Default transition style.
int rTransitionTics = 28; ///< cvar Default transition duration (in tics).

static boolean busyInited;
static BusyTask* busyTask; // Current task.
static thread_t busyThread;
static timespan_t busyTime;
static timespan_t accumulatedBusyTime; // Never cleared.
static volatile boolean busyDone;
static volatile boolean busyDoneCopy;
static boolean busyTaskEndedWithError;
static char busyError[256];
static fontid_t busyFont = 0;
static int busyFontHgt; // Height of the font.
static mutex_t busy_Mutex; // To prevent Data races in the busy thread.

static DGLuint texLoading[2];
static DGLuint texScreenshot; // Captured screenshot of the latest frame.

typedef struct {
    boolean inProgress; /// @c true= a transition is presently being animated.
    transitionstyle_t style; /// Style of transition (cross-fade, wipe, etc...).
    uint startTime; /// Time at the moment the transition began (in 35hz tics).
    uint tics; /// Time duration of the animation.
    float position; /// Animation interpolation point [0..1].
} transitionstate_t;

static transitionstate_t transition;

static float doomWipeSine[DOOMWIPESINE_NUMSAMPLES];
static float doomWipeSamples[SCREENWIDTH+1];

static boolean animatedTransitionActive(int busyMode)
{
    return (!isDedicated && !netGame && !(busyMode & BUSYF_STARTUP) &&
            rTransitionTics > 0 && (busyMode & BUSYF_TRANSITION));
}

/**
 * Sets up module state for running a busy task. After this the busy mode event
 * loop is started. The loop will run until the worker thread exits.
 *
 * @todo Refactor: Should be private and static (currently needed in busytask.cpp)
 */
void BusyTask_Begin(BusyTask* task)
{
    if(!task) return;

    if(!busyInited)
    {
        busy_Mutex = Sys_CreateMutex("BUSY_MUTEX");
    }
    if(busyInited)
    {
        Con_Error("Con_Busy: Already busy.\n");
    }

    // Discard input events so that any and all accumulated input
    // events are ignored.
    task->_wasIgnoringInput = DD_IgnoreInput(true);

    Sys_Lock(busy_Mutex);
    busyDone = false;
    busyTask = task;
    task->_willAnimateTransition = animatedTransitionActive(busyTask->mode);
    Sys_Unlock(busy_Mutex);

    // Load any textures needed in this mode.
    Con_BusyPrepareResources();

    busyTaskEndedWithError = false;
    busyInited = true;

    // Start the busy worker thread, which will process the task in the
    // background while we keep the user occupied with nice animations.
    busyThread = Sys_StartThread(busyTask->worker, busyTask->workerData);

    // Are we doing a transition effect?
    if(task->_willAnimateTransition)
    {
        transition.tics = rTransitionTics;
        transition.style = rTransition;
        if(transition.style == TS_DOOM || transition.style == TS_DOOMSMOOTH)
            seedDoomWipeSine();
        transition.inProgress = true;
    }

    // Switch the engine loop and window to the busy mode.
    LegacyCore_SetLoopFunc(de2LegacyCore, BusyTask_Loop);

    Window_SetDrawFunc(Window_Main(), BusyTask_Drawer);

    task->_startTime = Sys_GetRealSeconds();
}

/**
 * Exits the busy mode event loop. Called in the main thread, does not return
 * until the worker thread is stopped.
 */
static void BusyTask_Exit(void)
{
    int result;

    LIBDENG_ASSERT_IN_MAIN_THREAD();

    busyDone = true;

    // Make sure the worker finishes before we continue.
    result = Sys_WaitThread(busyThread, busyTaskEndedWithError? 100 : 5000);
    busyThread = NULL;

    BusyTask_StopEventLoopWithValue(result);
}

/**
 * Called after the busy mode worker thread and the event (sub-)loop has been stopped.
 *
 * @todo Refactor: Should be private and static (currently needed in busytask.cpp)
 */
void BusyTask_End(BusyTask* task)
{
    if(verbose)
    {
        Con_Message("Con_Busy: Was busy for %.2lf seconds.\n", busyTime);
    }

#if 0
    // Free resources.
    deleteBusyTextures();
#endif

    // The window drawer will be restored later to the appropriate function.
    Window_SetDrawFunc(Window_Main(), 0);

    if(busyTaskEndedWithError)
    {
        Con_AbnormalShutdown(busyError);
    }

    if(transition.inProgress)
    {
        transition.startTime = Sys_GetTime();
        transition.position = 0;
    }

    // Make sure that any remaining deferred content gets uploaded.
    if(!(task->mode & BUSYF_NO_UPLOADS))
    {
        GL_ProcessDeferredTasks(0);
    }

    Sys_DestroyMutex(busy_Mutex);
    busyInited = false;

    DD_IgnoreInput(task->_wasIgnoringInput);
    DD_ResetTimer();
}

void Con_BusyWorkerError(const char* message)
{
    busyTaskEndedWithError = true;
    strncpy(busyError, message, sizeof(busyError) - 1);
    Con_BusyWorkerEnd();
}

void Con_BusyWorkerEnd(void)
{
    if(!busyInited) return;

    Sys_Lock(busy_Mutex);
    busyDone = true;
    Sys_Unlock(busy_Mutex);
}

boolean Con_IsBusy(void)
{
    return busyInited;
}

boolean Con_InBusyWorker(void)
{
    boolean result;
    if(!Con_IsBusy()) return false;

    /// @todo Is locking necessary?
    Sys_Lock(busy_Mutex);
    result = Sys_ThreadId(busyThread) == Sys_CurrentThreadId();
    Sys_Unlock(busy_Mutex);
    return result;
}

static void Con_BusyPrepareResources(void)
{
    if(isDedicated || novideo) return;

    if(!(busyTask->mode & BUSYF_STARTUP))
    {
        // Not in startup, so take a copy of the current frame contents.
        Con_AcquireScreenshotTexture();
    }

    // Need to load any fonts for log messages etc?
    if((busyTask->mode & BUSYF_CONSOLE_OUTPUT) || busyTask->name)
    {
        // These must be real files in the base dir because virtual files haven't
        // been loaded yet when the engine startup is done.
        struct busyFont {
            const char* name;
            const char* path;
        } static const fonts[] = {
            { FN_SYSTEM_NAME":normal12", "}data/fonts/normal12.dfn" },
            { FN_SYSTEM_NAME":normal18", "}data/fonts/normal18.dfn" }
        };
        int fontIdx = !(Window_Width(theWindow) > 640)? 0 : 1;
        Uri* uri = Uri_NewWithPath2(fonts[fontIdx].name, RC_NULL);
        font_t* font = R_CreateFontFromFile(uri, fonts[fontIdx].path);
        Uri_Delete(uri);
        if(font)
        {
            busyFont = Fonts_Id(font);
            FR_SetFont(busyFont);
            FR_LoadDefaultAttrib();
            busyFontHgt = FR_SingleLineHeight("Busy");
        }
    }
}

static void deleteBusyTextures(void)
{
    if(novideo) return;

    glDeleteTextures(2, (const GLuint*) texLoading);
    texLoading[0] = texLoading[1] = 0;

    // Don't release yet if doing a transition.
    if(!transition.inProgress)
        Con_ReleaseScreenshotTexture();

    busyFont = 0;
}

/**
 * Take a screenshot and store it as a texture.
 */
void Con_AcquireScreenshotTexture(void)
{
#if 0
    int oldMaxTexSize = GL_state.maxTexSize;
    uint8_t* frame;
#ifdef _DEBUG
    timespan_t startTime;
#endif
#endif
    //timespan_t startTime = Sys_GetRealSeconds();

    if(texScreenshot)
    {
        Con_ReleaseScreenshotTexture();
    }
    texScreenshot = Window_GrabAsTexture(Window_Main(), true /*halfsized*/);

    //Con_Message("Acquired screenshot texture %i in %f seconds.", texScreenshot, Sys_GetRealSeconds() - startTime));

#if 0
#ifdef _DEBUG
    startTime = Sys_GetRealSeconds();
#endif
    frame = malloc(Window_Width(theWindow) * Window_Height(theWindow) * 3);
    GL_Grab(0, 0, Window_Width(theWindow), Window_Height(theWindow), DGL_RGB, frame);
    GL_state.maxTexSize = SCREENSHOT_TEXTURE_SIZE; // A bit of a hack, but don't use too large a texture.
    texScreenshot = GL_NewTextureWithParams2(DGL_RGB, Window_Width(theWindow), Window_Height(theWindow),
        frame, TXCF_NEVER_DEFER|TXCF_NO_COMPRESSION|TXCF_UPLOAD_ARG_NOSMARTFILTER, 0, GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    GL_state.maxTexSize = oldMaxTexSize;
    free(frame);
#ifdef _DEBUG
    printf("Con_AcquireScreenshotTexture: Took %.2f seconds.\n",
           Sys_GetRealSeconds() - startTime);
#endif
#endif
}

void Con_ReleaseScreenshotTexture(void)
{
    glDeleteTextures(1, (const GLuint*) &texScreenshot);
    texScreenshot = 0;
}

static void loadBusyTextures(void)
{
    image_t image;

    if(novideo) return;

    if(GL_LoadImage(&image, "}data/graphics/loading1.png"))
    {
        texLoading[0] = GL_NewTextureWithParams(DGL_RGBA, image.size.width, image.size.height, image.pixels, TXCF_NEVER_DEFER);
        GL_DestroyImage(&image);
    }

    if(GL_LoadImage(&image, "}data/graphics/loading2.png"))
    {
        texLoading[1] = GL_NewTextureWithParams(DGL_RGBA, image.size.width, image.size.height, image.pixels, TXCF_NEVER_DEFER);
        GL_DestroyImage(&image);
    }
}

static void preBusySetup(void)
{
    loadBusyTextures();

    // Save the present loop.
    LegacyCore_PushLoop(de2LegacyCore);

    // Set up loop for busy mode.
    LegacyCore_SetLoopRate(de2LegacyCore, 60);
    LegacyCore_SetLoopFunc(de2LegacyCore, NULL); // don't call main loop's func while busy

    Window_SetDrawFunc(Window_Main(), 0);
}

static void postBusyCleanup(void)
{
    deleteBusyTextures();

    // Restore old loop.
    LegacyCore_PopLoop(de2LegacyCore);

    // Resume drawing with the game loop drawer.
    Window_SetDrawFunc(Window_Main(), !Sys_IsShuttingDown()? DD_GameLoopDrawer : 0);
}

static int doBusy(int mode, const char* taskName, busyworkerfunc_t worker, void* workerData)
{
    if(novideo)
    {
        // Don't bother to start a thread -- non-GUI mode.
        return worker(workerData);
    }
    return BusyTask_Run(mode, taskName, worker, workerData);
}

int Con_Busy(int mode, const char* taskName, busyworkerfunc_t worker, void* workerData)
{
    int result;

    preBusySetup();
    result = doBusy(mode, taskName, worker, workerData);
    postBusyCleanup();
    return result;
}

void Con_BusyList(BusyTask* tasks, int numTasks)
{
    const char* currentTaskName = NULL;
    BusyTask* task;
    int i, mode;

    if(!tasks) return; // Hmm, no work?

    preBusySetup();

    // Process tasks.
    task = tasks;
    for(i = 0; i < numTasks; ++i, task++)
    {
        // If no name is specified for this task, continue using the name of the
        // the previous task.
        if(task->name)
        {
            if(task->name[0])
                currentTaskName = task->name;
            else // Clear the name.
                currentTaskName = NULL;
        }

        if(!task->worker)
        {
            // Null tasks are not processed; so signal success.
            //task->retVal = 0;
            continue;
        }

        // Process the work.

        // Is the worker updating its progress?
        if(task->maxProgress > 0)
            Con_InitProgress2(task->maxProgress, task->progressStart, task->progressEnd);

        /// @kludge Force BUSYF_STARTUP here so that the animation of one task is
        ///         not drawn on top of the last frame of the previous.
        mode = task->mode | BUSYF_STARTUP;
        // kludge end

        // Busy mode invokes the worker on our behalf in a new thread.
        doBusy(mode, currentTaskName, task->worker, task->workerData);
    }

    postBusyCleanup();
}

/**
 * The busy loop callback function. Called periodically in the main (UI) thread
 * while the busy worker is running.
 */
static void BusyTask_Loop(void)
{
    boolean canUpload = !(busyTask->mode & BUSYF_NO_UPLOADS);
    timespan_t oldTime;

    Garbage_Recycle();

    // Post and discard all input events.
    DD_ProcessEvents(0);
    DD_ProcessSharpEvents(0);

    if(canUpload)
    {
        Window_GLActivate(Window_Main());

        // Any deferred content needs to get uploaded.
        GL_ProcessDeferredTasks(15);
    }

    // Make sure the audio system gets regularly updated.
    S_EndFrame();

    // We accumulate time in the busy loop so that the animation of a task
    // sequence doesn't jump around but remains continuous.
    oldTime = busyTime;
    busyTime = Sys_GetRealSeconds() - busyTask->_startTime;
    if(busyTime > oldTime)
    {
        accumulatedBusyTime += busyTime - oldTime;
    }

    Sys_Lock(busy_Mutex);
    busyDoneCopy = busyDone;
    Sys_Unlock(busy_Mutex);

    if(!busyDoneCopy || (canUpload && GL_DeferredTaskCount() > 0) ||
       !Con_IsProgressAnimationCompleted())
    {
        // Let's keep running the busy loop.
        Window_Draw(Window_Main());
        return;
    }

    // Stop the loop.
    BusyTask_Exit();
}

/**
 * Draws the captured screenshot as a background, or just clears the screen if no
 * screenshot is available.
 */
static void Con_DrawScreenshotBackground(float x, float y, float width, float height)
{
    if(texScreenshot)
    {
        LIBDENG_ASSERT_IN_MAIN_THREAD();
        LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

        //GL_BindTextureUnmanaged(texScreenshot, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, texScreenshot);
        glEnable(GL_TEXTURE_2D);

        glColor3ub(255, 255, 255);
        glBegin(GL_QUADS);
            glTexCoord2f(0, 0);
            glVertex2f(x, y);
            glTexCoord2f(1, 0);
            glVertex2f(x + width, y);
            glTexCoord2f(1, 1);
            glVertex2f(x + width, y + height);
            glTexCoord2f(0, 1);
            glVertex2f(x, y + height);
        glEnd();

        glDisable(GL_TEXTURE_2D);
    }
    else
    {
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

#ifdef _DEBUG
#define _assertTexture(tex) { \
    GLint p; \
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &p); \
    Sys_GLCheckError(); \
    assert(p == tex); \
}
#else
#define _assertTexture(tex)
#endif

/**
 * @param 0...1, to indicate how far things have progressed.
 */
static void Con_BusyDrawIndicator(float x, float y, float radius, float pos)
{
    const float col[4] = {1.f, 1.f, 1.f, .25f};
    int i = 0, edgeCount = 0;
    int backW, backH;

    backW = backH = (radius * 2);

    pos = MINMAX_OF(0, pos, 1);
    edgeCount = MAX_OF(1, pos * 30);

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Draw a background.
    GL_BlendMode(BM_NORMAL);

    glBegin(GL_TRIANGLE_FAN);
        // Center.
        glColor4ub(0, 0, 0, 140);
        glVertex2f(x, y);
        glColor4ub(0, 0, 0, 0);
        // Vertices along the edge.
        glVertex2f(x, y - backH);
        glVertex2f(x + backW*.5f, y - backH*.8f);
        glVertex2f(x + backW*.8f, y - backH*.5f);
        glVertex2f(x + backW, y);
        glVertex2f(x + backW*.8f, y + backH*.5f);
        glVertex2f(x + backW*.5f, y + backH*.8f);
        glVertex2f(x, y + backH);
        glVertex2f(x - backW*.5f, y + backH*.8f);
        glVertex2f(x - backW*.8f, y + backH*.5f);
        glVertex2f(x - backW, y);
        glVertex2f(x - backW*.8f, y - backH*.5f);
        glVertex2f(x - backW*.5f, y - backH*.8f);
        glVertex2f(x, y - backH);
    glEnd();

    // Draw the frame.
    glEnable(GL_TEXTURE_2D);

    GL_BindTextureUnmanaged(texLoading[0], GL_LINEAR);
    _assertTexture(texLoading[0]);
    glColor4fv(col);
    GL_DrawRectf2(x - radius, y - radius, radius*2, radius*2);

    // Rotate around center.
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(.5f, .5f, 0.f);
    glRotatef(-accumulatedBusyTime * 20, 0.f, 0.f, 1.f);
    glTranslatef(-.5f, -.5f, 0.f);

    // Draw a fan.
    glColor4f(col[0], col[1], col[2], .5f);
    _assertTexture(texLoading[0]);
    GL_BindTextureUnmanaged(texLoading[1], GL_LINEAR);
    _assertTexture(texLoading[1]);
    glBegin(GL_TRIANGLE_FAN);
    // Center.
    glTexCoord2f(.5f, .5f);
    glVertex2f(x, y);
    // Vertices along the edge.
    for(i = 0; i <= edgeCount; ++i)
    {
        float angle = 2 * PI * pos * (i / (float)edgeCount) + PI/2;
        glTexCoord2f(.5f + cos(angle)*.5f, .5f + sin(angle)*.5f);
        glVertex2f(x + cos(angle)*radius*1.05f, y + sin(angle)*radius*1.05f);
    }
    glEnd();
    _assertTexture(texLoading[1]);

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    // Draw the task name.
    if(busyTask->name)
    {
        FR_SetFont(busyFont);
        FR_LoadDefaultAttrib();
        FR_SetColorAndAlpha(1.f, 1.f, 1.f, .66f);
        FR_DrawTextXY3(busyTask->name, x+radius*1.15f, y, ALIGN_LEFT, DTF_ONLY_SHADOW);
    }

    glDisable(GL_TEXTURE_2D);
}

#define LINE_COUNT 4

/**
 * @return              Number of new lines since the old ones.
 */
static int GetBufLines(CBuffer* buffer, cbline_t const **oldLines)
{
    cbline_t const*     bufLines[LINE_COUNT + 1];
    int                 count;
    int                 newCount = 0;
    int                 i, k;

    count = CBuffer_GetLines2(buffer, LINE_COUNT, -LINE_COUNT, bufLines, BLF_OMIT_RULER|BLF_OMIT_EMPTYLINE);
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
 * @todo: Wow. I had some weird time hacking the smooth scrolling. Cleanup would be
 *         good some day. -jk
 */
void Con_BusyDrawConsoleOutput(void)
{
    CBuffer*          buffer;
    static cbline_t const* visibleBusyLines[2 * LINE_COUNT];
    static float        scroll = 0;
    static float        scrollStartTime = 0;
    static float        scrollEndTime = 0;
    static double       lastNewTime = 0;
    static double       timeSinceLastNew = 0;
    double              nowTime = 0;
    float               y, topY;
    uint                i, newCount;

    buffer = Con_HistoryBuffer();
    if(!buffer) return;

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

    LIBDENG_ASSERT_IN_MAIN_THREAD();

    GL_BlendMode(BM_NORMAL);

    // Dark gradient as background.
    glBegin(GL_QUADS);
    glColor4ub(0, 0, 0, 0);
    y = Window_Height(theWindow) - (LINE_COUNT + 3) * busyFontHgt;
    glVertex2f(0, y);
    glVertex2f(Window_Width(theWindow), y);
    glColor4ub(0, 0, 0, 128);
    glVertex2f(Window_Width(theWindow), Window_Height(theWindow));
    glVertex2f(0, Window_Height(theWindow));
    glEnd();

    glEnable(GL_TEXTURE_2D);

    // The text lines.
    topY = y = Window_Height(theWindow) - busyFontHgt * (2 * LINE_COUNT + .5f);
    if(newCount > 0 ||
       (nowTime >= scrollStartTime && nowTime < scrollEndTime && scrollEndTime > scrollStartTime))
    {
        if(scrollEndTime - scrollStartTime > 0)
            y += scroll * (scrollEndTime - nowTime) / (scrollEndTime - scrollStartTime) *
                busyFontHgt;
    }

    FR_SetFont(busyFont);
    FR_LoadDefaultAttrib();
    FR_SetColor(1.f, 1.f, 1.f);

    for(i = 0; i < 2 * LINE_COUNT; ++i, y += busyFontHgt)
    {
        const cbline_t* line = visibleBusyLines[i];
        float alpha = 1;

        if(!line)
            continue;

        alpha = ((y - topY) / busyFontHgt) - (LINE_COUNT - 1);
        if(alpha < LINE_COUNT)
            alpha = MINMAX_OF(0, alpha/2, 1);
        else
            alpha = 1 - (alpha - LINE_COUNT);

        FR_SetAlpha(alpha);
        FR_DrawTextXY3(line->text, Window_Width(theWindow)/2, y, ALIGN_TOP, DTF_ONLY_SHADOW);
    }

    glDisable(GL_TEXTURE_2D);

#undef LINE_COUNT
}

/**
 * Busy drawer function. The entire frame is drawn here.
 */
static void BusyTask_Drawer(void)
{
    float pos = 0;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);

    Con_DrawScreenshotBackground(0, 0, Window_Width(theWindow), Window_Height(theWindow));

    // Indefinite activity?
    if((busyTask->mode & BUSYF_ACTIVITY) || (busyTask->mode & BUSYF_PROGRESS_BAR))
    {
        if(busyTask->mode & BUSYF_ACTIVITY)
            pos = 1;
        else // The progress is animated elsewhere.
            pos = Con_GetProgress();

        Con_BusyDrawIndicator(Window_Width(theWindow)/2,
                              Window_Height(theWindow)/2,
                              Window_Height(theWindow)/12, pos);
    }

    // Output from the console?
    if(busyTask->mode & BUSYF_CONSOLE_OUTPUT)
    {
        Con_BusyDrawConsoleOutput();
    }

#ifdef _DEBUG
    Z_DebugDrawer();
#endif

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    // The frame is ready to be shown.
    Window_SwapBuffers(Window_Main());
}

boolean Con_TransitionInProgress(void)
{
    return transition.inProgress;
}

static void Con_EndTransition(void)
{
    if(!transition.inProgress) return;

    // Clear any input events that might have accumulated during the transition.
    DD_ClearEvents();
    B_ActivateContext(B_ContextByName(UI_BINDING_CONTEXT_NAME), false);

    Con_ReleaseScreenshotTexture();
    transition.inProgress = false;
}

void Con_TransitionTicker(timespan_t ticLength)
{
    if(isDedicated) return;
    if(!Con_TransitionInProgress()) return;

    transition.position = (float)(Sys_GetTime() - transition.startTime) / transition.tics;
    if(transition.position >= 1)
    {
        Con_EndTransition();
    }
}

static void seedDoomWipeSine(void)
{
    int i;

    doomWipeSine[0] = (128 - RNG_RandByte()) / 128.f;
    for(i = 1; i < DOOMWIPESINE_NUMSAMPLES; ++i)
    {
        float r = (128 - RNG_RandByte()) / 512.f;
        doomWipeSine[i] = MINMAX_OF(-1, doomWipeSine[i-1] + r, 1);
    }
}

static float sampleDoomWipeSine(float point)
{
    float sample = doomWipeSine[(uint)ROUND((DOOMWIPESINE_NUMSAMPLES-1) * MINMAX_OF(0, point, 1))];
    float offset = SCREENHEIGHT * transition.position * transition.position;
    return offset + (SCREENHEIGHT/2) * (transition.position + sample) * transition.position * transition.position;
}

static void sampleDoomWipe(void)
{
    int i;
    for(i = 0; i <= SCREENWIDTH; ++i)
    {
        doomWipeSamples[i] = MAX_OF(0, sampleDoomWipeSine((float) i / SCREENWIDTH));
    }
}

void Con_DrawTransition(void)
{
    if(isDedicated) return;
    if(!Con_TransitionInProgress()) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, SCREENWIDTH, SCREENHEIGHT, 0, -1, 1);

    GL_BindTextureUnmanaged(texScreenshot, GL_LINEAR);
    glEnable(GL_TEXTURE_2D);

    switch(transition.style)
    {
    case TS_DOOMSMOOTH: {
        float topAlpha, s, div, colWidth = 1.f / SCREENWIDTH;
        int x, y, h, i;

        sampleDoomWipe();
        div = 1 - .25f * transition.position;
        topAlpha = 1 - transition.position;
        topAlpha *= topAlpha;

        h = SCREENHEIGHT * (1 - div);
        x = 0;
        s = 0;

        glBegin(GL_QUAD_STRIP);
        for(i = 0; i <= SCREENWIDTH; ++i, x++, s += colWidth)
        {
            y = doomWipeSamples[i];

            glColor4f(1, 1, 1, topAlpha);
            glTexCoord2f(s, 0); glVertex2i(x, y);
            glColor4f(1, 1, 1, 1);
            glTexCoord2f(s, 1-div); glVertex2i(x, y + h);
        }
        glEnd();

        x = 0;
        s = 0;

        glColor4f(1, 1, 1, 1);
        glBegin(GL_QUAD_STRIP);
        for(i = 0; i <= SCREENWIDTH; ++i, x++, s += colWidth)
        {
            y = doomWipeSamples[i] + h;

            glTexCoord2f(s, 1-div); glVertex2i(x, y);
            glTexCoord2f(s, 1); glVertex2i(x, y + (SCREENHEIGHT - h));
        }
        glEnd();
        break;
      }
    case TS_DOOM: {
        // As above but drawn with whole pixel columns.
        float s = 0, colWidth = 1.f / SCREENWIDTH;
        int x = 0, y, i;

        sampleDoomWipe();

        glColor4f(1, 1, 1, 1);
        glBegin(GL_QUADS);
        for(i = 0; i <= SCREENWIDTH; ++i, x++, s+= colWidth)
        {
            y = doomWipeSamples[i];

            glTexCoord2f(s, 0); glVertex2i(x, y);
            glTexCoord2f(s+colWidth, 0); glVertex2i(x+1, y);
            glTexCoord2f(s+colWidth, 1); glVertex2i(x+1, y+SCREENHEIGHT);
            glTexCoord2f(s, 1); glVertex2i(x, y+SCREENHEIGHT);
        }
        glEnd();
        break;
      }
    case TS_CROSSFADE:
        glColor4f(1, 1, 1, 1 - transition.position);

        glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex2f(0, 0);
            glTexCoord2f(0, 1); glVertex2f(0, SCREENHEIGHT);
            glTexCoord2f(1, 1); glVertex2f(SCREENWIDTH, SCREENHEIGHT);
            glTexCoord2f(1, 0); glVertex2f(SCREENWIDTH, 0);
        glEnd();
        break;
    }

    GL_SetNoTexture();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
