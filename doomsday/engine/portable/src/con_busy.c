/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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

static void     Con_BusyLoop(void);
static void     Con_BusyDrawer(void);
static void     Con_BusyLoadTextures(void);
static void     Con_BusyDeleteTextures(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

/*
boolean         startupScreen = false;
int             startupLogo;
*/

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean  busyInited;
static int      busyMode;
static thread_t busyThread;
static timespan_t busyTime;
static volatile boolean busyDone;
static volatile const char* busyError = NULL;
static float    busyProgress = 0;
static int      busyFont = 0;
static int      busyFontHgt;        // Height of the font.

static DGLuint  texLoading[2];
static DGLuint  texScreenshot;      // Captured screenshot of the latest frame.

static char *titleText = "";
static char secondaryTitleText[256];
static char statusText[256];

// CODE --------------------------------------------------------------------

/**
 * Busy mode.
 *
 * @param flags  Busy mode flags (see BUSYF_PROGRESS_BAR and others).
 * @param worker  Worker thread that does processing while in busy mode.
 * @param workerData  Data context for the worker thread.
 *
 * @return  Return value of the worker.
 */
int Con_Busy(int flags, busyworkerfunc_t worker, void *workerData)
{
    int result = 0;
    
    if(busyInited)
    {
        Con_Error("Con_Busy: Already busy.\n");
    }

    busyMode = flags;
    busyDone = false;

    // Load any textures needed in this mode.
    Con_BusyLoadTextures();

    busyInited = true;

    // Start the busy worker thread, which will proces things in the 
    // background while we keep the user occupied with nice animations.
    busyThread = Sys_StartThread(worker, workerData);
    
    // Wait for the busy thread to stop.
    Con_BusyLoop();

    // Free resources.
    Con_BusyDeleteTextures();

    if(busyError)
    {
        Con_AbnormalShutdown((const char*) busyError);
    }
    
    // Make sure the worker finishes before we continue.
    result = Sys_WaitThread(busyThread);   
    busyThread = NULL;
    busyInited = false;
    
    // Make sure that any remaining deferred content gets uploaded.
    if(!(busyMode & BUSYF_NO_UPLOADS))
    {
        GL_UploadDeferredContent(0);
    }
    
    return result;
}

/**
 * Called by the busy worker to shutdown the engine immediately.
 * 
 * @param message  Message, expected to exist until the engine closes.
 */
void Con_BusyWorkerError(const char* message)
{
    busyError = message;
    Con_BusyWorkerEnd();
}

/**
 * Called by the busy worker thread when it has finished processing, 
 * to end busy mode.
 */
void Con_BusyWorkerEnd(void)
{
    if(!busyInited)
        return;
    
    busyDone = true;
}

boolean Con_IsBusy(void)
{
    return busyInited;
}

static void Con_BusyLoadTextures(void)
{
    image_t image;

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
        if(GL_LoadImage(&image, "}data/graphics/loading1.png", false))
        {
            texLoading[0] = GL_NewTextureWithParams(DGL_RGBA, image.width, image.height, 
                                                    image.pixels, TXCF_NEVER_DEFER);
            GL_DestroyImage(&image);
        }

        if(GL_LoadImage(&image, "}data/graphics/loading2.png", false))
        {
            texLoading[1] = GL_NewTextureWithParams(DGL_RGBA, image.width, image.height, 
                                                    image.pixels, TXCF_NEVER_DEFER);
            GL_DestroyImage(&image);
        }        
    }    
    
    if(busyMode & BUSYF_CONSOLE_OUTPUT)
    {
        if(FR_PrepareFont("normal12"))
        {
            busyFont = FR_GetCurrent();
            busyFontHgt = FR_TextHeight("A");
        }
    }
}

static void Con_BusyDeleteTextures(void)
{
    // Destroy the font.
    if(busyFont)
    {
        FR_DestroyFont(busyFont);
    }
    
    gl.DeleteTextures(2, texLoading);
    texLoading[0] = texLoading[1] = 0;
    
    busyFont = 0;
    
    // Don't release this yet if doing a wipe.
    Con_ReleaseScreenshotTexture();
}

/**
 * Take a screenshot and store it as a texture.
 */
void Con_AcquireScreenshotTexture(void)
{
    int         oldMaxTexSize = glMaxTexSize;
    byte       *frame;
#ifdef _DEBUG
    timespan_t startTime;
#endif
    
    if(texScreenshot)
    {
        Con_ReleaseScreenshotTexture();
    }
    
#ifdef _DEBUG
    startTime = Sys_GetRealSeconds();
#endif

    frame = M_Malloc(glScreenWidth * glScreenHeight * 3);
    gl.Grab(0, 0, glScreenWidth, glScreenHeight, DGL_RGB, frame);
    glMaxTexSize = 512; // A bit of a hack, but don't use too large a texture.
    texScreenshot = GL_UploadTexture(frame, glScreenWidth, glScreenHeight,
                                     false, false, true, false, true,
                                     DGL_LINEAR, DGL_LINEAR, DGL_CLAMP, DGL_CLAMP,
                                     TXCF_NEVER_DEFER);
    glMaxTexSize = oldMaxTexSize;
    M_Free(frame);
    
#ifdef _DEBUG
    printf("Con_AcquireScreenshotTexture: Took %.2f seconds.\n", 
           Sys_GetRealSeconds() - startTime);
#endif
}

void Con_ReleaseScreenshotTexture(void)
{
    gl.DeleteTextures(1, &texScreenshot);
    texScreenshot = 0;
}

/**
 * The busy thread main function. Runs until busyDone set to true.
 */
static void Con_BusyLoop(void)
{
    timespan_t startTime = Sys_GetRealSeconds();

    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, glScreenWidth, glScreenHeight, -1, 1);
    
    while(!busyDone || (!(busyMode & BUSYF_NO_UPLOADS) && GL_GetDeferredCount() > 0))
    {
        Sys_Sleep(20);
        
        // Make sure that any deferred content gets uploaded.
        if(!(busyMode & BUSYF_NO_UPLOADS))
        {
            GL_UploadDeferredContent(15);
        }
        
        // Update the time.
        busyTime = Sys_GetRealSeconds() - startTime;
        
        // Time for an update?
        Con_BusyDrawer();
    }

    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();    
    
    if(verbose)
    {
        Con_Message("Con_Busy: Was busy for %.2lf seconds.\n", busyTime);
    }
}

/**
 * Draws the captured screenshot as a background, or just clears the screen if no
 * screenshot is available.
 */
static void Con_DrawScreenshotBackground(void)
{
    if(texScreenshot)
    {
        gl.Enable(DGL_TEXTURING);
        gl.Bind(texScreenshot);
        gl.Color3ub(255, 255, 255);
        gl.Begin(DGL_QUADS);
        gl.TexCoord2f(0, 1);
        gl.Vertex2f(0, 0);
        gl.TexCoord2f(1, 1);
        gl.Vertex2f(glScreenWidth, 0);
        gl.TexCoord2f(1, 0);
        gl.Vertex2f(glScreenWidth, glScreenHeight);
        gl.TexCoord2f(0, 0);
        gl.Vertex2f(0, glScreenHeight);
        gl.End();
    }
    else
    {
        gl.Clear(DGL_COLOR_BUFFER_BIT);
    }
}

/**
 * @param 0...1, to indicate how far things have progressed.
 */
static void Con_BusyDrawIndicator(float pos)
{
    int x = glScreenWidth/2;
    int y = glScreenHeight/2;
    int radius = glScreenHeight/16;
    float col[4] = {
         1.f, 1.f, 1.f, .2f
    };
    int i = 0;
    int edgeCount = 0;
    int backW = glScreenWidth/12;
    int backH = glScreenHeight/12;
    
    pos = MINMAX_OF(0, pos, 1);
    edgeCount = MAX_OF(1, pos * 30);
    
    // Draw a background.
    gl.Disable(DGL_TEXTURING);
    gl.Enable(DGL_BLENDING);
    gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);

    gl.Begin(DGL_TRIANGLE_FAN);
    gl.Color4ub(0, 0, 0, 128);
    gl.Vertex2f(x, y);
    gl.Color4ub(0, 0, 0, 0);
    gl.Vertex2f(x, y - backH);
    gl.Vertex2f(x + backW*.8f, y - backH*.8f);
    gl.Vertex2f(x + backW, y);
    gl.Vertex2f(x + backW*.8f, y + backH*.8f);
    gl.Vertex2f(x, y + backH);
    gl.Vertex2f(x - backW*.8f, y + backH*.8f);
    gl.Vertex2f(x - backW, y);
    gl.Vertex2f(x - backW*.8f, y - backH*.8f);
    gl.Vertex2f(x, y - backH);
    gl.End();
    
    // Draw the frame.
    gl.Enable(DGL_TEXTURING);

    gl.Bind(texLoading[0]);
    GL_DrawRect(x - radius, y - radius, radius*2, radius*2, col[0], col[1], col[2], col[3]);
    
    // Rotate around center.
    gl.MatrixMode(DGL_TEXTURE);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Translatef(.5f, .5f, 0.f);
    gl.Rotatef(-busyTime * 20, 0.f, 0.f, 1.f);
    gl.Translatef(-.5f, -.5f, 0.f);

    // Draw a fan.
    gl.Bind(texLoading[1]);
    gl.Begin(DGL_TRIANGLE_FAN);
    // Center.
    gl.TexCoord2f(.5f, .5f);
    gl.Vertex2f(x, y);
    // Vertices along the edge.
    for(i = 0; i <= edgeCount; ++i)
    {
        float angle = 2 * PI * pos * (i / (float)edgeCount) + PI/2;
        gl.TexCoord2f(.5f + cos(angle)*.5f, .5f + sin(angle)*.5f);
        gl.Vertex2f(x + cos(angle)*radius, y + sin(angle)*radius);
    }
    gl.End();
    
    gl.MatrixMode(DGL_TEXTURE);
    gl.PopMatrix();
}

/**
 * Draws a number of console output to the bottom of the screen.
 */
static void Con_BusyDrawConsoleOutput(void)
{
#define LINE_COUNT 3
    
    const char *lines[LINE_COUNT];
    int i, y;
    
    lines[0] = "First line of console output.";
    lines[1] = "Con_BusyDrawConsoleOutput: This is some output.";
    lines[2] = "The bottom-most line.";

    gl.Disable(DGL_TEXTURING);
    gl.Enable(DGL_BLENDING);
    gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);

    gl.Begin(DGL_QUADS);
    gl.Color4ub(0, 0, 0, 0);
    y = glScreenHeight - (LINE_COUNT + 3) * busyFontHgt;
    gl.Vertex2f(0, y);
    gl.Vertex2f(glScreenWidth, y);
    gl.Color4ub(0, 0, 0, 128);
    gl.Vertex2f(glScreenWidth, glScreenHeight);
    gl.Vertex2f(0, glScreenHeight);
    gl.End();
    
    gl.Enable(DGL_TEXTURING);
    
    y = glScreenHeight - busyFontHgt * (LINE_COUNT + .5f);
    for(i = 0; i < LINE_COUNT; ++i, y += busyFontHgt)
    {
        float color = 1.f / (LINE_COUNT - i);
        gl.Color4f(1.f, 1.f, 1.f, color);
        FR_TextOut(lines[i], (glScreenWidth - FR_TextWidth(lines[i]))/2, y);
    }
    
#undef LINE_COUNT
}

/**
 * Busy drawer function. The entire frame is drawn here.
 */
static void Con_BusyDrawer(void)
{
    DGLuint oldBinding = 0;
//    char buf[100];
    
    Con_DrawScreenshotBackground();
    
    // Indefinite activity?
    if(busyMode & BUSYF_ACTIVITY)
    {
        Con_BusyDrawIndicator(1);
    }
    else if(busyMode & BUSYF_PROGRESS_BAR)
    {
        // The progress is animated elsewhere.
        Con_BusyDrawIndicator(Con_GetProgress());
    }
    
    // Output from the console?
    if(busyMode & BUSYF_CONSOLE_OUTPUT)
    {
        Con_BusyDrawConsoleOutput();
    }
    
    /*sprintf(buf, "Busy %04x : %.2lf", busyMode, busyTime);
    gl.Color4f(1, 1, 1, 1);
    FR_TextOut(buf, 20, 20);*/
    
    // Swap buffers.
    gl.Show();
}

#if 0
/*
 * The startup screen mode is used during engine startup.  In startup
 * mode, the whole screen is used for console output.
 */
void Con_StartupInit(void)
{
    static boolean firstTime = true;

    if(novideo)
        return;

    GL_InitVarFont();
    fontHgt = FR_SingleLineHeight("Doomsday!");

    startupScreen = true;

    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, glScreenWidth, glScreenHeight, -1, 1);

    if(firstTime)
    {
        titleText = "Doomsday " DOOMSDAY_VERSION_TEXT " Startup";
        firstTime = false;
    }
    else
    {
        titleText = "Doomsday " DOOMSDAY_VERSION_TEXT;
    }

    // Load graphics.
    startupLogo = GL_LoadGraphics("Background", LGM_GRAYSCALE);
}

void Con_StartupDone(void)
{
    if(isDedicated)
        return;
    titleText = "Doomsday " DOOMSDAY_VERSION_TEXT;
    startupScreen = false;
    gl.DeleteTextures(1, (DGLuint*) &startupLogo);
    startupLogo = 0;
    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
    GL_ShutdownVarFont();

    // Update the secondary title and the game status.
    strncpy(secondaryTitleText, (char *) gx.GetVariable(DD_GAME_ID),
            sizeof(secondaryTitleText) - 1);
    strncpy(statusText, (char *) gx.GetVariable(DD_GAME_MODE),
            sizeof(statusText) - 1);
}

/*
 * Background with the "The Doomsday Engine" text superimposed.
 *
 * @param alpha  Alpha level to use when drawing the background.
 */
void Con_DrawStartupBackground(float alpha)
{
    float   mul = (startupLogo ? 1.5f : 1.0f);
    ui_color_t *dark = UI_COL(UIC_BG_DARK), *light = UI_COL(UIC_BG_LIGHT);

    // Background gradient picture.
    gl.Bind(startupLogo);
    if(alpha < 1.0)
    {
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        gl.Disable(DGL_BLENDING);
    }
    gl.Begin(DGL_QUADS);
    // Top color.
    gl.Color4f(dark->red * mul, dark->green * mul, dark->blue * mul, alpha);
    gl.TexCoord2f(0, 0);
    gl.Vertex2f(0, 0);
    gl.TexCoord2f(1, 0);
    gl.Vertex2f(glScreenWidth, 0);
    // Bottom color.
    gl.Color4f(light->red * mul, light->green * mul, light->blue * mul, alpha);
    gl.TexCoord2f(1, 1);
    gl.Vertex2f(glScreenWidth, glScreenHeight);
    gl.TexCoord2f(0, 1);
    gl.Vertex2f(0, glScreenHeight);
    gl.End();
    gl.Enable(DGL_BLENDING);
}

/*
 * Draws the title bar of the console.
 *
 * @return  Title bar height.
 */
int Con_DrawTitle(float alpha)
{
    int width = 0;
    int height = UI_TITLE_HGT;

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
    gl.LoadIdentity();

    FR_SetFont(glFontVariable[GLFS_BOLD]);
    height = FR_TextHeight("W") + UI_BORDER;
    UI_DrawTitleEx(titleText, height, alpha);
    if(secondaryTitleText[0])
    {
        width = FR_TextWidth(titleText) + FR_TextWidth("  ");
        FR_SetFont(glFontVariable[GLFS_LIGHT]);
        UI_TextOutEx(secondaryTitleText, UI_BORDER + width, height / 2,
                     false, true, UI_COL(UIC_TEXT), .75f * alpha);
    }
    if(statusText[0])
    {
        width = FR_TextWidth(statusText);
        FR_SetFont(glFontVariable[GLFS_LIGHT]);
        UI_TextOutEx(statusText, glScreenWidth - UI_BORDER - width, height / 2,
                     false, true, UI_COL(UIC_TEXT), .75f * alpha);
    }

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();

    FR_SetFont(glFontFixed);
    return height;
}

/*
 * Draw the background and the current console output.
 */
void Con_DrawStartupScreen(int show)
{
    int         vislines, y, x;
    int         topy;
    uint        i, count;
    cbuffer_t  *buffer;
    static cbline_t **lines = NULL;
    static int  bufferSize = 0;
    cbline_t   *line;

    // Print the messages in the console.
    if(!startupScreen || ui_active)
        return;

    //gl.MatrixMode(DGL_PROJECTION);
    //gl.LoadIdentity();
    //gl.Ortho(0, 0, glScreenWidth, glScreenHeight, -1, 1);

    Con_DrawStartupBackground(1.0);

    // Draw the title.
    topy = Con_DrawTitle(1.0);

    topy += UI_BORDER;
    vislines = (glScreenHeight - topy + fontHgt / 2) / fontHgt;
    y = topy;

    buffer = Con_GetConsoleBuffer();
    if(vislines > 0)
    {
        // Need to enlarge the buffer?
        if(vislines > bufferSize)
        {
            lines = Z_Realloc(lines, sizeof(cbline_t *) * (vislines + 1),
                              PU_STATIC);
            bufferSize = vislines;
        }

        count = Con_BufferGetLines(buffer, vislines, -vislines, lines);
        if(count > 0)
        {
            for(i = 0; i < count - 1; ++i)
            {
                line = lines[i];

                if(!line)
                    break;
                if(line->flags & CBLF_RULER)
                {
                    Con_DrawRuler(y, fontHgt, 1);
                }
                else
                {
                    if(!line->text)
                        continue;

                    x = (line->flags & CBLF_CENTER ?
                            (glScreenWidth - FR_TextWidth(line->text)) / 2 : 3);
                    //gl.Color3f(0, 0, 0);
                    //FR_TextOut(line->text, x + 1, y + 1);
                    gl.Color3f(1, 1, 1);
                    FR_CustomShadowTextOut(line->text, x, y, 1, 1, 1);
                }
                y += fontHgt;
            }
        }
    }
    if(show)
    {
        // Update the progress bar, if one is active.
        //Con_Progress(0, PBARF_NOBACKGROUND | PBARF_NOBLIT);
        gl.Show();
    }
}
#endif
