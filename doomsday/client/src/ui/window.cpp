/** @file window.cpp Window manager. Window manager that manages a QWidget-based window. 
 * @ingroup base
 *
 * The Doomsday window management is responsible for the positioning, sizing,
 * and state of the game's native windows. In practice, the code operates on Qt
 * top-level windows.
 *
 * At the moment, the quality of the code is adequate at best. See the todo
 * notes below for ideas for future improvements.
 *
 * @todo Instead of 'rect' and 'normalRect', the window should have a
 * 'fullscreenSize' and a 'normalRect'. It isn't ideal that when toggling
 * between fullscreen and windowed mode, the fullscreen resolution is chosen
 * based on the size of the normal-mode window.
 *
 * @todo It is not a good idea to duplicate window state locally (position,
 * size, flags). Much of the complexity here is due to this duplication, trying
 * to keep all the state consistent. Instead, the real QWidget should be used
 * for these properties. Qt has a mechanism for storing the state of a window:
 * QWidget::saveGeometry(), QMainWindow::saveState().
 *
 * @todo Refactor for multiple window support. One window should be the "main"
 * window while others are secondary windows.
 *
 * @todo Deferred window changes should be done using a queue-type solution
 * where it is possible to schedule multiple tasks into the future separately
 * for each window. Each window could have its own queue.
 *
 * @todo Platform-specific behavior should be encapsulated in subclasses, e.g.,
 * MacWindowBehavior. This would make the code easier to follow and more adaptable
 * to the quirks of each platform.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <QDebug>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <de/GuiApp>
#include <QPaintEvent>
#include <QWidget>
#include <QDesktopWidget>
#include "ui/canvaswindow.h"
#include "clientapp.h"
#include <de/Config>
#include <de/Record>
#include <de/NumberValue>
#include <de/ArrayValue>

#ifdef MACOSX
#  include "ui/displaymode_native.h"
#endif

#include "de_platform.h"

#include "ui/window.h"
#include "ui/displaymode.h"
#include "sys_system.h"
#include "busymode.h"
#include "dd_main.h"
#include "con_main.h"
#include "gl/gl_main.h"
#include "../updater/downloaddialog.h"
#include "ui/ui_main.h"
#include "filesys/fs_util.h"

#include <de/c_wrapper.h>
#include <de/Log>

#define IS_NONZERO(x) ((x) != 0)

using namespace de;

uint mainWindowIdx;

#ifdef MACOSX
static const int WAIT_MILLISECS_AFTER_MODE_CHANGE = 100; // ms
#else
static const int WAIT_MILLISECS_AFTER_MODE_CHANGE = 10; // ms
#endif

/// Used to determine the valid region for windows on the desktop.
/// A window should never go fully (or nearly fully) outside the desktop.
static const int DESKTOP_EDGE_GRACE = 30; // pixels

static QRect desktopRect()
{
    /// @todo Multimonitor? This checks the default screen.
    return QApplication::desktop()->screenGeometry();
}

/*
static QRect desktopValidRect()
{
    return desktopRect().adjusted(DESKTOP_EDGE_GRACE, DESKTOP_EDGE_GRACE,
                                  -DESKTOP_EDGE_GRACE, -DESKTOP_EDGE_GRACE);
}
*/

static void updateMainWindowLayout();
static void updateWindowStateAfterUserChange();
static void useAppliedGeometryForWindows();
static void notifyAboutModeChange();
static void endWindowWait();

struct ddwindow_s
{
    /// The widget this window represents.
    CanvasWindow *widget;

    /// Draw function for rendering the contents of the canvas.
    void (*drawFunc)();

    /// Saved geometry for detecting when changes have occurred.
    QRect appliedGeometry;

    bool needShowFullscreen;
    bool needReshowFullscreen;
    bool needShowNormal;
    bool needRecreateCanvas;
    bool needWait;
    bool willUpdateWindowState;

    /// Current actual geometry.
    RectRaw geometry;

    /// Normal-mode geometry (when not maximized or fullscreen).
    RectRaw normalGeometry;

    int colorDepthBits;
    int flags;

    void inline assertWindow() const
    {
        DENG_ASSERT(this);
        DENG_ASSERT(widget);
    }

    bool inline isBeingAdjusted() const {
        return needShowFullscreen || needReshowFullscreen || needShowNormal || needRecreateCanvas || needWait;
    }

    int x() const      { return geometry.origin.x; }

    int y() const      { return geometry.origin.y; }

    int width() const  { return geometry.size.width; }

    int height() const { return geometry.size.height; }

    QRect rect() const { return QRect(x(), y(), width(), height()); }

    QRect normalRect() const { return QRect(normalGeometry.origin.x,
                                            normalGeometry.origin.y,
                                            normalGeometry.size.width,
                                            normalGeometry.size.height); }

    bool checkFlag(int flag) const { return (flags & flag) != 0; }

    /**
     * Checks all command line options that affect window geometry and applies
     * them to this Window.
     */
    void modifyAccordingToOptions()
    {
        if(CommandLine_Exists("-nofullscreen") || CommandLine_Exists("-window"))
        {
            setFlag(DDWF_FULLSCREEN, false);
        }

        if(CommandLine_Exists("-fullscreen") || CommandLine_Exists("-nowindow"))
        {
            setFlag(DDWF_FULLSCREEN);
        }

        if(CommandLine_CheckWith("-width", 1))
        {
            geometry.size.width = de::max(WINDOW_MIN_WIDTH, atoi(CommandLine_Next()));
            if(!(flags & DDWF_FULLSCREEN))
            {
                normalGeometry.size.width = geometry.size.width;
            }
        }

        if(CommandLine_CheckWith("-height", 1))
        {
            geometry.size.height = de::max(WINDOW_MIN_HEIGHT, atoi(CommandLine_Next()));
            if(!(flags & DDWF_FULLSCREEN))
            {
                normalGeometry.size.height = geometry.size.height;
            }
        }

        if(CommandLine_CheckWith("-winsize", 2))
        {
            geometry.size.width  = de::max(WINDOW_MIN_WIDTH,  atoi(CommandLine_Next()));
            geometry.size.height = de::max(WINDOW_MIN_HEIGHT, atoi(CommandLine_Next()));

            if(!(flags & DDWF_FULLSCREEN))
            {
                normalGeometry.size.width  = geometry.size.width;
                normalGeometry.size.height = geometry.size.height;
            }
        }

        if(CommandLine_CheckWith("-colordepth", 1) || CommandLine_CheckWith("-bpp", 1))
        {
            colorDepthBits = qBound(8, atoi(CommandLine_Next()), 32);
        }

        if(CommandLine_Check("-nocenter"))
        {
            setFlag(DDWF_CENTERED, false);
        }

        if(CommandLine_CheckWith("-xpos", 1))
        {
            normalGeometry.origin.x = atoi(CommandLine_Next());
            setFlag(DDWF_CENTERED | DDWF_MAXIMIZED, false);
        }

        if(CommandLine_CheckWith("-ypos", 1))
        {
            normalGeometry.origin.y = atoi(CommandLine_Next());
            setFlag(DDWF_CENTERED | DDWF_MAXIMIZED, false);
        }

        if(CommandLine_Check("-center"))
        {
            setFlag(DDWF_CENTERED);
        }

        if(CommandLine_Check("-maximize"))
        {
            setFlag(DDWF_MAXIMIZED);
        }

        if(CommandLine_Check("-nomaximize"))
        {
            setFlag(DDWF_MAXIMIZED, false);
        }
    }

    bool applyDisplayMode()
    {
        assertWindow();

        if(!DisplayMode_Count()) return true; // No modes to change to.

        if(flags & DDWF_FULLSCREEN)
        {
            DisplayMode const *mode = DisplayMode_FindClosest(width(), height(), colorDepthBits, 0);
            if(mode && DisplayMode_Change(mode, true /* fullscreen: capture */))
            {
                geometry.size.width  = DisplayMode_Current()->width;
                geometry.size.height = DisplayMode_Current()->height;
#if defined MACOSX
                // Pull the window again over the shield after the mode change.
                DisplayMode_Native_Raise(Window_NativeHandle(this));
#endif
                Window_TrapMouse(this, true);
                return true;
            }
        }
        else
        {
            return DisplayMode_Change(DisplayMode_OriginalMode(),
                                      false /* windowed: don't capture */);
        }
        return false;
    }

    QRect centeredGeometry() const
    {
        QSize winSize = normalRect().size();

        // Center the window.
        QSize screenSize = desktopRect().size();
        LOG_DEBUG("centeredGeometry: Current desktop rect %ix%i") << screenSize.width() << screenSize.height();
        return QRect(desktopRect().topLeft() +
                     QPoint((screenSize.width()  - winSize.width())  / 2,
                            (screenSize.height() - winSize.height()) / 2),
                     winSize);
    }

    /**
     * Applies the information stored in the Window instance to the actual
     * widget geometry. Centering is applied in this stage (it only affects the
     * widget's geometry).
     */
    void applyWindowGeometry()
    {
        LOG_AS("applyWindowGeometry");

        assertWindow();

        // While we're adjusting the window, the window move/resizing callbacks
        // should've mess with the geometry values.
        needWait = true;
        LegacyCore_Timer(WAIT_MILLISECS_AFTER_MODE_CHANGE * 20, endWindowWait);

        bool modeChanged = applyDisplayMode();

        if(modeChanged)
        {
            // Others might be interested to hear about the mode change.
            LegacyCore_Timer(WAIT_MILLISECS_AFTER_MODE_CHANGE, notifyAboutModeChange);
        }

        /*
         * The following is a bit convoluted. The core idea is this, though: on
         * some platforms, changes to the window's mode (normal, maximized,
         * fullscreen/frameless) do not occur immediately. Instead, control
         * needs to return to the event loop and the native window events need
         * to play out. Thus some of the operations have to be performed in a
         * deferred way, after a short wait. The ideal would be to listen to
         * the native events and trigger the necessary updates after they
         * occur; however, now we just use naive time-based delays.
         */

        if(flags & DDWF_FULLSCREEN)
        {
            LOG_DEBUG("fullscreen mode (mode changed? %b)") << modeChanged;

            if(!modeChanged) return; // We don't need to do anything.

            if(widget->isVisible())
            {
                needShowFullscreen = !widget->isFullScreen();

#if defined(WIN32) || defined(Q_OS_LINUX)
                if(widget->isFullScreen())
                {
                    needShowFullscreen = false;
                    needReshowFullscreen = true;
                }
#endif
                LOG_DEBUG("widget is visible, need showFS:%b reshowFS:%b")
                        << needShowFullscreen << needReshowFullscreen;

#ifdef MACOSX
                // Kludge! See updateMainWindowLayout().
                appliedGeometry = QRect(0, 0,
                                        DisplayMode_Current()->width,
                                        DisplayMode_Current()->height);
#endif

                // The window is already visible, so let's allow a mode change to resolve itself
                // before we go changing the window.
                LegacyCore_Timer(WAIT_MILLISECS_AFTER_MODE_CHANGE, updateMainWindowLayout);
            }
            else
            {
                LOG_DEBUG("widget is not visible, setting geometry to %s")
                        << Vector2i(DisplayMode_Current()->width, DisplayMode_Current()->height).asText();

                widget->setGeometry(0, 0, DisplayMode_Current()->width, DisplayMode_Current()->height);
            }
        }
        else
        {
            // The window is in windowed mode (frames and window decoration visible).
            // We will restore it to its previous position and size.
            QRect geom = normalRect(); // Previously stored normal geometry.

            if(flags & DDWF_CENTERED)
            {
                // Center the window.
                geom = centeredGeometry();
            }

            if(flags & DDWF_MAXIMIZED)
            {
                // When a window is maximized, we'll let the native WM handle the sizing.
                if(widget->isVisible())
                {
                    LOG_DEBUG("Window maximized.");
                    widget->showMaximized();
                }

                // Non-visible windows will be shown later
                // (as maximized, if the flag is set).
            }
            else
            {
                // The window is in normal mode: not maximized or fullscreen.

                // If the window is already visible, changes to it need to be deferred
                // so that the native counterpart can be updated, too
                if(widget->isVisible() && (modeChanged || widget->isMaximized()))
                {
                    if(modeChanged)
                    {
                        // We'll wait before the mode change takes full effect.
                        needShowNormal = true;
                        LegacyCore_Timer(WAIT_MILLISECS_AFTER_MODE_CHANGE, updateMainWindowLayout);
                    }
                    else
                    {
                        // Display mode was not changed, so we can immediately
                        // change the window state.
                        widget->showNormal();
                    }
                }

                appliedGeometry = geom;

                if(widget->isVisible())
                {
                    // The native window may not be ready to receive the updated geometry
                    // (e.g., window decoration not made visible yet). We'll apply the
                    // geometry after a delay.
                    LegacyCore_Timer(50 + WAIT_MILLISECS_AFTER_MODE_CHANGE, useAppliedGeometryForWindows);
                }
                else
                {
                    // The native window is not visible yet, so we can apply any number
                    // of changes we like.
                    widget->setGeometry(geom);
                }
            }
        }
    }

    /**
     * Retrieves the actual widget geometry and updates the information stored
     * in the Window instance.
     */
    void fetchWindowGeometry()
    {
        assertWindow();

        setFlag(DDWF_MAXIMIZED, widget->isMaximized());

        QRect rect = widget->geometry();
        geometry.origin.x = rect.x();
        geometry.origin.y = rect.y();
        geometry.size.width = rect.width();
        geometry.size.height = rect.height();

        // If the window is presently maximized or fullscreen, we will not
        // store the actual coordinates.
        if(!widget->isMaximized() && !(flags & DDWF_FULLSCREEN) && !isBeingAdjusted())
        {
            normalGeometry.origin.x = rect.x();
            normalGeometry.origin.y = rect.y();
            normalGeometry.size.width = rect.width();
            DEBUG_Message(("ngw=%i [A]\n", normalGeometry.size.width));
            normalGeometry.size.height = rect.height();
        }

        LOG_DEBUG("Current window geometry: %i,%i %s (max:%b)")
                << geometry.origin.x << geometry.origin.y
                << Vector2i(geometry.size.width, geometry.size.height).asText()
                << ((flags & DDWF_MAXIMIZED) != 0);
        LOG_DEBUG("Normal window geometry: %i,%i %s")
                << normalGeometry.origin.x << normalGeometry.origin.y
                << Vector2i(normalGeometry.size.width, normalGeometry.size.height).asText();
    }

    void setFlag(int flag, bool set = true)
    {
        if(set)
        {
            flags |= flag;
            if(flag & DDWF_MAXIMIZED) LOG_DEBUG("Setting DDWF_MAXIMIZED");
        }
        else
        {
            flags &= ~flag;
            if(flag & DDWF_MAXIMIZED) LOG_DEBUG("Clearing DDWF_MAXIMIZED");
            if(flag & DDWF_CENTERED) LOG_DEBUG("Clearing DDWF_CENTERED");
        }
    }

    bool applyAttributes(int const *attribs)
    {
        LOG_AS("applyAttributes");

        bool changed = false;

        // Parse the attributes array and check the values.
        DENG_ASSERT(attribs);
        for(int i = 0; attribs[i]; ++i)
        {
            switch(attribs[i++])
            {
            case DDWA_X:
                if(x() != attribs[i])
                {
                    normalGeometry.origin.x = attribs[i];
                    setFlag(DDWF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case DDWA_Y:
                if(y() != attribs[i])
                {
                    normalGeometry.origin.y = attribs[i];
                    setFlag(DDWF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case DDWA_WIDTH:
                if(width() != attribs[i])
                {
                    if(attribs[i] < WINDOW_MIN_WIDTH) return false;
                    normalGeometry.size.width = geometry.size.width = attribs[i];
                    DEBUG_Message(("ngw=%i [B]\n", normalGeometry.size.width));
                    setFlag(DDWF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case DDWA_HEIGHT:
                if(height() != attribs[i])
                {
                    if(attribs[i] < WINDOW_MIN_HEIGHT) return false;
                    normalGeometry.size.height = geometry.size.height = attribs[i];
                    setFlag(DDWF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case DDWA_CENTERED:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(checkFlag(DDWF_CENTERED)))
                {
                    setFlag(DDWF_CENTERED, attribs[i]);
                    changed = true;
                }
                break;
            case DDWA_MAXIMIZED:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(checkFlag(DDWF_MAXIMIZED)))
                {
                    setFlag(DDWF_MAXIMIZED, attribs[i]);
                    changed = true;
                }
                break;
            case DDWA_FULLSCREEN:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(checkFlag(DDWF_FULLSCREEN)))
                {
                    if(attribs[i] && Updater_IsDownloadInProgress())
                    {
                        // Can't go to fullscreen when downloading.
                        return false;
                    }
                    setFlag(DDWF_FULLSCREEN, attribs[i]);
                    setFlag(DDWF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case DDWA_VISIBLE:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(checkFlag(DDWF_VISIBLE)))
                {
                    setFlag(DDWF_VISIBLE, attribs[i]);
                    changed = true;
                }
                break;
            case DDWA_COLOR_DEPTH_BITS:
                qDebug() << attribs[i] << colorDepthBits;
                if(attribs[i] != colorDepthBits)
                {
                    colorDepthBits = attribs[i];
                    if(colorDepthBits < 8 || colorDepthBits > 32) return false; // Illegal value.
                    changed = true;
                }
                break;
            default:
                // Unknown attribute.
                return false;
            }
        }

        if(!changed)
        {
            VERBOSE(Con_Message("New window attributes same as before."));
            return true;
        }

        // Seems ok, apply them.
        applyWindowGeometry();
        return true;
    }

    void updateLayout()
    {
        setFlag(DDWF_MAXIMIZED, widget->isMaximized());

        geometry.size.width  = widget->width();
        geometry.size.height = widget->height();

        if(!(flags & DDWF_FULLSCREEN))
        {
            LOG_DEBUG("Updating current view geometry for window, fetched %s")
                    << Vector2i(width(), height()).asText();

            if(!(flags & DDWF_MAXIMIZED) && !isBeingAdjusted())
            {
                // Update the normal-mode geometry (not fullscreen, not maximized).
                normalGeometry.size.width  = geometry.size.width;
                normalGeometry.size.height = geometry.size.height;

                LOG_DEBUG("Updating normal view geometry for window, fetched %s")
                        << Vector2i(width(), height()).asText();
            }
        }
        else
        {
            LOG_DEBUG("Updating view geometry for fullscreen %s")
                    << Vector2i(width(), height()).asText();
        }
    }
};

/// Current active window where all drawing operations occur.
Window const *theWindow;

static boolean winManagerInited = false;

static Window mainWindow;
static boolean mainWindowInited = false;

static void updateMainWindowLayout()
{
    Window *win = Window_Main();

    if(win->needReshowFullscreen)
    {
        LOG_DEBUG("Main window re-set to fullscreen mode.");
        win->needReshowFullscreen = false;
        win->widget->showNormal();
        win->widget->showFullScreen();
    }

    if(win->needShowFullscreen)
    {
        LOG_DEBUG("Main window to fullscreen mode.");
        win->needShowFullscreen = false;
        win->widget->showFullScreen();
    }

    if(win->flags & DDWF_FULLSCREEN)
    {
#if defined MACOSX
        // For some interesting reason, we have to scale the window twice in fullscreen mode
        // or the resulting layout won't be correct.
        win->widget->setGeometry(QRect(0, 0, 320, 240));
        win->widget->setGeometry(win->appliedGeometry);

        DisplayMode_Native_Raise(Window_NativeHandle(win));
#endif
        Window_TrapMouse(win, true);
    }

    if(win->needShowNormal)
    {
        LOG_DEBUG("Main window to normal mode (center:%b).") << ((win->flags & DDWF_CENTERED) != 0);
        win->needShowNormal = false;
        win->widget->showNormal();
    }
}

static void useAppliedGeometryForWindows()
{
    Window *win = Window_Main();
    if(!win || !win->widget) return;

    if(win->flags & DDWF_CENTERED)
    {
        win->appliedGeometry = win->centeredGeometry();
    }

    DEBUG_Message(("Using applied geometry: (%i,%i) %s",
                   win->appliedGeometry.x(),
                   win->appliedGeometry.y(),
                   Vector2i(win->appliedGeometry.width(),
                            win->appliedGeometry.height()).asText().toLatin1().constData()));
    win->widget->setGeometry(win->appliedGeometry);
}

Window *Window_Main()
{
    return &mainWindow;
}

static void notifyAboutModeChange()
{
    LOG_MSG("Display mode has changed.");
    DENG2_GUI_APP->notifyDisplayModeChanged();
}

static void endWindowWait()
{
    Window *win = Window_Main();
    if(win)
    {
        DEBUG_Message(("Window is no longer waiting for geometry changes."));

        // This flag is used for protecting against mode change resizings.
        win->needWait = false;
    }
}

static int getWindowIdx(Window const *wnd)
{
    /// @todo  Multiple windows.
    if(wnd == &mainWindow) return mainWindowIdx;

    return 0;
}

static inline Window *getWindow(uint idx)
{
    if(!winManagerInited)
        return NULL; // Window manager is not initialized.

    if(idx == 1)
        return &mainWindow;

    //DENG_ASSERT(false); // We can only have window 1 (main window).
    return NULL;
}

Window *Window_ByIndex(uint id)
{
    return getWindow(id);
}

void Sys_InitWindowManager()
{
    LOG_AS("Sys_InitWindowManager");

    // Already been here?
    if(winManagerInited)
        return;

    LOG_MSG("Using Qt window management.");

    CanvasWindow::setDefaultGLFormat();

    std::memset(&mainWindow, 0, sizeof(mainWindow));
    winManagerInited = true;
    theWindow = &mainWindow;
}

void Sys_ShutdownWindowManager()
{
    // Presently initialized?
    if(!winManagerInited)
        return;

    /// @todo Delete all windows, not just the main one.

    // Get rid of the windows.
    Window_Delete(Window_Main());

    // Now off-line, no more window management will be possible.
    winManagerInited = false;
}

static Window *canvasToWindow(Canvas &DENG_DEBUG_ONLY(canvas))
{
    DENG_ASSERT(mainWindow.widget->ownsCanvas(&canvas)); /// @todo multiwindow

    return &mainWindow;
}

static void windowFocusChanged(Canvas &canvas, bool focus)
{
    Window *wnd = canvasToWindow(canvas);
    wnd->assertWindow();

    LOG_DEBUG("windowFocusChanged focus:%b fullscreen:%b hidden:%b minimized:%b")
            << focus << Window_IsFullscreen(wnd)
            << wnd->widget->isHidden() << wnd->widget->isMinimized();

    if(!focus)
    {
        DD_ClearEvents();
        I_ResetAllDevices();
        Window_TrapMouse(wnd, false);
    }
    else if(Window_IsFullscreen(wnd))
    {
        // Trap the mouse again in fullscreen mode.
        Window_TrapMouse(wnd, true);
    }

    // Generate an event about this.
    ddevent_t ev;
    ev.type = E_FOCUS;
    ev.focus.gained = focus;
    ev.focus.inWindow = getWindowIdx(wnd);
    DD_PostEvent(&ev);
}

static void finishMainWindowInit(Canvas &canvas)
{
    Window *win = canvasToWindow(canvas);
    DENG_ASSERT(win == &mainWindow);

#if defined MACOSX
    if(Window_IsFullscreen(win))
    {
        // The window must be manually raised above the shielding window put up by
        // the display capture.
        DisplayMode_Native_Raise(Window_NativeHandle(win));
    }
#endif

    win->widget->raise();
    win->widget->activateWindow();

    // Automatically grab the mouse from the get-go if in fullscreen mode.
    if(Mouse_IsPresent() && Window_IsFullscreen(win))
    {
        Window_TrapMouse(&mainWindow, true);
    }

    win->widget->canvas().setFocusFunc(windowFocusChanged);

#ifdef WIN32
    if(Window_IsFullscreen(win))
    {
        // It would seem we must manually give our canvas focus. Bug in Qt?
        win->widget->canvas().setFocus();
    }
#endif

    DD_FinishInitializationAfterWindowReady();
}

static bool windowIsClosing(CanvasWindow &)
{
    LOG_DEBUG("Window is about to close, executing 'quit'.");

    /// @todo autosave and quit?
    Con_Execute(CMDS_DDAY, "quit", true, false);

    // We are not authorizing immediate closing of the window;
    // engine shutdown will take care of it later.
    return false; // don't close
}

/**
 * See the todo notes. Duplicating state is not a good idea.
 */
static void updateWindowStateAfterUserChange()
{
    Window *win = Window_Main();
    if(!win || !win->widget) return;

    win->fetchWindowGeometry();
    win->willUpdateWindowState = false;
}

static void windowWasMoved(CanvasWindow &cw)
{
    LOG_AS("windowWasMoved");

    Window *win = canvasToWindow(cw.canvas());
    DENG_ASSERT(win);

    if(!(win->flags & DDWF_FULLSCREEN) && !win->needWait)
    {
        // The window was moved from its initial position; it is therefore
        // not centered any more (most likely).
        win->setFlag(DDWF_CENTERED, false);
    }

    if(!win->willUpdateWindowState)
    {
        win->willUpdateWindowState = true;
        LegacyCore_Timer(500, updateWindowStateAfterUserChange);
    }
}

void Window_UpdateAfterResize(Window *win)
{
    win->assertWindow();
    win->updateLayout();
}

static Window *createWindow(char const *title)
{
    if(mainWindowInited) return 0; /// @todo  Allow multiple.

    Window *wnd = &mainWindow;
    std::memset(wnd, 0, sizeof(*wnd));
    mainWindowIdx = 1;

    Window_RestoreState(&mainWindow);

    // Create the main window (hidden).
    mainWindow.widget = new CanvasWindow;
    Window_SetTitle(&mainWindow, title);

    // Minimum possible size when resizing.
    mainWindow.widget->setMinimumSize(QSize(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT));

    // After the main window is created, we can finish with the engine init.
    mainWindow.widget->canvas().setInitFunc(finishMainWindowInit);

    mainWindow.widget->setCloseFunc(windowIsClosing);
    mainWindow.widget->setMoveFunc(windowWasMoved);

    // Let's see if there are command line options overriding the previous state.
    mainWindow.modifyAccordingToOptions();

    // Make it so. (Not shown yet.)
    mainWindow.applyWindowGeometry();

#ifdef WIN32
    // Set an icon for the window.
    AutoStr *iconPath = AutoStr_FromText("data\\graphics\\doomsday.ico");
    F_PrependBasePath(iconPath, iconPath);
    LOG_DEBUG("Window icon: ") << NativePath(Str_Text(iconPath)).pretty();
    mainWindow.widget->setWindowIcon(QIcon(Str_Text(iconPath)));
#endif

    /// @todo Refactor for multiwindow support.
    mainWindowInited = true;
    return &mainWindow;
}

Window *Window_New(char const *title)
{
    if(!winManagerInited) return 0;
    return createWindow(title);
}

void Window_Delete(Window *wnd)
{
    if(!wnd || !wnd->widget) return;

    wnd->assertWindow();
    wnd->widget->canvas().setFocusFunc(0);

    // Make sure we'll remember the config.
    Window_SaveState(wnd);

    if(wnd == &mainWindow)
    {
        DisplayMode_Shutdown();
    }

    // Delete the CanvasWindow.
    delete wnd->widget;

    std::memset(wnd, 0, sizeof(*wnd));
}

boolean Window_ChangeAttributes(Window *wnd, int *attribs)
{
    Window oldState = *wnd;

    if(!wnd->applyAttributes(attribs))
    {
        // These weren't good!
        *wnd = oldState;
        return false;
    }

    // Everything ok!
    return true;
}

void Window_SwapBuffers(Window const *win)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();

    DENG_ASSERT(win);
    if(!win->widget) return;

    // Force a swapbuffers right now.
    win->widget->canvas().swapBuffers();
}

DGLuint Window_GrabAsTexture(Window const *win, boolean halfSized)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    win->assertWindow();
    return win->widget->canvas().grabAsTexture(halfSized? QSize(win->width()/2, win->height()/2) : QSize());
}

boolean Window_GrabToFile(Window const *win, char const *fileName)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    win->assertWindow();
    return win->widget->canvas().grabImage().save(fileName);
}

void Window_Grab(Window const *win, image_t *image)
{
    Window_Grab2(win, image, false /* fullsize */);
}

void Window_Grab2(Window const *win, image_t *image, boolean halfSized)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    win->assertWindow();

    win->widget->canvas().grab(image, halfSized? QSize(win->width()/2, win->height()/2) : QSize());
}

void Window_SetTitle(Window const *win, char const *title)
{
    DENG_ASSERT(win);

    LIBDENG_ASSERT_IN_MAIN_THREAD();

    DENG_ASSERT(win->widget);
    win->widget->setWindowTitle(QString::fromLatin1(title));
}

boolean Window_IsFullscreen(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return (wnd->flags & DDWF_FULLSCREEN) != 0;
}

boolean Window_IsCentered(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return (wnd->flags & DDWF_CENTERED) != 0;
}

boolean Window_IsMaximized(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return (wnd->flags & DDWF_MAXIMIZED) != 0;
}

void *Window_NativeHandle(Window const *wnd)
{
    if(!wnd) return 0;
    if(!wnd->widget) return 0;
    return reinterpret_cast<void *>(wnd->widget->winId());
}

void Window_Draw(Window *win)
{
    DENG_ASSERT(win);
    DENG_ASSERT(win->widget);

    // Don't run the main loop until after the paint event has been dealt with.
    ClientApp::app().loop().pause();

    // The canvas needs to be recreated when the GL format has changed
    // (e.g., multisampling).
    if(win->needRecreateCanvas)
    {
        win->needRecreateCanvas = false;
        if(win->widget->recreateCanvas())
        {
            // Wait until the new Canvas is ready.
            return;
        }
    }

    if(Window_ShouldRepaintManually(win))
    {
        LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

        // Perform the drawing manually right away.
        win->widget->canvas().updateGL();
    }
    else
    {
        // Request update at the earliest convenience.
        win->widget->canvas().update();
    }
}

void Window_Show(Window *wnd, boolean show)
{
    DENG_ASSERT(wnd);
    DENG_ASSERT(wnd->widget);

    if(show)
    {
        if(wnd->flags & DDWF_FULLSCREEN)
            wnd->widget->showFullScreen();
        else if(wnd->flags & DDWF_MAXIMIZED)
            wnd->widget->showMaximized();
        else
            wnd->widget->showNormal();

        //qDebug() << "Window_Show: Geometry" << wnd->widget->geometry();
    }
    else
    {
        wnd->widget->hide();
    }
}

int Window_X(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return wnd->x();
}

int Window_Y(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return wnd->y();
}

int Window_Width(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return wnd->width();
}

int Window_Height(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return wnd->height();
}

int Window_NormalX(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return wnd->normalRect().x();
}

int Window_NormalY(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return wnd->normalRect().y();
}

int Window_NormalWidth(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return wnd->normalRect().width();
}

int Window_NormalHeight(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return wnd->normalRect().height();
}

int Window_ColorDepthBits(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return wnd->colorDepthBits;
}

Size2Raw const *Window_Size(Window const *wnd)
{
    DENG_ASSERT(wnd);
    return &wnd->geometry.size;
}

void Window_SaveState(Window *wnd)
{
    DENG_ASSERT(wnd);

    //uint idx = mainWindowIdx;
    //DENG_ASSERT(idx == 1);

    DENG_ASSERT(wnd == &mainWindow); /// @todo  Figure out the window index if there are many.

    Config &config = App::config();

    QRect rect = wnd->rect();
    ArrayValue *array = new ArrayValue;
    *array << NumberValue(rect.left())
           << NumberValue(rect.top())
           << NumberValue(rect.width())
           << NumberValue(rect.height());
    config.names()["window.main.rect"] = array;

    QRect normRect = wnd->normalRect();
    array = new ArrayValue;
    *array << NumberValue(normRect.left())
           << NumberValue(normRect.top())
           << NumberValue(normRect.width())
           << NumberValue(normRect.height());
    config.names()["window.main.normalRect"] = array;

    config.names()["window.main.center"]     = new NumberValue((wnd->flags & DDWF_CENTERED) != 0);
    config.names()["window.main.maximize"]   = new NumberValue((wnd->flags & DDWF_MAXIMIZED) != 0);
    config.names()["window.main.fullscreen"] = new NumberValue((wnd->flags & DDWF_FULLSCREEN) != 0);
    config.names()["window.main.colorDepth"] = new NumberValue(Window_ColorDepthBits(wnd));
}

void Window_RestoreState(Window *wnd)
{
    LOG_AS("Window_RestoreState");
    DENG_ASSERT(wnd);

    DENG_ASSERT(wnd == &mainWindow);  /// @todo  Figure out the window index if there are many.
    //uint idx = mainWindowIdx;
    //DENG_ASSERT(idx == 1);

    Config &config = App::config();

    // The default state of the window is determined by these values.
    ArrayValue &rect = config.geta("window.main.rect");
    if(rect.size() >= 4)
    {
        QRect geom(rect.at(0).asNumber(), rect.at(1).asNumber(),
                   rect.at(2).asNumber(), rect.at(3).asNumber());
        wnd->geometry.origin.x = geom.x();
        wnd->geometry.origin.y = geom.y();
        wnd->geometry.size.width = geom.width();
        wnd->geometry.size.height = geom.height();
    }

    ArrayValue &normalRect = config.geta("window.main.normalRect");
    if(normalRect.size() >= 4)
    {
        QRect geom(normalRect.at(0).asNumber(), normalRect.at(1).asNumber(),
                   normalRect.at(2).asNumber(), normalRect.at(3).asNumber());
        wnd->normalGeometry.origin.x = geom.x();
        wnd->normalGeometry.origin.y = geom.y();
        wnd->normalGeometry.size.width = geom.width();
        wnd->normalGeometry.size.height = geom.height();
    }

    wnd->colorDepthBits = config.geti("window.main.colorDepth");
    wnd->setFlag(DDWF_CENTERED,   config.getb("window.main.center"));
    wnd->setFlag(DDWF_MAXIMIZED,  config.getb("window.main.maximize"));
    wnd->setFlag(DDWF_FULLSCREEN, config.getb("window.main.fullscreen"));
}

void Window_TrapMouse(Window const *wnd, boolean enable)
{
    if(!wnd || !wnd->widget || novideo) return;

    wnd->assertWindow();
    wnd->widget->canvas().trapMouse(enable);
}

boolean Window_IsMouseTrapped(Window const *wnd)
{
    wnd->assertWindow();
    return wnd->widget->canvas().isMouseTrapped();
}

boolean Window_ShouldRepaintManually(Window const *wnd)
{
    //return false;

    // When the pointer is not grabbed, allow the system to regulate window
    // updates (e.g., for window manipulation).
    if(Window_IsFullscreen(wnd)) return true;
    return !Mouse_IsPresent() || Window_IsMouseTrapped(wnd);
}

void Window_UpdateCanvasFormat(Window *wnd)
{
    DENG_ASSERT(wnd != 0);
    wnd->needRecreateCanvas = true;

    // Save the relevant format settings.
    App::config().names()["window.fsaa"] = new NumberValue(Con_GetByte("vid-fsaa") != 0);
}

#if defined(UNIX) && !defined(MACOSX)
void GL_AssertContextActive()
{
    //Window *wnd = Window_Main();
    DENG_ASSERT(QGLContext::currentContext() != 0);
}
#endif

void Window_GLActivate(Window *wnd)
{
    wnd->assertWindow();
    wnd->widget->canvas().makeCurrent();

    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();
}

void Window_GLDone(Window *wnd)
{
    wnd->assertWindow();
    wnd->widget->canvas().doneCurrent();
}

QWidget *Window_Widget(Window *wnd)
{
    if(!wnd) return 0;
    return wnd->widget;
}

CanvasWindow *Window_CanvasWindow(Window *wnd)
{
    if(!wnd) return 0;
    return wnd->widget;
}
