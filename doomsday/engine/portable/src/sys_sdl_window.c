/**\file sys_sdl_window.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2008 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * Cross-platform, SDL-based window management.
 *
 * This code wraps SDL window management routines in order to provide
 * common behavior. The availabilty of features and behavioral traits can
 * be queried for.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_syswm.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_ui.h"

#include "rend_particle.h" // \todo Should not be necessary at this level.

// MACROS ------------------------------------------------------------------

#define LINELEN                 (1024)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean setDDWindow(ddwindow_t *win, int newWidth, int newHeight,
                           int newBPP, uint wFlags, uint uFlags);
static void setConWindowCmdLine(uint idx, const char *text,
                                unsigned int cursorPos, int flags);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Currently active window where all drawing operations are directed at.
const ddwindow_t* theWindow;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean winManagerInited = false;

static ddwindow_t mainWindow;
static boolean mainWindowInited = false;

static int screenWidth, screenHeight, screenBPP;

#if defined(UNIX)
static WINDOW* cursesRootWin;
#endif

// CODE --------------------------------------------------------------------

static __inline ddwindow_t *getWindow(uint idx)
{
    if(!winManagerInited)
        return NULL; // Window manager is not initialized.

    if(idx != 0)
        return NULL;

    return &mainWindow;
}

#if defined(UNIX)
static void setAttrib(int flags)
{
    if(!mainWindowInited)
        return;

    if(flags & (CPF_YELLOW | CPF_LIGHT))
        wattrset(mainWindow.console.winText, A_BOLD);
    else
        wattrset(mainWindow.console.winText, A_NORMAL);
}

/**
 * Writes the text in winText at (cx,cy).
 */
static void writeText(const char *line, int len)
{  
    wmove(mainWindow.console.winText, mainWindow.console.cy, mainWindow.console.cx);
    waddnstr(mainWindow.console.winText, line, len);
    wclrtoeol(mainWindow.console.winText);
}

static int getScreenSize(int axis)
{
    int                 x, y;

    getmaxyx(mainWindow.console.winText, y, x);
    return axis == VX ? x : y;
}

void Sys_ConPrint(uint idx, const char *text, int clflags)
{
    ddwindow_t         *win;
    char                line[LINELEN];
    int                 count = strlen(text), lineStart, bPos;
    const char         *ptr = text;
    char                ch;
    int                 maxPos[2];

    if(!winManagerInited || !text)
        return;

    if(!novideo && idx != 1)
    {
        // We only support one terminal window (this isn't for us).
        return;
    }

    win = &mainWindow;

    // Determine the size of the text window.
    getmaxyx(win->console.winText, maxPos[VY], maxPos[VX]);

    if(win->console.needNewLine)
    {
        // Need to make some room.
        win->console.cx = 0;
        win->console.cy++;
        while(win->console.cy >= maxPos[VY])
        {
            win->console.cy--;
            scroll(win->console.winText);
        }
        win->console.needNewLine = false;
    }
    bPos = lineStart = win->console.cx;

    setAttrib(clflags);

    for(; count > 0; count--, ptr++)
    {
        ch = *ptr;
        // Ignore carriage returns.
        if(ch == '\r')
            continue;
        if(ch != '\n' && bPos < maxPos[VX])
        {
            line[bPos] = ch;
            bPos++;
        }
        // Time for newline?
        if(ch == '\n' || bPos >= maxPos[VX])
        {
            writeText(line + lineStart, bPos - lineStart);
            win->console.cx += bPos - lineStart;
            bPos = 0;
            lineStart = 0;
            if(count > 1)       // Not the last character?
            {
                win->console.needNewLine = false;
                win->console.cx = 0;
                win->console.cy++;
                while(win->console.cy >= maxPos[VY])
                {
                    scroll(win->console.winText);
                    win->console.cy--;
                }
            }
            else
                win->console.needNewLine = true;
        }
    }

    // Something in the buffer?
    if(bPos - lineStart)
    {
        writeText(line + lineStart, bPos - lineStart);
        win->console.cx += bPos - lineStart;
    }

    wrefresh(win->console.winText);

    // Move the cursor back onto the command line.
    setConWindowCmdLine(1, NULL, 0, 0);
}

void Sys_SetConWindowCmdLine(uint idx, const char* text, uint cursorPos, int flags)
{
    ddwindow_t* win;

    if(!winManagerInited)
        return;

    win = getWindow(idx - 1);

    if(!win || win->type != WT_CONSOLE)
        return;

    setConWindowCmdLine(idx, text, cursorPos, flags);
}

static void setConWindowCmdLine(uint idx, const char *text,
                                unsigned int cursorPos, int flags)
{
    ddwindow_t  *win;
    unsigned int i;
    char        line[LINELEN], *ch;
    int         maxX;
    int         length;

    if(idx != 1)
    {
        // We only support one console window; (this isn't for us).
        return;
    }
    win = &mainWindow;

    maxX = getScreenSize(VX);

    if(!text)
    {
        int         y, x;

        // Just move the cursor into the command line window.
        getyx(win->console.winCommand, y, x);
        wmove(win->console.winCommand, y, x);
    }
    else
    {
        memset(line, 0, sizeof(line));
        line[0] = '>';
        for(i = 0, ch = line + 1; i < LINELEN - 1; ++i, ch++)
        {
            if(i < strlen(text))
                *ch = text[i];
            else
                *ch = 0;
        }
        wmove(win->console.winCommand, 0, 0);

        // Can't print longer than the window.
        length = strlen(text);
        waddnstr(win->console.winCommand, line, MIN_OF(maxX, length + 1));
        wclrtoeol(win->console.winCommand);
    }
    wrefresh(win->console.winCommand);
}
#endif

boolean Sys_ChangeVideoMode(int width, int height, int bpp)
{
    int         flags = SDL_OPENGL;
    const SDL_VideoInfo *info = NULL;
    int         windowflags;

    windowflags = (theWindow->flags);
    if(windowflags & DDWF_FULLSCREEN)
        flags |= SDL_FULLSCREEN;

    if(!SDL_SetVideoMode(width, height, bpp, flags))
    {
        // This could happen for a variety of reasons, including
        // DISPLAY not being set, the specified resolution not being
        // available, etc.
        Con_Message("SDL Error: %s\n", SDL_GetError());
        return false;
    }

    Con_Message("createContext: OpenGL.\n");

    info = SDL_GetVideoInfo();
    screenWidth = info->current_w;
    screenHeight = info->current_h;
    screenBPP = info->vfmt->BitsPerPixel;

    return true;
}

/**
 * Initialize the window manager.
 * Tasks include; checking the system environment for feature enumeration.
 *
 * @return              @c true, if initialization was successful.
 */
boolean Sys_InitWindowManager(void)
{
    if(winManagerInited)
        return true; // Already been here.

    Con_Message("Sys_InitWindowManager: Using SDL window management.\n");

    // Initialize the SDL video subsystem, unless we're going to run in
    // dedicated mode.
    if(!ArgExists("-dedicated"))
    {
/**
 * @attention Solaris has no Joystick support according to
 * https://sourceforge.net/tracker/?func=detail&atid=542099&aid=1732554&group_id=74815
 */
#ifdef SOLARIS
        if(SDL_InitSubSystem(SDL_INIT_VIDEO))
#else
        if(SDL_InitSubSystem(SDL_INIT_VIDEO | (!ArgExists("-nojoy")?SDL_INIT_JOYSTICK : 0)))
#endif
        {
            Con_Message("SDL Init Failed: %s\n", SDL_GetError());
            return false;
        }
    }

    memset(&mainWindow, 0, sizeof(mainWindow));
    winManagerInited = true;
    theWindow = &mainWindow;
    return true;
}

/**
 * Shutdown the window manager.
 *
 * @return              @c true, if shutdown was successful.
 */
boolean Sys_ShutdownWindowManager(void)
{
    if(!winManagerInited)
        return false; // Window manager is not initialized.

    if(mainWindow.type == WT_CONSOLE)
        Sys_DestroyWindow(1);

    // Now off-line, no more window management will be possible.
    winManagerInited = false;

    //if(isDedicated)
    //    Sys_ConShutdown();

    return true;
}

static boolean initOpenGL(void)
{
    // Attempt to set the video mode.
    if(!Sys_ChangeVideoMode(theWindow->geometry.size.width,
            theWindow->geometry.size.height, theWindow->normal.bpp))
        return false;

    Sys_GLConfigureDefaultState();
    return true;
}

/**
 * Attempt to acquire a device context for OGL rendering and then init.
 *
 * @param width         Width of the OGL window.
 * @param height        Height of the OGL window.
 * @param bpp           0= the current display color depth is used.
 * @param windowed      @c true = windowed mode ELSE fullscreen.
 * @param data          Ptr to system-specific data, e.g a window handle
 *                      or similar.
 *
 * @return              @c true if successful.
 */
static boolean createContext(int width, int height, int bpp,
                             boolean windowed, void *data)
{
    Con_Message("createContext: OpenGL.\n");

    // Set GL attributes.  We want at least 5 bits per color and a 16
    // bit depth buffer.  Plus double buffering, of course.
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    if(!initOpenGL())
    {
        Con_Error("createContext: OpenGL init failed.\n");
    }

    return true;
}

/**
 * Complete the given wminfo_t, detailing what features are supported by
 * this window manager implementation.
 *
 * @param info          Ptr to the wminfo_t structure to complete.
 *
 * @return              @c true, if successful.
 */
boolean Sys_GetWindowManagerInfo(wminfo_t *info)
{
    if(!winManagerInited)
        return false; // Window manager is not initialized.

    if(!info)
        return false; // Wha?

    // Complete the structure detailing what features are available.
    info->canMoveWindow = false;
    info->maxWindows = 1;
    info->maxConsoles = 1;

    return true;
}

static ddwindow_t* createDDWindow(application_t* app, const Size2Rawi* size,
    int bpp, int flags, ddwindowtype_t type, const char* title)
{
    // SDL only supports one window.
    if(mainWindowInited) return NULL;

    if(type == WT_CONSOLE)
    {
#if defined(UNIX)
        int         maxPos[2];

        // Initialize curses.
        if(!(cursesRootWin = initscr()))
        {
            Sys_CriticalMessage("createDDWindow: Failed creating terminal.");
            return NULL;
        }

        cbreak();
        noecho();
        nonl();

        mainWindow.type = type;

        // The current size of the screen.
        getmaxyx(stdscr, maxPos[VY], maxPos[VX]);

        // Create the three windows we will be using.
        mainWindow.console.winTitle = newwin(1, maxPos[VX], 0, 0);
        mainWindow.console.winText = newwin(maxPos[VY] - 2, maxPos[VX], 1, 0);
        mainWindow.console.winCommand = newwin(1, maxPos[VX], maxPos[VY] - 1, 0);

        // Set attributes.
        wattrset(mainWindow.console.winTitle, A_REVERSE);
        wattrset(mainWindow.console.winText, A_NORMAL);
        wattrset(mainWindow.console.winCommand, A_BOLD);

        scrollok(mainWindow.console.winText, TRUE);
        wclear(mainWindow.console.winText);
        wrefresh(mainWindow.console.winText);

        keypad(mainWindow.console.winCommand, TRUE);
        nodelay(mainWindow.console.winCommand, TRUE);
        setConWindowCmdLine(1, "", 1, 0);

        // The background will also be in reverse.
        wbkgdset(mainWindow.console.winTitle, ' ' | A_REVERSE);

        // First clear the whole line.
        wmove(mainWindow.console.winTitle, 0, 0);
        wclrtoeol(mainWindow.console.winTitle);

        // Center the title.
        wmove(mainWindow.console.winTitle, 0, getmaxx(mainWindow.console.winTitle) / 2 - strlen(title) / 2);
        waddstr(mainWindow.console.winTitle, title);
        wrefresh(mainWindow.console.winTitle);

        // We'll need the input event handler.
        Sys_ConInputInit();
#endif
    }
    else
    {
        if(!(bpp == 32 || bpp == 16))
        {
            Con_Message("createWindow: Unsupported BPP %i.", bpp);
            return 0;
        }

#if defined(WIN32)
        // We need to grab a handle from SDL so we can link other subsystems
        // (e.g. DX-based input).
        {
        struct SDL_SysWMinfo wmInfo;
        if(!SDL_GetWMInfo(&wmInfo))
            return NULL;

        mainWindow.hWnd = wmInfo.window;
        }
#endif
    }

    setDDWindow(&mainWindow, size->width, size->height, bpp, flags,
                DDSW_NOVISIBLE | DDSW_NOCENTER | DDSW_NOFULLSCREEN);

    mainWindowInited = true;
    return &mainWindow;
}

uint Sys_CreateWindow(application_t* app, uint parentIdx, const Point2Rawi* origin,
    const Size2Rawi* size, int bpp, int flags, ddwindowtype_t type, const char* title,
    void* userData)
{
    ddwindow_t* win;
    if(!winManagerInited) return 0;

    win = createDDWindow(app, size, bpp, flags, type, title);
    if(win) return 1; // Success.
    return 0;
}

/**
 * Destroy the specified window.
 *
 * Side-effects: If the window is fullscreen and the current video mode is
 * not that set as the desktop default: an attempt will be made to change
 * back to the desktop default video mode.
 *
 * @param idx           Index of the window to destroy (1-based).
 *
 * @return              @c true, if successful.
 */
boolean Sys_DestroyWindow(uint idx)
{
    ddwindow_t         *window = getWindow(idx - 1);

    if(!window)
        return false;

    if(window->type == WT_CONSOLE)
    {
        // Delete windows and shut down curses.
        delwin(window->console.winTitle);
        delwin(window->console.winText);
        delwin(window->console.winCommand);

        window->console.winTitle = window->console.winText =
            window->console.winCommand = NULL;

        delwin(cursesRootWin);
        cursesRootWin = 0;

        endwin();
        refresh();

        Sys_ConInputShutdown();
    }

    return true;
}

/**
 * Change the currently active window.
 *
 * @param idx           Index of the window to make active (1-based).
 *
 * @return              @c true, if successful.
 */
boolean Sys_SetActiveWindow(uint idx)
{
    // We only support one window, so yes its active.
    return true;
}

static boolean setDDWindow(ddwindow_t *window, int newWidth, int newHeight,
                           int newBPP, uint wFlags, uint uFlags)
{
    int             width, height, bpp, flags;
    boolean         newGLContext = false;
    boolean         changeWindowDimensions = false;
    boolean         inControlPanel = false;

    if(novideo)
        return true;

    if(uFlags & DDSW_NOCHANGES)
        return true; // Nothing to do.

    // Grab the current values.
    width = window->geometry.size.width;
    height = window->geometry.size.height;
    bpp = window->normal.bpp;
    flags = window->flags;
    // Force update on init?
    if(!window->inited && window->type == WT_NORMAL)
    {
        newGLContext = true;
    }

    if(window->type == WT_NORMAL)
        inControlPanel = UI_IsActive();

    // Change to fullscreen?
    if(!(uFlags & DDSW_NOFULLSCREEN) &&
       (flags & DDWF_FULLSCREEN) != (wFlags & DDWF_FULLSCREEN))
    {
        flags ^= DDWF_FULLSCREEN;

        if(window->type == WT_NORMAL)
        {
            newGLContext = true;
            //changeVideoMode = true;
        }
    }

    // Change window size?
    if(!(uFlags & DDSW_NOSIZE) && (width != newWidth || height != newHeight))
    {
        width = newWidth;
        height = newHeight;
        changeWindowDimensions = true;

        if(window->type == WT_NORMAL)
            newGLContext = true;
    }

    // Change BPP (bits per pixel)?
    if(window->type == WT_NORMAL)
    {
        if(!(uFlags & DDSW_NOBPP) && bpp != newBPP)
        {
            if(!(newBPP == 32 || newBPP == 16))
                Con_Error("Sys_SetWindow: Unsupported BPP %i.", newBPP);

            bpp = newBPP;

            newGLContext = true;
            //changeVideoMode = true;
        }
    }

    if(changeWindowDimensions && window->type == WT_NORMAL)
    {
        // Can't change the resolution while the UI is active.
        // (controls need to be repositioned).
        if(inControlPanel)
            UI_End();
    }
/*
    if(changeVideoMode)
    {
        if(flags & DDWF_FULLSCREEN)
        {
            if(!Sys_ChangeVideoMode(width, height, bpp))
            {
                Sys_CriticalMessage("Sys_SetWindow: Resolution change failed.");
                return false;
            }
        }
        else
        {
            // Go back to normal display settings.
            ChangeDisplaySettings(0, 0);
        }
    }
*/
    // Update the current values.
    window->geometry.size.width = width;
    window->geometry.size.height = height;
    window->normal.bpp = bpp;
    window->flags = flags;
    if(!window->inited)
        window->inited = true;

    // Do NOT modify ddwindow_t properties after this point.

    // Do we need a new GL context due to changes to the window?
    if(newGLContext)
    {   // Maybe requires a renderer restart.
extern boolean usingFog;

        boolean         glIsInited = GL_IsInited();
#if defined(WIN32)
        void           *data = window->hWnd;
#else
        void           *data = NULL;
#endif
        boolean         hadFog = false;

        if(glIsInited)
        {
            // Shut everything down, but remember our settings.
            hadFog = usingFog;
            GL_TotalReset();

            if(!DD_IsNullGameInfo(DD_GameInfo()) && gx.UpdateState)
                gx.UpdateState(DD_RENDER_RESTART_PRE);

            R_UnloadVectorGraphics();
            GL_ReleaseTextures();
        }

        if(createContext(window->geometry.size.width, window->geometry.size.height,
               window->normal.bpp, (window->flags & DDWF_FULLSCREEN)? false : true, data))
        {
            // We can get on with initializing the OGL state.
            Sys_GLConfigureDefaultState();
        }

        if(glIsInited)
        {
            // Re-initialize.
            GL_TotalRestore();
            GL_InitRefresh();

            if(hadFog)
                GL_UseFog(true);

            if(!DD_IsNullGameInfo(DD_GameInfo()) && gx.UpdateState)
                gx.UpdateState(DD_RENDER_RESTART_POST);
        }
    }
    /*else
    {
        Sys_ChangeVideoMode(window->geometry.size.width,
            window->geometry.size.height, window->normal.bpp);
    }*/

    // If the window dimensions have changed, update any sub-systems
    // which need to respond.
    if(changeWindowDimensions && window->type == WT_NORMAL)
    {
        // Update viewport coordinates.
        R_SetViewGrid(0, 0);

        if(inControlPanel) // Reactivate the panel?
            Con_Execute(CMDS_DDAY, "panel", true, false);
    }

    return true;
}

/**
 * Attempt to set the appearance/behavioral properties of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param newX          New x position of the left side of the window.
 * @param newY          New y position of the top of the window.
 * @param newWidth      New width to set the window to.
 * @param newHeight     New height to set the window to.
 * @param newBPP        New BPP (bits-per-pixel) to change to.
 * @param wFlags        DDWF_* flags to change other appearance/behavior.
 * @param uFlags        DDSW_* flags govern how the other paramaters should be
 *                      interpreted.
 *
 *                      DDSW_NOSIZE:
 *                      If set, params 'newWidth' and 'newHeight' are ignored
 *                      and no change will be made to the window dimensions.
 *
 *                      DDSW_NOMOVE:
 *                      If set, params 'newX' and 'newY' are ignored and no
 *                      change will be made to the window position.
 *
 *                      DDSW_NOBPP:
 *                      If set, param 'newBPP' is ignored and the no change
 *                      will be made to the window color depth.
 *
 *                      DDSW_NOFULLSCREEN:
 *                      If set, the value of the DDWF_FULLSCREEN bit in param
 *                      'wFlags' is ignored and no change will be made to the
 *                      fullscreen state of the window.
 *
 *                      DDSW_NOVISIBLE:
 *                      If set, the value of the DDWF_VISIBLE bit in param
 *                      'wFlags' is ignored and no change will be made to the
 *                      window's visibility.
 *
 *                      DDSW_NOCENTER:
 *                      If set, the value of the DDWF_CENTER bit in param
 *                      'wFlags' is ignored and no change will be made to the
 *                      auto-center state of the window.
 *
 * @return              @c true, if successful.
 */
boolean Sys_SetWindow(uint idx, int newX, int newY, int newWidth, int newHeight,
                      int newBPP, uint wFlags, uint uFlags)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(window)
        return setDDWindow(window, newWidth, newHeight, newBPP,
                           wFlags, uFlags);

    return false;
}

/**
 * Make the content of the framebuffer visible.
 */
void Sys_UpdateWindow(uint idx)
{
    if(GL_state.forceFinishBeforeSwap)
    {
        glFinish();
    }

    // Swap buffers.
    SDL_GL_SwapBuffers(); // Includes a call to glFlush()
}

/**
 * Attempt to set the title of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param title         New title for the window.
 *
 * @return              @c true, if successful.
 */
boolean Sys_SetWindowTitle(uint idx, const char *title)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(window)
    {
        if(window->type == WT_NORMAL)
        {
            SDL_WM_SetCaption(title, NULL);
        }
        else // Its a terminal window.
        {
#if defined(UNIX)
            // The background will also be in reverse.
            wbkgdset(window->console.winTitle, ' ' | A_REVERSE);

            // First clear the whole line.
            wmove(window->console.winTitle, 0, 0);
            wclrtoeol(window->console.winTitle);

            // Center the title.
            wmove(window->console.winTitle, 0, getmaxx(window->console.winTitle) / 2 - strlen(title) / 2);
            waddstr(window->console.winTitle, title);
            wrefresh(window->console.winTitle);
#endif
        }

        return true;
    }

    return false;
}

const RectRawi* Sys_GetWindowGeometry(uint idx)
{
    ddwindow_t* window = getWindow(idx - 1);
    if(!window) return NULL;
    // Moving does not work in dedicated mode.
    if(isDedicated) return NULL;
    return &window->geometry;
}

const Point2Rawi* Sys_GetWindowOrigin(uint idx)
{
    ddwindow_t* window = getWindow(idx - 1);
    if(!window) return NULL;
    // Moving does not work in dedicated mode.
    if(isDedicated) return NULL;
    return &window->geometry.origin;
}

const Size2Rawi* Sys_GetWindowSize(uint idx)
{
    ddwindow_t* window = getWindow(idx - 1);
    if(!window) return NULL;
    // Moving does not work in dedicated mode.
    if(isDedicated) return NULL;
    return &window->geometry.size;
}

/**
 * Attempt to get the BPP (bits-per-pixel) of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param bpp           Address to write the BPP back to (if any).
 *
 * @return              @c true, if successful.
 */
boolean Sys_GetWindowBPP(uint idx, int *bpp)
{
    ddwindow_t* window = getWindow(idx - 1);

    if(!window || !bpp)
        return false;

    // Not in dedicated mode.
    if(isDedicated)
        return false;

    *bpp = window->normal.bpp;

    return true;
}

/**
 * Attempt to get the fullscreen-state of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param fullscreen    Address to write the fullscreen state back to (if any).
 *
 * @return              @c true, if successful.
 */
boolean Sys_GetWindowFullscreen(uint idx, boolean *fullscreen)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window || !fullscreen)
        return false;

    *fullscreen = ((window->flags & DDWF_FULLSCREEN)? true : false);

    return true;
}

/**
 * Attempt to get a HWND handle to the given window.
 *
 * \todo: Factor platform specific design patterns out of Doomsday. We should
 * not be passing around HWND handles...
 *
 * @param idx           Index identifier (1-based) to the window.
 *
 * @return              HWND handle if successful, ELSE @c NULL,.
 */
#if defined(WIN32)
HWND Sys_GetWindowHandle(uint idx)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window)
        return NULL;

    return window->hWnd;
}
#endif
