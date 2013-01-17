/**
 * @file busyvisual.c
 * Busy Mode visualizer. @ingroup render
 *
 * @authors Copyright © 2007-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
#include "de_ui.h"

#include "cbuffer.h"
#include "gl/texturecontent.h"
#include "resource/image.h"
#include "resource/font.h"
#include "resource/fonts.h"
#include "render/rend_font.h"
#include "ui/busyvisual.h"

static fontid_t busyFont = 0;
static int busyFontHgt; // Height of the font.

static DGLuint texLoading[2];
static DGLuint texScreenshot; // Captured screenshot of the latest frame.

static void releaseScreenshotTexture(void)
{
    glDeleteTextures(1, (const GLuint*) &texScreenshot);
    texScreenshot = 0;
}

static void acquireScreenshotTexture(void)
{
#ifdef _DEBUG
    timespan_t startTime;
#endif

    if(texScreenshot)
    {
        releaseScreenshotTexture();
    }

#ifdef _DEBUG
    startTime = Timer_RealSeconds();
#endif

    texScreenshot = Window_GrabAsTexture(Window_Main(), true /*halfsized*/);

    DEBUG_Message(("Busy Mode: Took %.2f seconds acquiring screenshot texture #%i.\n",
                   Timer_RealSeconds() - startTime, texScreenshot));
}

void BusyVisual_ReleaseTextures(void)
{
    if(novideo) return;

    glDeleteTextures(2, (const GLuint*) texLoading);
    texLoading[0] = texLoading[1] = 0;

    // Don't release yet if doing a transition.
    if(!Con_TransitionInProgress())
    {
        releaseScreenshotTexture();
    }

    busyFont = 0;
}

void BusyVisual_PrepareResources(void)
{
    BusyTask* task = BusyMode_CurrentTask();

    if(isDedicated || novideo || !task) return;

    if(!(task->mode & BUSYF_STARTUP))
    {
        // Not in startup, so take a copy of the current frame contents.
        acquireScreenshotTexture();
    }
}

void BusyVisual_PrepareFont(void)
{
    if(!novideo)
    {
        /**
         * @todo At the moment this is called from preBusySetup() so that the font
         * is present throughout the busy mode during all the individual tasks.
         * Previously the font was being prepared at the beginning of each task,
         * but that was resulting in a rendering glitch where the font GL texture
         * was not being properly drawn on screen during the first ~1 second of
         * BusyVisual visibility. The exact cause was not determined, but it may be
         * due to a conflict with the fonts being prepared from both the main
         * thread and the worker thread, or because the GL deferring mechanism is
         * interfering somehow.
         */

        // These must be real files in the base dir because virtual files haven't
        // been loaded yet when the engine startup is done.
        static const struct busyfont_s {
            const char* name;
            const char* path;
        } fonts[] = {
            { "System:normal12", "}data/fonts/normal12.dfn" },
            { "System:normal18", "}data/fonts/normal18.dfn" }
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
        else
        {
            busyFont = 0;
            busyFontHgt = 0;
        }
    }
}

void BusyVisual_LoadTextures(void)
{
    image_t image;
    if(novideo) return;

    if(GL_LoadImage(&image, "}data/graphics/loading1.png"))
    {
        texLoading[0] = GL_NewTextureWithParams(DGL_RGBA, image.size.width, image.size.height, image.pixels, TXCF_NEVER_DEFER);
        Image_Destroy(&image);
    }

    if(GL_LoadImage(&image, "}data/graphics/loading2.png"))
    {
        texLoading[1] = GL_NewTextureWithParams(DGL_RGBA, image.size.width, image.size.height, image.pixels, TXCF_NEVER_DEFER);
        Image_Destroy(&image);
    }
}

/**
 * Draws the captured screenshot as a background, or just clears the screen if no
 * screenshot is available.
 */
static void drawBackground(float x, float y, float width, float height)
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

/**
 * @param pos  Position (0...1) indicating how far things have progressed.
 */
static void drawPositionIndicator(float x, float y, float radius, float pos,
    const char* taskName)
{
    const timespan_t accumulatedBusyTime = BusyMode_ElapsedTime();
    const float col[4] = {1.f, 1.f, 1.f, .25f};
    const int backW = (radius * 2);
    const int backH = (radius * 2);
    int i, edgeCount;

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
    LIBDENG_ASSERT_GL_TEXTURE_ISBOUND(texLoading[0]);
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
    LIBDENG_ASSERT_GL_TEXTURE_ISBOUND(texLoading[0]);
    GL_BindTextureUnmanaged(texLoading[1], GL_LINEAR);
    LIBDENG_ASSERT_GL_TEXTURE_ISBOUND(texLoading[1]);
    glBegin(GL_TRIANGLE_FAN);
    // Center.
    glTexCoord2f(.5f, .5f);
    glVertex2f(x, y);
    // Vertices along the edge.
    for(i = 0; i <= edgeCount; ++i)
    {
        float angle = 2 * de::PI * pos * (i / (float)edgeCount) + de::PI/2;
        glTexCoord2f(.5f + cos(angle)*.5f, .5f + sin(angle)*.5f);
        glVertex2f(x + cos(angle)*radius*1.05f, y + sin(angle)*radius*1.05f);
    }
    glEnd();
    LIBDENG_ASSERT_GL_TEXTURE_ISBOUND(texLoading[1]);

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    // Draw the task name.
    if(taskName)
    {
        FR_SetFont(busyFont);
        FR_LoadDefaultAttrib();
        FR_SetColorAndAlpha(1.f, 1.f, 1.f, .66f);
        FR_DrawTextXY3(taskName, x+radius*1.15f, y, ALIGN_LEFT, DTF_ONLY_SHADOW);
    }

    glDisable(GL_TEXTURE_2D);
}

#define LINE_COUNT 4

/**
 * @return  Number of new lines since the old ones.
 */
static int GetBufLines(CBuffer* buffer, cbline_t const **oldLines)
{
    cbline_t const* bufLines[LINE_COUNT + 1];
    const int count = CBuffer_GetLines2(buffer, LINE_COUNT, -LINE_COUNT, bufLines, BLF_OMIT_RULER|BLF_OMIT_EMPTYLINE);
    int i, k, newCount;

    newCount = 0;
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
        {
            oldLines[k] = oldLines[k+1];
        }
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
static void drawConsoleOutput(void)
{
    CBuffer* buffer;
    static cbline_t const* visibleBusyLines[2 * LINE_COUNT];
    static float scroll = 0;
    static float scrollStartTime = 0;
    static float scrollEndTime = 0;
    static double lastNewTime = 0;
    static double timeSinceLastNew = 0;
    double nowTime = 0;
    float y, topY;
    uint i, newCount;

    buffer = Con_HistoryBuffer();
    if(!buffer) return;

    newCount = GetBufLines(buffer, visibleBusyLines);
    nowTime = Timer_RealSeconds();
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
            double interval = MIN_OF(timeSinceLastNew/2, 1.3);

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

        if(!line) continue;

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

void BusyVisual_Render(void)
{
    BusyTask* task = BusyMode_CurrentTask();
    float pos = 0;

    if(!task) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);

    drawBackground(0, 0, Window_Width(theWindow), Window_Height(theWindow));

    // Indefinite activity?
    if((task->mode & BUSYF_ACTIVITY) || (task->mode & BUSYF_PROGRESS_BAR))
    {
        if(task->mode & BUSYF_ACTIVITY)
            pos = 1;
        else // The progress is animated elsewhere.
            pos = Con_GetProgress();

        drawPositionIndicator(Window_Width(theWindow)/2,
                              Window_Height(theWindow)/2,
                              Window_Height(theWindow)/12, pos, task->name);
    }

    // Output from the console?
    if(task->mode & BUSYF_CONSOLE_OUTPUT)
    {
        drawConsoleOutput();
    }

#ifdef _DEBUG
    Z_DebugDrawer();
#endif

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    // The frame is ready to be shown.
    Window_SwapBuffers(Window_Main());
}

/**
 * Transition effect:
 */
#include "de_misc.h"

#define DOOMWIPESINE_NUMSAMPLES 320

static void seedDoomWipeSine(void);

int rTransition = (int) TS_CROSSFADE; ///< cvar Default transition style.
int rTransitionTics = 28; ///< cvar Default transition duration (in tics).

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

void Con_TransitionRegister(void)
{
    C_VAR_INT("con-transition", &rTransition, 0, FIRST_TRANSITIONSTYLE, LAST_TRANSITIONSTYLE);
    C_VAR_INT("con-transition-tics", &rTransitionTics, 0, 0, 60);
}

void Con_TransitionConfigure(void)
{
    transition.tics = rTransitionTics;
    transition.style = (transitionstyle_t) rTransition;
    if(transition.style == TS_DOOM || transition.style == TS_DOOMSMOOTH)
    {
        seedDoomWipeSine();
    }
    transition.inProgress = true;
}

void Con_TransitionBegin(void)
{
    transition.startTime = Timer_Ticks();
    transition.position = 0;
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

    releaseScreenshotTexture();
    transition.inProgress = false;
}

void Con_TransitionTicker(timespan_t ticLength)
{
    if(isDedicated) return;
    if(!Con_TransitionInProgress()) return;

    transition.position = (float)(Timer_Ticks() - transition.startTime) / transition.tics;
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
