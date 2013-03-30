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

#include <cstdlib>
#include <cstdio>

#include <QDebug>
#include <QPaintEvent>
#include <QWidget>
#include <QDesktopWidget>

#include <de/ArrayValue>
#include <de/Config>
#include <de/GuiApp>
#include <de/Log>
#include <de/NumberValue>
#include <de/Record>
#include <de/Vector>
#include <de/math.h>

#include "ui/canvaswindow.h"
#include "ui/displaymode.h"
#ifdef MACOSX
#  include "ui/displaymode_native.h"
#endif
#include "ui/ui_main.h"
#include "ui/window.h"

#include "clientapp.h"
#include "de_platform.h"
#include "../updater/downloaddialog.h"

#include "sys_system.h"
#include "busymode.h"
#include "dd_main.h"
#include "con_main.h"
#include "gl/gl_main.h"

using namespace de;

#define IS_NONZERO(x) ((x) != 0)

/**
 * @defgroup windowFlags Window Flags.
 */
///@{
#define WF_VISIBLE              0x01
#define WF_CENTERED             0x02
#define WF_MAXIMIZED            0x04
#define WF_FULLSCREEN           0x08
///@}

uint mainWindowIdx;

#ifdef MACOSX
static const int WAIT_MILLISECS_AFTER_MODE_CHANGE = 100; // ms
#else
static const int WAIT_MILLISECS_AFTER_MODE_CHANGE = 10; // ms
#endif

/// Used to determine the valid region for windows on the desktop.
/// A window should never go fully (or nearly fully) outside the desktop.
static const int DESKTOP_EDGE_GRACE = 30; // pixels

static bool winManagerInited = false;
static Window *mainWindow;

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

static QRect centeredGeometry(Window const &wnd)
{
    QSize winSize = wnd.normalRect().size();

    // Center the window.
    QSize screenSize = desktopRect().size();
    LOG_DEBUG("centeredGeometry: Current desktop rect %ix%i") << screenSize.width() << screenSize.height();
    return QRect(desktopRect().topLeft() +
                 QPoint((screenSize.width()  - winSize.width())  / 2,
                        (screenSize.height() - winSize.height()) / 2),
                 winSize);
}

static void notifyAboutModeChange()
{
    LOG_MSG("Display mode has changed.");
    DENG2_GUI_APP->notifyDisplayModeChanged();
}

DENG2_PIMPL(Window)
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

    Instance(Public *i)
        : Base(i),
          widget(new CanvasWindow),
          drawFunc(0)
    {
        // Minimum possible size when resizing.
        widget->setMinimumSize(QSize(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT));

        widget->setCloseFunc(windowIsClosing);
        widget->setMoveFunc(windowWasMoved);
    }

    void inline assertWindow() const
    {
        DENG_ASSERT(widget);
    }

    bool inline isBeingAdjusted() const {
        return needShowFullscreen || needReshowFullscreen || needShowNormal || needRecreateCanvas || needWait;
    }

    inline bool isFlagged(int flag) const {
        return (flags & flag) != 0;
    }

    /**
     * Checks all command line options that affect window geometry and applies
     * them to this Window.
     */
    void modifyAccordingToOptions()
    {
        if(CommandLine_Exists("-nofullscreen") || CommandLine_Exists("-window"))
        {
            setFlag(WF_FULLSCREEN, false);
        }

        if(CommandLine_Exists("-fullscreen") || CommandLine_Exists("-nowindow"))
        {
            setFlag(WF_FULLSCREEN);
        }

        if(CommandLine_CheckWith("-width", 1))
        {
            geometry.size.width = de::max(WINDOW_MIN_WIDTH, atoi(CommandLine_Next()));
            if(!(flags & WF_FULLSCREEN))
            {
                normalGeometry.size.width = geometry.size.width;
            }
        }

        if(CommandLine_CheckWith("-height", 1))
        {
            geometry.size.height = de::max(WINDOW_MIN_HEIGHT, atoi(CommandLine_Next()));
            if(!(flags & WF_FULLSCREEN))
            {
                normalGeometry.size.height = geometry.size.height;
            }
        }

        if(CommandLine_CheckWith("-winsize", 2))
        {
            geometry.size.width  = de::max(WINDOW_MIN_WIDTH,  atoi(CommandLine_Next()));
            geometry.size.height = de::max(WINDOW_MIN_HEIGHT, atoi(CommandLine_Next()));

            if(!(flags & WF_FULLSCREEN))
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
            setFlag(WF_CENTERED, false);
        }

        if(CommandLine_CheckWith("-xpos", 1))
        {
            normalGeometry.origin.x = atoi(CommandLine_Next());
            setFlag(WF_CENTERED | WF_MAXIMIZED, false);
        }

        if(CommandLine_CheckWith("-ypos", 1))
        {
            normalGeometry.origin.y = atoi(CommandLine_Next());
            setFlag(WF_CENTERED | WF_MAXIMIZED, false);
        }

        if(CommandLine_Check("-center"))
        {
            setFlag(WF_CENTERED);
        }

        if(CommandLine_Check("-maximize"))
        {
            setFlag(WF_MAXIMIZED);
        }

        if(CommandLine_Check("-nomaximize"))
        {
            setFlag(WF_MAXIMIZED, false);
        }
    }

    bool applyDisplayMode()
    {
        assertWindow();

        if(!DisplayMode_Count()) return true; // No modes to change to.

        if(flags & WF_FULLSCREEN)
        {
            DisplayMode const *mode = DisplayMode_FindClosest(geometry.size.width, geometry.size.height,
                                                              colorDepthBits, 0);

            if(mode && DisplayMode_Change(mode, true /* fullscreen: capture */))
            {
                geometry.size.width  = DisplayMode_Current()->width;
                geometry.size.height = DisplayMode_Current()->height;
#if defined MACOSX
                // Pull the window again over the shield after the mode change.
                DisplayMode_Native_Raise(self.nativeHandle());
#endif
                self.trapMouse(true);
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

        if(flags & WF_FULLSCREEN)
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
            QRect geom = self.normalRect(); // Previously stored normal geometry.

            if(flags & WF_CENTERED)
            {
                // Center the window.
                geom = centeredGeometry(self);
            }

            if(flags & WF_MAXIMIZED)
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

        setFlag(WF_MAXIMIZED, widget->isMaximized());

        QRect rect = widget->geometry();
        geometry.origin.x = rect.x();
        geometry.origin.y = rect.y();
        geometry.size.width = rect.width();
        geometry.size.height = rect.height();

        // If the window is presently maximized or fullscreen, we will not
        // store the actual coordinates.
        if(!widget->isMaximized() && !(flags & WF_FULLSCREEN) && !isBeingAdjusted())
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
                << ((flags & WF_MAXIMIZED) != 0);
        LOG_DEBUG("Normal window geometry: %i,%i %s")
                << normalGeometry.origin.x << normalGeometry.origin.y
                << Vector2i(normalGeometry.size.width, normalGeometry.size.height).asText();
    }

    void setFlag(int flag, bool set = true)
    {
        if(set)
        {
            flags |= flag;
        }
        else
        {
            flags &= ~flag;
            if(flag & WF_CENTERED) LOG_DEBUG("Clearing WF_CENTERED");
        }
    }

    void applyAttributes(int const *attribs)
    {
        LOG_AS("applyAttributes");

        bool changed = false;

        // Parse the attributes array and check the values.
        DENG_ASSERT(attribs);
        for(int i = 0; attribs[i]; ++i)
        {
            switch(attribs[i++])
            {
            case Window::X:
                if(geometry.origin.x != attribs[i])
                {
                    normalGeometry.origin.x = attribs[i];
                    changed = true;
                }
                break;
            case Window::Y:
                if(geometry.origin.y != attribs[i])
                {
                    normalGeometry.origin.y = attribs[i];
                    changed = true;
                }
                break;
            case Window::Width:
                if(geometry.size.width != attribs[i])
                {
                    DENG_ASSERT(attribs[i] >= WINDOW_MIN_WIDTH);
                    normalGeometry.size.width = geometry.size.width = attribs[i];
                    DEBUG_Message(("ngw=%i [B]\n", normalGeometry.size.width));
                    changed = true;
                }
                break;
            case Window::Height:
                if(geometry.size.height != attribs[i])
                {
                    DENG_ASSERT(attribs[i] >= WINDOW_MIN_HEIGHT);
                    normalGeometry.size.height = geometry.size.height = attribs[i];
                    changed = true;
                }
                break;
            case Window::Centered:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(isFlagged(WF_CENTERED)))
                {
                    setFlag(WF_CENTERED, attribs[i]);
                    changed = true;
                }
                break;
            case Window::Maximized:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(isFlagged(WF_MAXIMIZED)))
                {
                    setFlag(WF_MAXIMIZED, attribs[i]);
                    changed = true;
                }
                break;
            case Window::Fullscreen:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(isFlagged(WF_FULLSCREEN)))
                {
                    DENG_ASSERT(!(attribs[i] && Updater_IsDownloadInProgress()));
                    setFlag(WF_FULLSCREEN, attribs[i]);
                    changed = true;
                }
                break;
            case Window::Visible:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(isFlagged(WF_VISIBLE)))
                {
                    setFlag(WF_VISIBLE, attribs[i]);
                    changed = true;
                }
                break;
            case Window::ColorDepthBits:
                qDebug() << attribs[i] << colorDepthBits;
                if(attribs[i] != colorDepthBits)
                {
                    colorDepthBits = attribs[i];
                    DENG_ASSERT(colorDepthBits >= 8 && colorDepthBits <= 32);
                    changed = true;
                }
                break;
            default:
                // Unknown attribute.
                DENG_ASSERT(false);
            }
        }

        // No change?
        if(!changed)
        {
            VERBOSE(Con_Message("New window attributes same as before."));
            return;
        }

        // Apply them.
        applyWindowGeometry();
    }

    void updateLayout()
    {
        setFlag(WF_MAXIMIZED, widget->isMaximized());

        geometry.size.width  = widget->width();
        geometry.size.height = widget->height();

        if(!(flags & WF_FULLSCREEN))
        {
            LOG_DEBUG("Updating current view geometry for window, fetched %s")
                    << Vector2i(geometry.size.width, geometry.size.height).asText();

            if(!(flags & WF_MAXIMIZED) && !isBeingAdjusted())
            {
                // Update the normal-mode geometry (not fullscreen, not maximized).
                normalGeometry.size.width  = geometry.size.width;
                normalGeometry.size.height = geometry.size.height;

                LOG_DEBUG("Updating normal view geometry for window, fetched %s")
                        << Vector2i(normalGeometry.size.width, normalGeometry.size.height).asText();
            }
        }
        else
        {
            LOG_DEBUG("Updating view geometry for fullscreen %s")
                    << Vector2i(geometry.size.width, geometry.size.height).asText();
        }
    }

    static void updateMainWindowLayout()
    {
        Window *wnd = Window::mainPtr();

        if(wnd->d->needReshowFullscreen)
        {
            LOG_DEBUG("Main window re-set to fullscreen mode.");
            wnd->d->needReshowFullscreen = false;
            wnd->d->widget->showNormal();
            wnd->d->widget->showFullScreen();
        }

        if(wnd->d->needShowFullscreen)
        {
            LOG_DEBUG("Main window to fullscreen mode.");
            wnd->d->needShowFullscreen = false;
            wnd->d->widget->showFullScreen();
        }

        if(wnd->d->flags & WF_FULLSCREEN)
        {
#if defined MACOSX
            // For some interesting reason, we have to scale the window twice in fullscreen mode
            // or the resulting layout won't be correct.
            wnd->d->widget->setGeometry(QRect(0, 0, 320, 240));
            wnd->d->widget->setGeometry(wnd->d->appliedGeometry);

            DisplayMode_Native_Raise(wnd->nativeHandle());
#endif
            wnd->trapMouse();
        }

        if(wnd->d->needShowNormal)
        {
            LOG_DEBUG("Main window to normal mode (center:%b).") << ((wnd->d->flags & WF_CENTERED) != 0);
            wnd->d->needShowNormal = false;
            wnd->d->widget->showNormal();
        }
    }

    static void useAppliedGeometryForWindows()
    {
        Window *wnd = Window::mainPtr();
        if(!wnd || !wnd->d->widget) return;

        if(wnd->d->flags & WF_CENTERED)
        {
            wnd->d->appliedGeometry = centeredGeometry(*wnd);
        }

        DEBUG_Message(("Using applied geometry: (%i,%i) %s",
                       wnd->d->appliedGeometry.x(),
                       wnd->d->appliedGeometry.y(),
                       Vector2i(wnd->d->appliedGeometry.width(),
                                wnd->d->appliedGeometry.height()).asText()));
        wnd->d->widget->setGeometry(wnd->d->appliedGeometry);
    }

    static void endWindowWait()
    {
        Window *wnd = Window::mainPtr();
        if(wnd)
        {
            DEBUG_Message(("Window is no longer waiting for geometry changes."));

            // This flag is used for protecting against mode change resizings.
            wnd->d->needWait = false;
        }
    }

    static Window *canvasToWindow(Canvas &DENG_DEBUG_ONLY(canvas))
    {
        DENG_ASSERT(mainWindow->d->widget->ownsCanvas(&canvas)); /// @todo multiwindow

        return mainWindow;
    }

    static int getWindowIdx(Window const &wnd)
    {
        /// @todo  Multiple windows.
        if(&wnd == mainWindow)
            return mainWindowIdx;
        return 0;
    }

    static void windowFocusChanged(Canvas &canvas, bool focus)
    {
        Window *wnd = canvasToWindow(canvas);
        wnd->d->assertWindow();

        LOG_DEBUG("windowFocusChanged focus:%b fullscreen:%b hidden:%b minimized:%b")
                << focus << wnd->isFullscreen()
                << wnd->d->widget->isHidden() << wnd->d->widget->isMinimized();

        if(!focus)
        {
            DD_ClearEvents();
            I_ResetAllDevices();
            wnd->trapMouse(false);
        }
        else if(wnd->isFullscreen())
        {
            // Trap the mouse again in fullscreen mode.
            wnd->trapMouse();
        }

        // Generate an event about this.
        ddevent_t ev;
        ev.type           = E_FOCUS;
        ev.focus.gained   = focus;
        ev.focus.inWindow = getWindowIdx(*wnd);
        DD_PostEvent(&ev);
    }

    static void finishMainWindowInit(Canvas &canvas)
    {
        Window *wnd = canvasToWindow(canvas);
        DENG_ASSERT(wnd == mainWindow);

#if defined MACOSX
        if(wnd->isFullscreen())
        {
            // The window must be manually raised above the shielding window put up by
            // the display capture.
            DisplayMode_Native_Raise(wnd->nativeHandle());
        }
#endif

        wnd->d->widget->raise();
        wnd->d->widget->activateWindow();

        // Automatically grab the mouse from the get-go if in fullscreen mode.
        if(Mouse_IsPresent() && wnd->isFullscreen())
        {
            wnd->trapMouse();
        }

        wnd->d->widget->canvas().setFocusFunc(windowFocusChanged);

#ifdef WIN32
        if(wnd->isFullscreen())
        {
            // It would seem we must manually give our canvas focus. Bug in Qt?
            wnd->d->widget->canvas().setFocus();
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
        Window *wnd = Window::mainPtr();
        if(!wnd || !wnd->d->widget) return;

        wnd->d->fetchWindowGeometry();
        wnd->d->willUpdateWindowState = false;
    }

    static void windowWasMoved(CanvasWindow &cw)
    {
        LOG_AS("windowWasMoved");

        Window *wnd = canvasToWindow(cw.canvas());
        DENG_ASSERT(wnd != 0);

        if(!(wnd->d->flags & WF_FULLSCREEN) && !wnd->d->needWait)
        {
            // The window was moved from its initial position; it is therefore
            // not centered any more (most likely).
            wnd->d->setFlag(WF_CENTERED, false);
        }

        if(!wnd->d->willUpdateWindowState)
        {
            wnd->d->willUpdateWindowState = true;
            LegacyCore_Timer(500, updateWindowStateAfterUserChange);
        }
    }
};

void Window::initialize()
{
    LOG_AS("Window::initialize");

    // Already been here?
    if(winManagerInited)
        return;

    LOG_MSG("Using Qt window management.");

    CanvasWindow::setDefaultGLFormat();

    winManagerInited = true;
}

void Window::shutdown()
{
    // Presently initialized?
    if(!winManagerInited)
        return;

    /// @todo Delete all windows, not just the main one.

    // Get rid of the windows.
    DENG_ASSERT(mainWindow != 0);
    delete mainWindow; mainWindow = 0;

    // Now off-line, no more window management will be possible.
    winManagerInited = false;
}

Window *Window::create(char const *title)
{
    if(mainWindow) return 0; /// @todo  Allow multiple.

    Window *wnd = new Window();

    // Is this the main window?
    if(!mainWindow)
    {
        mainWindow = wnd;
        mainWindowIdx = 1;

        // After the main window is created, we can finish with the engine init.
        wnd->d->widget->canvas().setInitFunc(Instance::finishMainWindowInit);
    }

    wnd->restoreState();
    wnd->setTitle(title);

    // Let's see if there are command line options overriding the previous state.
    wnd->d->modifyAccordingToOptions();

    // Make it so. (Not shown yet.)
    wnd->d->applyWindowGeometry();

#ifdef WIN32
    // Set an icon for the window.
    Path iconPath = DENG2_APP->nativeBasePath() / ("data\\graphics\\doomsday.ico");
    LOG_DEBUG("Window icon: ") << NativePath(iconPath).pretty();
    wnd->d->widget->setWindowIcon(QIcon(iconPath));
#endif

    return wnd;
}

bool Window::haveMain()
{
    return mainWindow != 0;
}

Window &Window::main()
{
    if(mainWindow)
    {
        return *mainWindow;
    }
    /// @throw MissingWindowError Attempted to reference a non-existant window.
    throw MissingWindowError("Window::main", "No main window is presently available");
}

Window *Window::byIndex(uint idx)
{
    if(!winManagerInited)
        return 0; // Window manager is not initialized.

    if(idx == 1)
        return mainWindow;

    //DENG_ASSERT(false); // We can only have window 1 (main window).
    return 0;
}

Window::Window(char const *title) : d(new Instance(this))
{
    setTitle(title);
}

Window::~Window()
{
    if(!d->widget) return;

    d->assertWindow();
    d->widget->canvas().setFocusFunc(0);

    // Make sure we'll remember the config.
    saveState();

    if(this == mainWindow)
    {
        DisplayMode_Shutdown();
    }

    // Delete the CanvasWindow.
    delete d->widget;
}

void Window::updateAfterResize()
{
    d->assertWindow();
    d->updateLayout();
}

static bool validateAttributes(int const *attribs)
{
    // Parse the attributes array and check the values.
    DENG_ASSERT(attribs);
    for(int i = 0; attribs[i]; ++i)
    {
        switch(attribs[i++])
        {
        case Window::Width:
            if(attribs[i] < WINDOW_MIN_WIDTH)
                return false;
            break;
        case Window::Height:
            if(attribs[i] < WINDOW_MIN_HEIGHT)
                return false;
            break;
        case Window::Fullscreen:
            // Can't go to fullscreen when downloading.
            if(attribs[i] && Updater_IsDownloadInProgress())
                return false;
            break;
        case Window::ColorDepthBits:
            if(attribs[i] < 8 || attribs[i] > 32)
                return false; // Illegal value.
            break;
        default:
            // Unknown attribute.
            return false;
        }
    }

    // Seems ok.
    return true;
}

bool Window::changeAttributes(int const *attribs)
{
    if(validateAttributes(attribs))
    {
        d->applyAttributes(attribs);
        return true;
    }

    // These weren't good!
    return false;
}

void Window::swapBuffers() const
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();

    if(!d->widget) return;

    // Force a swapbuffers right now.
    d->widget->canvas().swapBuffers();
}

void Window::grab(image_t &image, bool halfSized) const
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    d->assertWindow();

    d->widget->canvas().grab(&image, halfSized? QSize(d->geometry.size.width/2,
                                                      d->geometry.size.height/2) : QSize());
}

DGLuint Window::grabAsTexture(bool halfSized) const
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    d->assertWindow();
    return d->widget->canvas().grabAsTexture(halfSized? QSize(d->geometry.size.width/2,
                                                              d->geometry.size.height/2) : QSize());
}

bool Window::grabToFile(char const *fileName) const
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    d->assertWindow();
    return d->widget->canvas().grabImage().save(fileName);
}

void Window::setTitle(char const *title) const
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();

    DENG_ASSERT(d->widget);
    d->widget->setWindowTitle(QString::fromLatin1(title));
}

bool Window::isFullscreen() const
{
    return (d->flags & WF_FULLSCREEN) != 0;
}

bool Window::isCentered() const
{
    return (d->flags & WF_CENTERED) != 0;
}

bool Window::isMaximized() const
{
    return (d->flags & WF_MAXIMIZED) != 0;
}

void *Window::nativeHandle() const
{
    if(!d->widget) return 0;
    return reinterpret_cast<void *>(d->widget->winId());
}

void Window::draw()
{
    DENG_ASSERT(d->widget);

    // Don't run the main loop until after the paint event has been dealt with.
    ClientApp::app().loop().pause();

    // The canvas needs to be recreated when the GL format has changed
    // (e.g., multisampling).
    if(d->needRecreateCanvas)
    {
        d->needRecreateCanvas = false;
        if(d->widget->recreateCanvas())
        {
            // Wait until the new Canvas is ready.
            return;
        }
    }

    if(shouldRepaintManually())
    {
        LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

        // Perform the drawing manually right away.
        d->widget->canvas().updateGL();
    }
    else
    {
        // Request update at the earliest convenience.
        d->widget->canvas().update();
    }
}

void Window::show(bool show)
{
    DENG_ASSERT(d->widget);

    if(show)
    {
        if(d->flags & WF_FULLSCREEN)
            d->widget->showFullScreen();
        else if(d->flags & WF_MAXIMIZED)
            d->widget->showMaximized();
        else
            d->widget->showNormal();

        //qDebug() << "Window::show: Geometry" << d->widget->geometry();
    }
    else
    {
        d->widget->hide();
    }
}

QRect Window::rect() const
{
    return QRect(d->geometry.origin.x, d->geometry.origin.y,
                 d->geometry.size.width, d->geometry.size.height);
}

QRect Window::normalRect() const
{
    return QRect(d->normalGeometry.origin.x, d->normalGeometry.origin.y,
                 d->normalGeometry.size.width, d->normalGeometry.size.height);
}

int Window::colorDepthBits() const
{
    return d->colorDepthBits;
}

void Window::saveState()
{
    //uint idx = mainWindowIdx;
    //DENG_ASSERT(idx == 1);

    DENG_ASSERT(this == mainWindow); /// @todo  Figure out the window index if there are many.

    Config &config = App::config();

    QRect geom = rect();
    ArrayValue *array = new ArrayValue;
    *array << NumberValue(geom.left())
           << NumberValue(geom.top())
           << NumberValue(geom.width())
           << NumberValue(geom.height());
    config.names()["window.main.rect"] = array;

    QRect normGeom = normalRect();
    array = new ArrayValue;
    *array << NumberValue(normGeom.left())
           << NumberValue(normGeom.top())
           << NumberValue(normGeom.width())
           << NumberValue(normGeom.height());
    config.names()["window.main.normalRect"] = array;

    config.names()["window.main.center"]     = new NumberValue((d->flags & WF_CENTERED) != 0);
    config.names()["window.main.maximize"]   = new NumberValue((d->flags & WF_MAXIMIZED) != 0);
    config.names()["window.main.fullscreen"] = new NumberValue((d->flags & WF_FULLSCREEN) != 0);
    config.names()["window.main.colorDepth"] = new NumberValue(colorDepthBits());
}

void Window::restoreState()
{
    LOG_AS("Window::restoreState");

    DENG_ASSERT(this == mainWindow);  /// @todo  Figure out the window index if there are many.
    //uint idx = mainWindowIdx;
    //DENG_ASSERT(idx == 1);

    Config &config = App::config();

    // The default state of the window is determined by these values.
    ArrayValue &rect = config.geta("window.main.rect");
    if(rect.size() >= 4)
    {
        QRect geom(rect.at(0).asNumber(), rect.at(1).asNumber(),
                   rect.at(2).asNumber(), rect.at(3).asNumber());
        d->geometry.origin.x = geom.x();
        d->geometry.origin.y = geom.y();
        d->geometry.size.width = geom.width();
        d->geometry.size.height = geom.height();
    }

    ArrayValue &normalRect = config.geta("window.main.normalRect");
    if(normalRect.size() >= 4)
    {
        QRect geom(normalRect.at(0).asNumber(), normalRect.at(1).asNumber(),
                   normalRect.at(2).asNumber(), normalRect.at(3).asNumber());
        d->normalGeometry.origin.x = geom.x();
        d->normalGeometry.origin.y = geom.y();
        d->normalGeometry.size.width = geom.width();
        d->normalGeometry.size.height = geom.height();
    }

    d->colorDepthBits = config.geti("window.main.colorDepth");
    d->setFlag(WF_CENTERED,   config.getb("window.main.center"));
    d->setFlag(WF_MAXIMIZED,  config.getb("window.main.maximize"));
    d->setFlag(WF_FULLSCREEN, config.getb("window.main.fullscreen"));
}

void Window::trapMouse(bool enable) const
{
    if(!d->widget || novideo) return;

    d->assertWindow();
    d->widget->canvas().trapMouse(enable);
}

bool Window::isMouseTrapped() const
{
    d->assertWindow();
    return d->widget->canvas().isMouseTrapped();
}

bool Window::shouldRepaintManually() const
{
    //return false;

    // When the pointer is not grabbed, allow the system to regulate window
    // updates (e.g., for window manipulation).
    if(isFullscreen()) return true;
    return !Mouse_IsPresent() || isMouseTrapped();
}

void Window::updateCanvasFormat()
{
    d->needRecreateCanvas = true;

    // Save the relevant format settings.
    App::config().names()["window.fsaa"] = new NumberValue(Con_GetByte("vid-fsaa") != 0);
}

#if defined(UNIX) && !defined(MACOSX)
void GL_AssertContextActive()
{
    //Window *wnd = Window::main();
    DENG_ASSERT(QGLContext::currentContext() != 0);
}
#endif

void Window::glActivate()
{
    d->assertWindow();
    d->widget->canvas().makeCurrent();

    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();
}

void Window::glDone()
{
    d->assertWindow();
    d->widget->canvas().doneCurrent();
}

QWidget *Window::widgetPtr()
{
    return d->widget;
}

CanvasWindow &Window::canvasWindow()
{
    DENG_ASSERT(d->widget);
    return *d->widget;
}
