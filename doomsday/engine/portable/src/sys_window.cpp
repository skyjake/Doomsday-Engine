/** @file sys_window.cpp
 * Qt-based window management implementation. @ingroup base
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2008 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <QWidget>
#include <QApplication>
#include <QDesktopWidget>
#include <QSettings>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef UNIX
#  include "m_args.h"
#  include <SDL.h>
#endif

#include "canvaswindow.h"

#include "de_platform.h"
#include "sys_window.h"
#include "sys_system.h"
#include "dd_main.h"
#include "con_main.h"
#include "gl_main.h"
#include "ui_main.h"

struct ddwindow_s
{
    CanvasWindow* widget; ///< The widget this window represents.
    void (*drawFunc)(void); ///< Draws the contents of the canvas.
    QRect appliedGeometry; ///< Saved for detecting when changes have occurred.

    ddwindowtype_t type;
    boolean inited;
    RectRaw geometry;
    int bpp;
    int flags;
    consolewindow_t console; ///< Only used for WT_CONSOLE windows.

#if defined(WIN32)
    HWND hWnd; ///< Needed for input (among other things).
    HGLRC glContext;
#endif

    void __inline assertWindow() const
    {
        assert(this);
        assert(widget);
    }

    int x() const
    {
        return geometry.origin.x;
    }

    int y() const
    {
        return geometry.origin.y;
    }

    int width() const
    {
        return geometry.size.width;
    }

    int height() const
    {
        return geometry.size.height;
    }

    /**
     * Checks all command line options that affect window geometry and applies
     * them to this Window.
     */
    void modifyAccordingToOptions()
    {
        if(ArgCheckWith("-width", 1))
        {
            geometry.size.width = atoi(ArgNext());
        }

        if(ArgCheckWith("-height", 1))
        {
            geometry.size.height = atoi(ArgNext());
        }

        if(ArgCheckWith("-winsize", 2))
        {
            geometry.size.width = atoi(ArgNext());
            geometry.size.height = atoi(ArgNext());
        }

        if(ArgCheckWith("-bpp", 1))
        {
            bpp = atoi(ArgNext());
        }

        bool noCenter = false;
        if(ArgCheck("-nocenter"))
        {
            noCenter = true;
        }

        if(ArgCheckWith("-xpos", 1))
        {
            geometry.origin.x = atoi(ArgNext());
            noCenter = true;
        }

        if(ArgCheckWith("-ypos", 1))
        {
            geometry.origin.y = atoi(ArgNext());
            noCenter = true;
        }

        if(noCenter)
        {
            flags &= ~DDWF_CENTER;
        }

        if(ArgCheck("-center"))
        {
            flags |= DDWF_CENTER;
        }

        if(ArgExists("-nofullscreen") || ArgExists("-window"))
        {
            flags &= ~DDWF_FULLSCREEN;
        }

        if(ArgExists("-fullscreen") || ArgExists("-nowindow"))
        {
            flags |= DDWF_FULLSCREEN;
        }
    }

    /**
     * Applies the information stored in the Window instance to the actual
     * widget geometry. Centering is applied in this stage (it only affects the
     * widget's geometry).
     */
    void applyWindowGeometry()
    {
        assertWindow();

        QRect geom(x(), y(), width(), height());

        if(flags & DDWF_CENTER)
        {
            // Center the window.
            QSize screenSize = QApplication::desktop()->screenGeometry().size();
            geom = QRect((screenSize.width() - width())/2,
                         (screenSize.height() - height())/2,
                         width(), height());
        }

        if(flags & DDWF_FULLSCREEN)
        {
            /// @todo fullscreen mode
        }

        appliedGeometry = geom; // Saved for detecting changes.
        widget->setGeometry(geom);
    }

    /**
     * Retrieves the actual widget geometry and updates the information stored
     * in the Window instance.
     */
    void fetchWindowGeometry()
    {
        assertWindow();

        QRect rect = widget->geometry();
        geometry.origin.x = rect.x();
        geometry.origin.y = rect.y();
        geometry.size.width = rect.width();
        geometry.size.height = rect.height();

        if(rect != appliedGeometry)
        {
            // The user has moved or resized the window.
            // Let's not recenter it any more.
            flags &= ~DDWF_CENTER;
        }
    }
};

/// Currently active window where all drawing operations are directed at.
const Window* theWindow;

static boolean winManagerInited = false;

static Window mainWindow;
static boolean mainWindowInited = false;

static int screenWidth, screenHeight, screenBPP;
static boolean screenIsWindow;

Window* Window_Main(void)
{
    return &mainWindow;
}

static __inline Window *getWindow(uint idx)
{
    if(!winManagerInited)
        return NULL; // Window manager is not initialized.

    if(idx == 1)
        return &mainWindow;

    assert(false); // We can only have window 1 (main window).
    return NULL;
}

Window* Window_ByIndex(uint id)
{
    return getWindow(id);
}

boolean Sys_ChangeVideoMode(int width, int height, int bpp)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();

    // Do we need to change it?
    if(width == screenWidth && height == screenHeight && bpp == screenBPP &&
       screenIsWindow == !(theWindow->flags & DDWF_FULLSCREEN))
    {
        // Got it already.
        DEBUG_Message(("Sys_ChangeVideoMode: Ignoring because already using %ix%i bpp:%i window:%i\n",
                       width, height, bpp, screenIsWindow));
        return true;
    }

    DEBUG_Message(("Sys_ChangeVideoMode: Setting %ix%i bpp:%i window:%i\n",
                   width, height, bpp, screenIsWindow));

    // TODO: Attempt to change mode.


    // Update current mode.
    screenWidth = width; //info->current_w;
    screenHeight = height; //info->current_h;
    screenBPP = bpp; //info->vfmt->BitsPerPixel;
    screenIsWindow = (theWindow->flags & DDWF_FULLSCREEN? false : true);
    return true;

#if 0
    int flags = SDL_OPENGL;
    const SDL_VideoInfo *info = NULL;

    LIBDENG_ASSERT_IN_MAIN_THREAD();

    // Do we need to change it?
    if(width == screenWidth && height == screenHeight && bpp == screenBPP &&
       screenIsWindow == !(theWindow->flags & DDWF_FULLSCREEN))
    {
        // Got it already.
        DEBUG_Message(("Sys_ChangeVideoMode: Ignoring because already using %ix%i bpp:%i window:%i\n",
                       width, height, bpp, screenIsWindow));
        return true;
    }

    if(theWindow->flags & DDWF_FULLSCREEN)
        flags |= SDL_FULLSCREEN;

    DEBUG_Message(("Sys_ChangeVideoMode: Setting %ix%i bpp:%i window:%i\n",
                   width, height, bpp, screenIsWindow));

    if(!SDL_SetVideoMode(width, height, bpp, flags))
    {
        // This could happen for a variety of reasons, including
        // DISPLAY not being set, the specified resolution not being
        // available, etc.
        Con_Message("SDL Error: %s\n", SDL_GetError());
        return false;
    }

    info = SDL_GetVideoInfo();
    screenWidth = info->current_w;
    screenHeight = info->current_h;
    screenBPP = info->vfmt->BitsPerPixel;
    screenIsWindow = (theWindow->flags & DDWF_FULLSCREEN? false : true);

    return true;
#endif
}

#if 0
static boolean setDDWindow(Window *window, int newWidth, int newHeight,
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
    bpp = window->bpp;
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
    window->bpp = bpp;
    window->flags = flags;
    if(!window->inited)
        window->inited = true;

    // Do NOT modify Window properties after this point.

    // Do we need a new GL context due to changes to the window?
    if(newGLContext)
    {
#if 0
        // Maybe requires a renderer restart.
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

            if(DD_GameLoaded() && gx.UpdateState)
                gx.UpdateState(DD_RENDER_RESTART_PRE);

            R_UnloadSvgs();
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

            if(DD_GameLoaded() && gx.UpdateState)
                gx.UpdateState(DD_RENDER_RESTART_POST);
        }
#endif
    }

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
#endif

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

    Con_Message("Sys_InitWindowManager: Using Qt window management.\n");

    CanvasWindow::setDefaultGLFormat();

#ifdef UNIX
    // Initialize the SDL video subsystem, unless we're going to run in
    // dedicated mode.
    if(!ArgExists("-dedicated"))
    {
        SDL_InitSubSystem(SDL_INIT_VIDEO);
    }
#endif

#if 0
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
#endif

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

    // Get rid of the windows.
    Window_Delete(Window_Main());

    // Now off-line, no more window management will be possible.
    winManagerInited = false;

    return true;
}

#if 0
/**
 * Attempt to acquire a device context for OpenGL rendering and then init.
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
static boolean createContext(void)
{
    Con_Message("createContext: OpenGL.\n");

#if 0
    // Set GL attributes.  We want at least 5 bits per color and a 16
    // bit depth buffer.  Plus double buffering, of course.
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#endif

    /*
    // Attempt to set the video mode.
    if(!Sys_ChangeVideoMode(Window_Width(theWindow),
                            Window_Height(theWindow),
                            Window_BitsPerPixel(theWindow)))
    {
        Con_Error("createContext: Video mode change failed.\n");
        return false;
    }
    */

    Sys_GLConfigureDefaultState();

#ifdef MACOSX
    // Vertical sync is a GL context property.
    GL_SetVSync(true);
#endif

    return true;
}
#endif

static void drawCanvasWithCallback(Canvas& canvas)
{
    // Look up the correct window.
    assert(&canvas == &mainWindow.widget->canvas());

    Window* win = &mainWindow;
    if(win->drawFunc)
    {
        win->drawFunc();
    }
}

static void finishMainWindowInit(Canvas& canvas)
{
    assert(&mainWindow.widget->canvas() == &canvas);
    DD_FinishInitializationAfterWindowReady();
}

static Window* createWindow(ddwindowtype_t type, const char* title)
{
    if(mainWindowInited) return NULL; /// @todo  Allow multiple.

    Window* wnd = &mainWindow;
    memset(wnd, 0, sizeof(*wnd));
    mainWindowIdx = 1;

    if(type == WT_CONSOLE)
    {
        mainWindow.type = WT_CONSOLE;
        Sys_ConInit(title);
    }
    else
    {
        // Create the main window (hidden).
        mainWindow.widget = new CanvasWindow;
        Window_SetTitle(&mainWindow, title);
        Window_RestoreState(&mainWindow);
        mainWindow.widget->setMinimumSize(QSize(320, 240)); // Minimum possible size when resizing.

        // After the main window is created, we can finish with the engine init.
        mainWindow.widget->canvas().setInitCallback(finishMainWindowInit);

        // Let's see if there are command line options overriding the previous state.
        mainWindow.modifyAccordingToOptions();

        // Make it so. (Not shown yet.)
        mainWindow.applyWindowGeometry();

        mainWindow.inited = true;

#if 0
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
#endif
    }

    /// @todo Refactor for multiwindow support.
    mainWindowInited = true;
    return &mainWindow;
}

Window* Window_New(ddwindowtype_t type, const char* title)
{
    if(!winManagerInited) return 0;
    return createWindow(type, title);
}

void Window_Delete(Window* wnd)
{
    assert(wnd);

    if(wnd->type == WT_CONSOLE)
    {
        Sys_ConShutdown(wnd);
    }
    else
    {
        assert(wnd->widget);

        // Make sure we'll remember the config.
        Window_SaveState(wnd);

        // Delete the CanvasWindow.
        delete wnd->widget;

        memset(wnd, 0, sizeof(*wnd));
    }
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
    Window *window = getWindow(idx);

    if(window)
    {
        // TODO: Update if necessary.
        return true;
    }
    /*
    if(window)
        return setDDWindow(window, newWidth, newHeight, newBPP,
                           wFlags, uFlags);
    */
    return false;
}

/**
 * Make the content of the framebuffer visible.
 */
void Window_SwapBuffers(const Window* win)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();

    assert(win);
    if(!win->widget) return;

    // Force a swapbuffers right now.
    win->widget->canvas().swapBuffers();
}

/**
 * Attempt to set the title of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param title         New title for the window.
 *
 * @return              @c true, if successful.
 */
void Window_SetTitle(const Window* win, const char *title)
{
    assert(win);

    LIBDENG_ASSERT_IN_MAIN_THREAD();

    switch(win->type)
    {
    case WT_NORMAL:
        assert(win->widget);
        win->widget->setWindowTitle(QString::fromLatin1(title));
        break;

    case WT_CONSOLE:
        ConsoleWindow_SetTitle(win, title);
        break;

    default:
        break;
    }
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
    Window *window = getWindow(idx);

    if(!window || !fullscreen)
        return false;

    *fullscreen = ((window->flags & DDWF_FULLSCREEN)? true : false);

    return true;
}

#if 0

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
    Window *window = getWindow(idx);

    if(!window)
        return NULL;

    return window->hWnd;
}
#endif

#endif

void Window_SetDrawFunction(Window* win, void (*drawFunc)(void))
{
    if(win->type == WT_CONSOLE) return;

    assert(win);
    assert(win->widget);

    win->drawFunc = drawFunc;
    win->widget->canvas().setDrawCallback(drawFunc? drawCanvasWithCallback : 0);
}

void Window_Draw(Window* win)
{
    if(win->type == WT_CONSOLE) return;

    assert(win);
    assert(win->widget);

    // Repaint right now.
    win->widget->canvas().forcePaint();
}

void Window_Show(Window *wnd, boolean show)
{
    /// Assumption: This is only called once, during startup.

    assert(wnd);

    if(wnd->type == WT_CONSOLE)
    {
        // Not really applicable.
        if(show)
        {
            /// @todo  Kludge: finish init in dedicated mode.
            DD_FinishInitializationAfterWindowReady();
            return;
        }
    }

    assert(wnd->widget);
    if(show)
    {
        wnd->widget->show();
    }
    else
    {
        wnd->widget->hide();
    }
}

ddwindowtype_t Window_Type(const Window* wnd)
{
    assert(wnd);
    return wnd->type;
}

struct consolewindow_s* Window_Console(Window* wnd)
{
    assert(wnd);
    return &wnd->console;
}

const struct consolewindow_s* Window_ConsoleConst(const Window* wnd)
{
    assert(wnd);
    return &wnd->console;
}

int Window_X(const Window* wnd)
{
    assert(wnd);
    return wnd->x();
}

int Window_Y(const Window* wnd)
{
    assert(wnd);
    return wnd->y();
}

int Window_Width(const Window* wnd)
{
    assert(wnd);
    return wnd->width();
}

int Window_Height(const Window *wnd)
{
    assert(wnd);
    return wnd->height();
}

int Window_BitsPerPixel(const Window* wnd)
{
    assert(wnd);
    return wnd->bpp;
}

const Size2Raw* Window_Size(const Window* wnd)
{
    assert(wnd);
    return &wnd->geometry.size;
}

static QString windowSettingsKey(uint idx, const char* name)
{
    return QString("window/%1/").arg(idx) + name;
}

void Window_SaveState(Window* wnd)
{
    assert(wnd);

    // Console windows are not saved.
    if(wnd->type == WT_CONSOLE) return;

    assert(wnd == &mainWindow); /// @todo  Figure out the window index if there are many.
    uint idx = mainWindowIdx;
    assert(idx == 1);

    wnd->fetchWindowGeometry();

    QSettings st;
    st.setValue(windowSettingsKey(idx, "rect"), QRect(Window_X(wnd), Window_Y(wnd), Window_Width(wnd), Window_Height(wnd)));
    st.setValue(windowSettingsKey(idx, "center"), (wnd->flags & DDWF_CENTER) != 0);
    st.setValue(windowSettingsKey(idx, "fullscreen"), (wnd->flags & DDWF_FULLSCREEN) != 0);
    st.setValue(windowSettingsKey(idx, "bpp"), Window_BitsPerPixel(wnd));
}

void Window_RestoreState(Window* wnd)
{
    assert(wnd);

    // Console windows can not be restored.
    if(wnd->type == WT_CONSOLE) return;

    assert(wnd == &mainWindow);  /// @todo  Figure out the window index if there are many.
    uint idx = mainWindowIdx;
    assert(idx == 1);

    // The default state of the window is determined by these values.
    QSettings st;
    QRect geom = st.value(windowSettingsKey(idx, "rect"), QRect(0, 0, 640, 480)).toRect();
    wnd->geometry.origin.x = geom.x();
    wnd->geometry.origin.y = geom.y();
    wnd->geometry.size.width = geom.width();
    wnd->geometry.size.height = geom.height();
    wnd->bpp = st.value(windowSettingsKey(idx, "bpp"), 32).toInt();

    if(st.value(windowSettingsKey(idx, "center"), true).toBool())
        wnd->flags |= DDWF_CENTER;
    else
        wnd->flags &= ~DDWF_CENTER;

    if(st.value(windowSettingsKey(idx, "fullscreen"), true).toBool())
        wnd->flags |= DDWF_FULLSCREEN;
    else
        wnd->flags &= ~DDWF_FULLSCREEN;
}
