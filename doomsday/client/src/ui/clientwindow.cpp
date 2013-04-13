/** @file clientwindow.cpp  Top-level window with UI widgets.
 * @ingroup base
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

#include "ui/clientwindow.h"
#include "clientapp.h"
#include <de/DisplayMode>
#include <de/NumberValue>
#include <QGLFormat>
#include <QCloseEvent>

#include "gl/sys_opengl.h"
#include "gl/gl_main.h"
#include "ui/legacywidget.h"
#include "ui/busywidget.h"
#include "ui/mouse_qt.h"

#include "dd_main.h"
#include "con_main.h"

#if 0
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

// --------------------------------------------------------------------------

#include <QApplication>
#include <QGLFormat>
#include <QMoveEvent>
#include <de/App>
#include <de/Config>
#include <de/Record>
#include <de/NumberValue>
#include <de/Log>
#include <de/RootWidget>
#include <de/c_wrapper.h>

#include "de_platform.h"
#include "dd_loop.h"
#include "con_main.h"
#ifdef __CLIENT__
#  include "gl/gl_main.h"
#endif
#include "ui/canvaswindow.h"
#include "clientapp.h"

#include <assert.h>
#include <QThread>
#endif

using namespace de;

static String const LEGACY_WIDGET_NAME = "legacy";

DENG2_PIMPL(ClientWindow),
    DENG2_OBSERVES(KeyEventSource,   KeyEvent),
    DENG2_OBSERVES(MouseEventSource, MouseStateChange),
#ifndef WIN32
    DENG2_OBSERVES(MouseEventSource, MouseAxisEvent),
    DENG2_OBSERVES(MouseEventSource, MouseButtonEvent),
#endif
    DENG2_OBSERVES(Canvas,           FocusChange)
{
    bool needMainInit;
    bool needRecreateCanvas;

    Mode mode;

    /// Root of the nomal UI widgets of this window.
    GuiRootWidget root;

    GuiRootWidget busyRoot;

    Instance(Public *i)
        : Base(i),
          needMainInit(true),
          needRecreateCanvas(false),
          mode(Normal),
          root(thisPublic),
          busyRoot(thisPublic)
    {
        LegacyWidget *legacy = new LegacyWidget(LEGACY_WIDGET_NAME);
        legacy->rule()
                .setLeftTop    (root.viewLeft(),  root.viewTop())
                .setRightBottom(root.viewRight(), root.viewBottom());
        root.add(legacy);

        // Initially the widget is disabled. It will be enabled when the window
        // is visible and ready to be drawn.
        legacy->disable();

        // For busy mode we have an entirely different widget tree.
        BusyWidget *busy = new BusyWidget;
        busy->rule()
                .setLeftTop    (busyRoot.viewLeft(),  busyRoot.viewTop())
                .setRightBottom(busyRoot.viewRight(), busyRoot.viewBottom());
        busyRoot.add(busy);

        /// @todo The decision whether to receive input notifications from the
        /// canvas is really a concern for the input drivers.

        // Listen to input.
        self.canvas().audienceForKeyEvent += this;
        self.canvas().audienceForMouseStateChange += this;

#ifndef WIN32 // On Windows, DirectInput bypasses the mouse input from Canvas.
        self.canvas().audienceForMouseAxisEvent += this;
        self.canvas().audienceForMouseButtonEvent += this;
#endif
    }

    ~Instance()
    {
        self.canvas().audienceForFocusChange -= this;
        self.canvas().audienceForMouseStateChange -= this;
        self.canvas().audienceForKeyEvent -= this;
    }

    void setMode(Mode const &newMode)
    {
        LOG_VERBOSE("Switching to %s mode") << (newMode == Busy? "Busy" : "Normal");

        mode = newMode;
    }

    void finishMainWindowInit()
    {
#ifdef MACOSX
        if(self.isFullScreen())
        {
            // The window must be manually raised above the shielding window put up by
            // the fullscreen display capture.
            DisplayMode_Native_Raise(self.nativeHandle());
        }
#endif

        self.raise();
        self.activateWindow();

        // Automatically grab the mouse from the get-go if in fullscreen mode.
        if(Mouse_IsPresent() && self.isFullScreen())
        {
            self.canvas().trapMouse();
        }

        self.canvas().audienceForFocusChange += this;

#ifdef WIN32
        if(self.isFullScreen())
        {
            // It would seem we must manually give our canvas focus. Bug in Qt?
            self.canvas().setFocus();
        }
#endif

        DD_FinishInitializationAfterWindowReady();
    }

    void keyEvent(KeyEvent const &ev)
    {
        /// @todo Input drivers should observe the notification instead, input
        /// subsystem passes it to window system. -jk

        // Pass the event onto the window system.
        ClientApp::windowSystem().processEvent(ev);
    }

    void mouseStateChanged(MouseEventSource::State state)
    {
        Mouse_Trap(state == MouseEventSource::Trapped);
    }

#ifndef WIN32
    void mouseAxisEvent(MouseEventSource::Axis axis, Vector2i const &value)
    {
        switch(axis)
        {
        case MouseEventSource::Motion:
            Mouse_Qt_SubmitMotion(IMA_POINTER, value.x, value.y);
            break;

        case MouseEventSource::Wheel:
            Mouse_Qt_SubmitMotion(IMA_WHEEL, value.x, value.y);
            break;

        default:
            break;
        }
    }

    void mouseButtonEvent(MouseEventSource::Button button, MouseEventSource::ButtonState state)
    {
        Mouse_Qt_SubmitButton(
                    button == MouseEventSource::Left?     IMB_LEFT :
                    button == MouseEventSource::Middle?   IMB_MIDDLE :
                    button == MouseEventSource::Right?    IMB_RIGHT :
                    button == MouseEventSource::XButton1? IMB_EXTRA1 :
                    button == MouseEventSource::XButton2? IMB_EXTRA2 : IMB_MAXBUTTONS,
                    state == MouseEventSource::Pressed);
    }
#endif // !WIN32

    void canvasFocusChanged(Canvas &canvas, bool hasFocus)
    {
        LOG_DEBUG("canvasFocusChanged focus:%b fullscreen:%b hidden:%b minimized:%b")
                << hasFocus << self.isFullScreen() << self.isHidden() << self.isMinimized();

        if(!hasFocus)
        {
            DD_ClearEvents();
            I_ResetAllDevices();
            canvas.trapMouse(false);
        }
        else if(self.isFullScreen())
        {
            // Trap the mouse again in fullscreen mode.
            canvas.trapMouse();
        }

        // Generate an event about this.
        ddevent_t ev;
        ev.type           = E_FOCUS;
        ev.focus.gained   = hasFocus;
        ev.focus.inWindow = 1; /// @todo Ask WindowSystem for an identifier number.
        DD_PostEvent(&ev);
    }
};

ClientWindow::ClientWindow(String const &id)
    : PersistentCanvasWindow(id), d(new Instance(this))
{
    canvas().audienceForGLResize += this;
    canvas().audienceForGLInit += this;

#ifdef WIN32
    // Set an icon for the window.
    Path iconPath = DENG2_APP->nativeBasePath() / "data\\graphics\\doomsday.ico";
    LOG_DEBUG("Window icon: ") << NativePath(iconPath).pretty();
    setWindowIcon(QIcon(iconPath));
#endif
}

GuiRootWidget &ClientWindow::root()
{
    return d->mode == Busy? d->busyRoot : d->root;
}

void ClientWindow::setMode(Mode const &mode)
{
    LOG_AS("ClientWindow");

    d->setMode(mode);
}

void ClientWindow::closeEvent(QCloseEvent *ev)
{
    LOG_DEBUG("Window is about to close, executing 'quit'.");

    /// @todo autosave and quit?
    Con_Execute(CMDS_DDAY, "quit", true, false);

    // We are not authorizing immediate closing of the window;
    // engine shutdown will take care of it later.
    ev->ignore(); // don't close
}

void ClientWindow::canvasGLReady(Canvas &canvas)
{
    // Update the capability flags.
    GL_state.features.multisample = canvas.format().sampleBuffers();
    LOG_DEBUG("GL feature: Multisampling: %b") << GL_state.features.multisample;

    PersistentCanvasWindow::canvasGLReady(canvas);

    // Now that the Canvas is ready for drawing we can enable the LegacyWidget.
    d->root.find(LEGACY_WIDGET_NAME)->enable();

    LOG_DEBUG("LegacyWidget enabled");

    if(d->needMainInit)
    {
        d->needMainInit = false;
        d->finishMainWindowInit();
    }
}

void ClientWindow::canvasGLInit(Canvas &)
{
    Sys_GLConfigureDefaultState();
    GL_Init2DState();
}

void ClientWindow::canvasGLDraw(Canvas &canvas)
{
    // All of this occurs during the Canvas paintGL event.

    ClientApp::app().preFrame(); /// @todo what about multiwindow?

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    root().draw();

    // Finish GL drawing and swap it on to the screen. Blocks until buffers
    // swapped.
    GL_DoUpdate();

    ClientApp::app().postFrame(); /// @todo what about multiwindow?

    PersistentCanvasWindow::canvasGLDraw(canvas);
}

void ClientWindow::canvasGLResized(Canvas &canvas)
{
    LOG_AS("ClientWindow");

    Vector2i size = canvas.size();
    LOG_DEBUG("Canvas resized to ") << size.asText();

    // Tell the widgets.
    d->root.setViewSize(size);
    d->busyRoot.setViewSize(size);
}

bool ClientWindow::setDefaultGLFormat() // static
{
    LOG_AS("DefaultGLFormat");

    // Configure the GL settings for all subsequently created canvases.
    QGLFormat fmt;
    fmt.setDepthBufferSize(16);
    fmt.setStencilBufferSize(8);
    fmt.setDoubleBuffer(true);

    if(CommandLine_Exists("-novsync") || !Con_GetByte("vid-vsync"))
    {
        fmt.setSwapInterval(0); // vsync off
        LOG_DEBUG("vsync off");
    }
    else
    {
        fmt.setSwapInterval(1);
        LOG_DEBUG("vsync on");
    }

    // The value of the "vid-fsaa" variable is written to this settings
    // key when the value of the variable changes.
    bool configured = de::App::config().getb("window.fsaa");

    if(CommandLine_Exists("-nofsaa") || !configured)
    {
        fmt.setSampleBuffers(false);
        LOG_DEBUG("multisampling off");
    }
    else
    {
        fmt.setSampleBuffers(true); // multisampling on (default: highest available)
        //fmt.setSamples(4);
        LOG_DEBUG("multisampling on (max)");
    }

    if(fmt != QGLFormat::defaultFormat())
    {
        LOG_DEBUG("Applying new format...");
        QGLFormat::setDefaultFormat(fmt);
        return true;
    }
    else
    {
        LOG_DEBUG("New format is the same as before.");
        return false;
    }
}

void ClientWindow::draw()
{
    // Don't run the main loop until after the paint event has been dealt with.
    ClientApp::app().loop().pause();

    // The canvas needs to be recreated when the GL format has changed
    // (e.g., multisampling).
    if(d->needRecreateCanvas)
    {
        d->needRecreateCanvas = false;
        if(setDefaultGLFormat())
        {
            recreateCanvas();
            // Wait until the new Canvas is ready (note: loop remains paused!).
            return;
        }
    }

    if(shouldRepaintManually())
    {
        DENG_ASSERT_GL_CONTEXT_ACTIVE();

        // Perform the drawing manually right away.
        canvas().updateGL();
    }
    else
    {
        // Request update at the earliest convenience.
        canvas().update();
    }
}

bool ClientWindow::shouldRepaintManually() const
{
    // When the mouse is not trapped, allow the system to regulate window
    // updates (e.g., for window manipulation).
    if(isFullScreen()) return true;
    return !Mouse_IsPresent() || canvas().isMouseTrapped();
}

void ClientWindow::grab(image_t &img, bool halfSized) const
{
    DENG_ASSERT_IN_MAIN_THREAD();

    QSize outputSize = (halfSized? QSize(width()/2, height()/2) : QSize());
    QImage grabbed = canvas().grabImage(outputSize);

    Image_Init(&img);
    img.size.width  = grabbed.width();
    img.size.height = grabbed.height();
    img.pixelSize   = grabbed.depth()/8;
    img.pixels = (uint8_t *) malloc(grabbed.byteCount());
    memcpy(img.pixels, grabbed.constBits(), grabbed.byteCount());

    LOG_DEBUG("Canvas: grabbed %i x %i, byteCount:%i depth:%i format:%i")
            << grabbed.width() << grabbed.height()
            << grabbed.byteCount() << grabbed.depth() << grabbed.format();

    DENG_ASSERT(img.pixelSize != 0);
}

void ClientWindow::updateCanvasFormat()
{
    d->needRecreateCanvas = true;

    // Save the relevant format settings.
    App::config().set("window.fsaa", Con_GetByte("vid-fsaa") != 0);
}

ClientWindow &ClientWindow::main()
{
    return static_cast<ClientWindow &>(PersistentCanvasWindow::main());
}

#if defined(UNIX) && !defined(MACOSX)
void GL_AssertContextActive()
{
    DENG_ASSERT(QGLContext::currentContext() != 0);
}
#endif

#if 0
// --------------------------------------------------------------------------


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
    QRect geometry;

    /// Normal-mode geometry (when not maximized or fullscreen).
    QRect normalGeometry;

    int colorDepthBits;
    int flags;

    Instance(Public *i)
        : Base(i),
          widget(new CanvasWindow),
          drawFunc(0)
    {
        // Minimum possible size when resizing.
        widget->setMinimumSize(QSize(Window::MIN_WIDTH, Window::MIN_HEIGHT));

        widget->setCloseFunc(windowIsClosing);
        widget->setMoveFunc(windowWasMoved);
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
        CommandLine const &cmdLine = DENG2_APP->commandLine();

        if(cmdLine.has("-nofullscreen") || cmdLine.has("-window"))
        {
            setFlag(WF_FULLSCREEN, false);
        }

        if(cmdLine.has("-fullscreen") || cmdLine.has("-nowindow"))
        {
            setFlag(WF_FULLSCREEN);
        }

        if(int arg = cmdLine.check("-width", 1))
        {
            geometry.setWidth(de::max(Window::MIN_WIDTH, cmdLine.at(arg+1).toInt()));
            if(!(flags & WF_FULLSCREEN))
            {
                normalGeometry.setWidth(geometry.width());
            }
        }

        if(int arg = cmdLine.check("-height", 1))
        {
            geometry.setHeight(de::max(Window::MIN_HEIGHT, cmdLine.at(arg+1).toInt()));
            if(!(flags & WF_FULLSCREEN))
            {
                normalGeometry.setHeight(geometry.height());
            }
        }

        if(int arg = cmdLine.check("-winsize", 2))
        {
            geometry.setSize(QSize(de::max(Window::MIN_WIDTH,  cmdLine.at(arg+1).toInt()),
                                   de::max(Window::MIN_HEIGHT, cmdLine.at(arg+2).toInt())));

            if(!(flags & WF_FULLSCREEN))
            {
                normalGeometry.setSize(geometry.size());
            }
        }

        if(int arg = cmdLine.check("-colordepth", 1))
        {
            colorDepthBits = de::clamp(8, cmdLine.at(arg+1).toInt(), 32);
        }
        if(int arg = cmdLine.check("-bpp", 1))
        {
            colorDepthBits = de::clamp(8, cmdLine.at(arg+1).toInt(), 32);
        }

        if(cmdLine.check("-nocenter"))
        {
            setFlag(WF_CENTERED, false);
        }

        if(int arg = cmdLine.check("-xpos", 1))
        {
            normalGeometry.setX(cmdLine.at(arg+1).toInt());
            setFlag(WF_CENTERED | WF_MAXIMIZED, false);
        }

        if(int arg = cmdLine.check("-ypos", 1))
        {
            normalGeometry.setY(cmdLine.at(arg+1).toInt());
            setFlag(WF_CENTERED | WF_MAXIMIZED, false);
        }

        if(cmdLine.check("-center"))
        {
            setFlag(WF_CENTERED);
        }

        if(cmdLine.check("-maximize"))
        {
            setFlag(WF_MAXIMIZED);
        }

        if(cmdLine.check("-nomaximize"))
        {
            setFlag(WF_MAXIMIZED, false);
        }
    }

    bool applyDisplayMode()
    {
        DENG_ASSERT(widget);

        if(!DisplayMode_Count()) return true; // No modes to change to.

        if(flags & WF_FULLSCREEN)
        {
            DisplayMode const *mode =
                DisplayMode_FindClosest(geometry.width(), geometry.height(), colorDepthBits, 0);

            if(mode && DisplayMode_Change(mode, true /* fullscreen: capture */))
            {
                geometry.setSize(QSize(DisplayMode_Current()->width,
                                       DisplayMode_Current()->height));
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
    void applyGeometry()
    {
        LOG_AS("applyGeometry");

        DENG_ASSERT(widget);

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

                // The window is already visible, so let's allow a mode change to
                // resolve itself before we go changing the window.
                LegacyCore_Timer(WAIT_MILLISECS_AFTER_MODE_CHANGE, updateMainWindowLayout);
            }
            else
            {
                LOG_DEBUG("widget is not visible, setting geometry to %s")
                        << Vector2i(DisplayMode_Current()->width,
                                    DisplayMode_Current()->height).asText();

                widget->setGeometry(0, 0, DisplayMode_Current()->width, DisplayMode_Current()->height);
            }
        }
        else
        {
            // The window is in windowed mode (frames and window decoration visible).
            // We will restore it to its previous position and size.
            QRect geom = normalGeometry; // Previously stored normal geometry.

            if(flags & WF_CENTERED)
            {
                // We'll center the window.
                geom = centeredRect(normalGeometry.size());
            }

            if(flags & WF_MAXIMIZED)
            {
                // When a window is maximized, we'll let the native WM handle the sizing.
                if(widget->isVisible())
                {
                    LOG_DEBUG("now maximized.");
                    widget->showMaximized();
                }

                // Non-visible windows will be shown later
                // (as maximized, if the flag is set).
            }
            else
            {
                // The window is in normal mode: not maximized or fullscreen.

                // If the window is already visible, changes to it need to be
                // deferred so that the native counterpart can be updated, too
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
                    // The native window may not be ready to receive the updated
                    // geometry (e.g., window decoration not made visible yet).
                    // We'll apply the geometry after a delay.
                    LegacyCore_Timer(50 + WAIT_MILLISECS_AFTER_MODE_CHANGE, useAppliedGeometryForWindows);
                }
                else
                {
                    // The native window is not visible yet, so we can apply any
                    // number of changes we like.
                    widget->setGeometry(geom);
                }
            }
        }
    }

    /**
     * Retrieves the actual widget geometry and updates the information stored
     * in the Window instance.
     */
    void fetchGeometry()
    {
        DENG_ASSERT(widget);

        setFlag(WF_MAXIMIZED, widget->isMaximized());

        geometry = widget->geometry();

        // If the window is presently maximized or fullscreen, we will not
        // store the actual coordinates.
        if(!widget->isMaximized() && !(flags & WF_FULLSCREEN) && !isBeingAdjusted())
        {
            normalGeometry = widget->geometry();
            DEBUG_Message(("ngw=%i [A]\n", normalGeometry.width()));
        }

        LOG_DEBUG("Current window geometry: %i,%i %s (max:%b)")
                << geometry.x() << geometry.y()
                << Vector2i(geometry.width(), geometry.height()).asText()
                << ((flags & WF_MAXIMIZED) != 0);
        LOG_DEBUG("Normal window geometry: %i,%i %s")
                << normalGeometry.x() << normalGeometry.y()
                << Vector2i(normalGeometry.width(), normalGeometry.height()).asText();
    }

    void setFlag(int flag, bool set = true)
    {
        if(set)
        {
            flags |= flag;

            if(flag & WF_MAXIMIZED)
                LOG_DEBUG("Setting DDWF_MAXIMIZED");
        }
        else
        {
            flags &= ~flag;

            if(flag & WF_CENTERED)
                LOG_DEBUG("Clearing WF_CENTERED");
            if(flag & WF_MAXIMIZED)
                LOG_DEBUG("Clearing DDWF_MAXIMIZED");
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
                if(geometry.x() != attribs[i])
                {
                    normalGeometry.setX(attribs[i]);
                    setFlag(WF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case Window::Y:
                if(geometry.y() != attribs[i])
                {
                    normalGeometry.setY(attribs[i]);
                    setFlag(WF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case Window::Width:
                if(geometry.width() != attribs[i])
                {
                    DENG_ASSERT(attribs[i] >= Window::MIN_WIDTH);
                    geometry.setWidth(attribs[i]);
                    normalGeometry.setWidth(attribs[i]);
                    DEBUG_Message(("ngw=%i [B]\n", normalGeometry.width()));
                    setFlag(WF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case Window::Height:
                if(geometry.height() != attribs[i])
                {
                    DENG_ASSERT(attribs[i] >= Window::MIN_HEIGHT);
                    geometry.setHeight(attribs[i]);
                    normalGeometry.setHeight(attribs[i]);
                    setFlag(WF_MAXIMIZED, false);
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
                    setFlag(WF_MAXIMIZED, false);
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
        applyGeometry();
    }

    void updateLayout()
    {
        setFlag(WF_MAXIMIZED, widget->isMaximized());

        geometry.setSize(widget->size());

        if(!(flags & WF_FULLSCREEN))
        {
            LOG_DEBUG("Updating current view geometry for window, fetched %s")
                    << Vector2i(geometry.width(), geometry.height()).asText();

            if(!(flags & WF_MAXIMIZED) && !isBeingAdjusted())
            {
                // Update the normal-mode geometry (not fullscreen, not maximized).
                normalGeometry.setSize(geometry.size());

                LOG_DEBUG("Updating normal view geometry for window, fetched %s")
                        << Vector2i(normalGeometry.width(), normalGeometry.height()).asText();
            }
        }
        else
        {
            LOG_DEBUG("Updating view geometry for fullscreen %s")
                    << Vector2i(geometry.width(), geometry.height()).asText();
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
            // For some interesting reason, we have to scale the window twice
            // in fullscreen mode or the resulting layout won't be correct.
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
            wnd->d->appliedGeometry = centeredRect(wnd->rect().size());
        }

        LOG_DEBUG("Using applied geometry: %i, %i %s")
            << wnd->d->appliedGeometry.x() << wnd->d->appliedGeometry.y()
            << Vector2i(wnd->d->appliedGeometry.width(), wnd->d->appliedGeometry.height()).asText();
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

    }


    static bool windowIsClosing(CanvasWindow &)
    {
    }

    /**
     * See the todo notes. Duplicating state is not a good idea.
     */
    static void updateWindowStateAfterUserChange()
    {
        Window *wnd = Window::mainPtr();
        if(!wnd || !wnd->d->widget) return;

        wnd->d->fetchGeometry();
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
    wnd->d->applyGeometry();

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

    d->widget->canvas().setFocusFunc(0);

    // Make sure we'll remember the config.
    saveState();

    // As the color transfer table is owned by the window on some platforms
    // (e.g., Windows) we must shutdown DisplayMode first.
    if(this == mainWindow)
    {
        DisplayMode_Shutdown();
    }

    // Delete the CanvasWindow.
    delete d->widget;
}

void Window::updateAfterResize()
{
    DENG_ASSERT(d->widget);
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
            if(attribs[i] < Window::MIN_WIDTH)
                return false;
            break;
        case Window::Height:
            if(attribs[i] < Window::MIN_HEIGHT)
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
            LOG_WARNING("Unknown attribute %i, aborting...") << attribs[i];
            return false;
        }
    }

    // Seems ok.
    return true;
}

bool Window::changeAttributes(int const *attribs)
{
    LOG_AS("Window::changeAttributes");

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
    DENG_ASSERT_IN_MAIN_THREAD();

    if(!d->widget) return;

    // Force a swapbuffers right now.
    d->widget->canvas().swapBuffers();
}

DGLuint Window::grabAsTexture(bool halfSized) const
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT(d->widget);
    return d->widget->canvas().grabAsTexture(halfSized? QSize(d->geometry.width()/2,
                                                              d->geometry.height()/2) : QSize());
}

bool Window::grabToFile(char const *fileName) const
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT(d->widget);
    return d->widget->canvas().grabImage().save(fileName);
}

void Window::setTitle(char const *title) const
{
    DENG_ASSERT_IN_MAIN_THREAD();
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
        DENG_ASSERT_GL_CONTEXT_ACTIVE();

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
    return d->geometry;
}

QRect Window::normalRect() const
{
    return d->normalGeometry;
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

    ArrayValue *array = new ArrayValue;
    *array << NumberValue(d->geometry.left())
           << NumberValue(d->geometry.top())
           << NumberValue(d->geometry.width())
           << NumberValue(d->geometry.height());
    config.names()["window.main.rect"] = array;

    array = new ArrayValue;
    *array << NumberValue(d->normalGeometry.left())
           << NumberValue(d->normalGeometry.top())
           << NumberValue(d->normalGeometry.width())
           << NumberValue(d->normalGeometry.height());
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
        d->geometry.setRect(rect.at(0).asNumber(), rect.at(1).asNumber(),
                            rect.at(2).asNumber(), rect.at(3).asNumber());
    }

    ArrayValue &normalRect = config.geta("window.main.normalRect");
    if(normalRect.size() >= 4)
    {
        d->normalGeometry.setRect(normalRect.at(0).asNumber(), normalRect.at(1).asNumber(),
                                  normalRect.at(2).asNumber(), normalRect.at(3).asNumber());
    }

    d->colorDepthBits = config.geti("window.main.colorDepth");
    d->setFlag(WF_CENTERED,   config.getb("window.main.center"));
    d->setFlag(WF_MAXIMIZED,  config.getb("window.main.maximize"));
    d->setFlag(WF_FULLSCREEN, config.getb("window.main.fullscreen"));
}

void Window::trapMouse(bool enable) const
{
    if(!d->widget || novideo) return;

    DENG_ASSERT(d->widget);
    d->widget->canvas().trapMouse(enable);
}

bool Window::isMouseTrapped() const
{
    DENG_ASSERT(d->widget);
    return d->widget->canvas().isMouseTrapped();
}

bool Window::shouldRepaintManually() const
{
    //return false;

    // When the mouse is not trapped, allow the system to regulate window
    // updates (e.g., for window manipulation).
    if(isFullscreen()) return true;
    return !Mouse_IsPresent() || isMouseTrapped();
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

#endif

