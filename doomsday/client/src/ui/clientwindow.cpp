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
#include <de/GLState>
#include <QCloseEvent>

#include "gl/sys_opengl.h"
#include "gl/gl_main.h"
#include "ui/widgets/legacywidget.h"
#include "ui/widgets/busywidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/consolewidget.h"
#include "ui/widgets/notificationwidget.h"
#include "ui/widgets/gameselectionwidget.h"
#include "ui/widgets/progresswidget.h"
#include "ui/dialogs/coloradjustmentdialog.h"
#include "CommandAction"
#include "ui/mouse_qt.h"

#include "dd_main.h"
#include "con_main.h"
#include "render/vr.h"

using namespace de;

static String const LEGACY_WIDGET_NAME = "legacy";

DENG2_PIMPL(ClientWindow),
DENG2_OBSERVES(KeyEventSource,   KeyEvent),
DENG2_OBSERVES(MouseEventSource, MouseStateChange),
DENG2_OBSERVES(MouseEventSource, MouseEvent),
DENG2_OBSERVES(Canvas,           FocusChange),
DENG2_OBSERVES(App,              GameChange)
{
    bool needMainInit;
    bool needRecreateCanvas;

    Mode mode;

    /// Root of the nomal UI widgets of this window.
    GuiRootWidget root;
    LegacyWidget *legacy;
    TaskBarWidget *taskBar;
    NotificationWidget *notifications;
    ColorAdjustmentDialog *colorAdjust;
    LabelWidget *background;
    GameSelectionWidget *games;
    BusyWidget *busy;
    GuiWidget *sidebar;

    GuiRootWidget busyRoot;

    // FPS notifications.
    LabelWidget *fpsCounter;
    float oldFps;

    Instance(Public *i)
        : Base(i),
          needMainInit(true),
          needRecreateCanvas(false),
          mode(Normal),
          root(thisPublic),
          legacy(0),
          taskBar(0),
          notifications(0),
          colorAdjust(0),
          background(0),
          games(0),
          sidebar(0),
          busyRoot(thisPublic),
          fpsCounter(0),
          oldFps(0)
    {
        /// @todo The decision whether to receive input notifications from the
        /// canvas is really a concern for the input drivers.

        App::app().audienceForGameChange += this;

        // Listen to input.
        self.canvas().audienceForKeyEvent += this;
        self.canvas().audienceForMouseStateChange += this;
        self.canvas().audienceForMouseEvent += this;
    }

    ~Instance()
    {
        App::app().audienceForGameChange -= this;

        self.canvas().audienceForFocusChange -= this;
        self.canvas().audienceForMouseStateChange -= this;
        self.canvas().audienceForKeyEvent -= this;
    }

    void setupUI()
    {
        Style &style = ClientApp::windowSystem().style();

        // Background for Ring Zero.
        background = new LabelWidget;
        background->setImage(style.images().image("window.background"));
        background->setImageFit(ui::FitToSize);
        background->setSizePolicy(ui::Filled, ui::Filled);
        background->margins().set("");
        background->rule()
                .setInput(Rule::Left,   root.viewLeft())
                .setInput(Rule::Top,    root.viewTop())
                .setInput(Rule::Right,  root.viewRight())
                .setInput(Rule::Bottom, root.viewBottom());
        root.add(background);

        legacy = new LegacyWidget(LEGACY_WIDGET_NAME);
        legacy->rule()
                .setLeftTop    (root.viewLeft(),  root.viewTop())
                .setRightBottom(root.viewWidth(), root.viewBottom());
        // Initially the widget is disabled. It will be enabled when the window
        // is visible and ready to be drawn.
        legacy->disable();
        root.add(legacy);

        // Game selection.
        games = new GameSelectionWidget;
        games->rule()
                .setInput(Rule::AnchorX, root.viewLeft() + root.viewWidth() / 2)
                .setInput(Rule::AnchorY, root.viewTop() + root.viewHeight() / 2)
                .setInput(Rule::Width,   OperatorRule::minimum(root.viewWidth(),
                                                               style.rules().rule("gameselection.max.width")))
                .setAnchorPoint(Vector2f(.5f, .5f));
        root.add(games);

        // Common notification area.
        notifications = new NotificationWidget;
        notifications->rule()
                .setInput(Rule::Top,   root.viewTop()   + style.rules().rule("gap") - notifications->shift())
                .setInput(Rule::Right, legacy->rule().right() - style.rules().rule("gap"));
        root.add(notifications);

        // FPS counter for the notification area.
        fpsCounter = new LabelWidget;
        fpsCounter->setSizePolicy(ui::Expand, ui::Expand);
        fpsCounter->setAlignment(ui::AlignRight);

        // Taskbar is over almost everything else.
        taskBar = new TaskBarWidget;
        taskBar->rule()
                .setInput(Rule::Left,   root.viewLeft())
                .setInput(Rule::Bottom, root.viewBottom() + taskBar->shift())
                .setInput(Rule::Width,  root.viewWidth());
        root.add(taskBar);

        // The game selection's height depends on the taskbar.
        games->rule().setInput(Rule::Height,
                               OperatorRule::minimum(root.viewHeight(),
                                                     (taskBar->rule().top() - root.viewHeight() / 2) * 2,
                                                     style.rules().rule("gameselection.max.height")));

        // Color adjustment dialog.
        colorAdjust = new ColorAdjustmentDialog;
        colorAdjust->setAnchor(root.viewWidth() / 2, root.viewTop());
        colorAdjust->setOpeningDirection(ui::Down);
        root.add(colorAdjust);

        // For busy mode we have an entirely different widget tree.
        busy = new BusyWidget;
        busy->rule()
                .setLeftTop    (busyRoot.viewLeft(),  busyRoot.viewTop())
                .setRightBottom(busyRoot.viewRight(), busyRoot.viewBottom());
        busyRoot.add(busy);
    }

    void currentGameChanged(game::Game const &newGame)
    {
        if(newGame.isNull())
        {
            //legacy->hide();
            background->show();
            games->show();
            taskBar->console().enableBlur();
        }
        else
        {
            //legacy->show();
            background->hide();
            games->hide();

            // For the time being, blurring is not compatible with the
            // legacy OpenGL rendering.
            taskBar->console().enableBlur(false);
        }
    }

    void setMode(Mode const &newMode)
    {
        LOG_DEBUG("Switching to %s mode") << (newMode == Busy? "Busy" : "Normal");

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

        /*
        // Automatically grab the mouse from the get-go if in fullscreen mode.
        if(Mouse_IsPresent() && self.isFullScreen())
        {
            self.canvas().trapMouse();
        }
        */

        self.canvas().audienceForFocusChange += this;

#ifdef WIN32
        if(self.isFullScreen())
        {
            // It would seem we must manually give our canvas focus. Bug in Qt?
            self.canvas().setFocus();
        }
#endif

        self.canvas().makeCurrent();

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

    void mouseEvent(MouseEvent const &event)
    {
        MouseEvent ev = event;

        switch(VR::mode)
        {
        // Left-right screen split modes
        case VR::MODE_SIDE_BY_SIDE:
        case VR::MODE_CROSSEYE:
        case VR::MODE_PARALLEL:
        case VR::MODE_OCULUS_RIFT:
            if(ev.type() == Event::MousePosition || ev.type() == Event::MouseButton)
            {
                // We need to map the real window coordinates to logical
                // root view coordinates.
                Vector2f pos = ev.pos();

                // Make it possible to access both frames.
                if(pos.x >= self.width()/2)
                {
                    pos.x -= self.width()/2;
                }
                pos.x *= 2;

                // Scale to logical size.
                pos = pos / Vector2f(self.size()) *
                        Vector2f(root.viewWidth().value(), root.viewHeight().value());

                ev.setPos(pos.toVector2i());
            }
            break;
        // Top-bottom screen split modes
        case VR::MODE_TOP_BOTTOM:
            if(ev.type() == Event::MousePosition || ev.type() == Event::MouseButton)
            {
                // We need to map the real window coordinates to logical
                // root view coordinates.
                Vector2f pos = ev.pos();

                // Make it possible to access both frames.
                if(pos.y >= self.height()/2)
                {
                    pos.y -= self.height()/2;
                }
                pos.y *= 2;

                // Scale to logical size.
                pos = pos / Vector2f(self.size()) *
                        Vector2f(root.viewWidth().value(), root.viewHeight().value());

                ev.setPos(pos.toVector2i());
            }
            break;
        default:
            break;
        }

        if(ClientApp::windowSystem().processEvent(ev))
        {
            // Eaten by the window system.
            return;
        }

        // Fall back to legacy handling.
        switch(ev.type())
        {
        case Event::MouseButton:
            Mouse_Qt_SubmitButton(
                        ev.button() == MouseEvent::Left?     IMB_LEFT :
                        ev.button() == MouseEvent::Middle?   IMB_MIDDLE :
                        ev.button() == MouseEvent::Right?    IMB_RIGHT :
                        ev.button() == MouseEvent::XButton1? IMB_EXTRA1 :
                        ev.button() == MouseEvent::XButton2? IMB_EXTRA2 : IMB_MAXBUTTONS,
                        ev.state() == MouseEvent::Pressed);
            break;

        case Event::MouseMotion:
            Mouse_Qt_SubmitMotion(IMA_POINTER, ev.pos().x, ev.pos().y);
            break;

        case Event::MouseWheel:
            Mouse_Qt_SubmitMotion(IMA_WHEEL, ev.pos().x, ev.pos().y);
            break;

        default:
            break;
        }
    }

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
        else if(self.isFullScreen() && !taskBar->isOpen())
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

    void updateFpsNotification(float fps)
    {       
        notifications->showOrHide(fpsCounter, self.isFPSCounterVisible());

        if(!fequal(oldFps, fps))
        {
            fpsCounter->setText(QString("%1 " _E(l) + tr("FPS")).arg(fps, 0, 'f', 1));
            oldFps = fps;
        }
    }

    void installSidebar(SidebarLocation location, GuiWidget *widget)
    {
        // Get rid of the old sidebar.
        if(sidebar)
        {
            uninstallSidebar(location);
        }
        if(!widget) return;

        DENG2_ASSERT(sidebar == NULL);

        // Attach the widget.
        switch(location)
        {
        case RightEdge:
            widget->rule()
                    .setInput(Rule::Top,    root.viewTop())
                    .setInput(Rule::Right,  root.viewRight())
                    .setInput(Rule::Bottom, taskBar->rule().top());
            legacy->rule()
                    .setInput(Rule::Right,  widget->rule().left());
            break;
        }

        sidebar = widget;
        root.insertBefore(sidebar, *notifications);
    }

    void uninstallSidebar(SidebarLocation location)
    {
        DENG2_ASSERT(sidebar != NULL);

        switch(location)
        {
        case RightEdge:
            legacy->rule().setInput(Rule::Right, root.viewRight());
            break;
        }

        root.remove(*sidebar);
        sidebar->deleteLater();
        sidebar = 0;
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

    d->setupUI();
}

GuiRootWidget &ClientWindow::root()
{
    return d->mode == Busy? d->busyRoot : d->root;
}

TaskBarWidget &ClientWindow::taskBar()
{
    return *d->taskBar;
}

ConsoleWidget &ClientWindow::console()
{
    return d->taskBar->console();
}

NotificationWidget &ClientWindow::notifications()
{
    return *d->notifications;
}

LegacyWidget &ClientWindow::game()
{
    return *d->legacy;
}

BusyWidget &ClientWindow::busy()
{
    return *d->busy;
}

bool ClientWindow::isFPSCounterVisible() const
{
    return App::config().getb(configName("showFps"));
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

    // Configure a viewport immediately.
    GLState::top().setViewport(Rectangleui(0, 0, canvas.width(), canvas.height())).apply();

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

    switch(VR::mode)
    {
    // A) Single view type stereo 3D modes here:
    case VR::MODE_MONO:
        // Non-stereoscopic frame.
        root().draw();
        break;
    case VR::MODE_LEFT:
        // Left eye view
        VR::eyeShift = VR::getEyeShift(-1);
        root().draw();
        break;
    case VR::MODE_RIGHT:
        // Right eye view
        VR::eyeShift = VR::getEyeShift(+1);
        root().draw();
        break;
    // B) Split-screen type stereo 3D modes here:
    case VR::MODE_TOP_BOTTOM: // TODO - which is on top?
        // Left eye view on top of screen.
        VR::eyeShift = VR::getEyeShift(-1);
        GLState::setActiveRect(Rectangleui(0, 0, width(), height()/2), true);
        root().draw();
        // Right eye view on bottom of screen.
        VR::eyeShift = VR::getEyeShift(+1);
        GLState::setActiveRect(Rectangleui(0, height()/2, width(), height()/2), true);
        root().draw();
        break;
    case VR::MODE_SIDE_BY_SIDE: // Squished aspect
        // Left eye view on left side of screen.
        VR::eyeShift = VR::getEyeShift(-1);
        GLState::setActiveRect(Rectangleui(0, 0, width()/2, height()), true);
        root().draw();
        // Right eye view on right side of screen.
        VR::eyeShift = VR::getEyeShift(+1);
        GLState::setActiveRect(Rectangleui(width()/2, 0, width()/2, height()), true);
        root().draw();
        break;
    case VR::MODE_PARALLEL: // TODO Normal aspect
        // Left eye view on left side of screen.
        VR::eyeShift = VR::getEyeShift(-1);
        GLState::setActiveRect(Rectangleui(0, 0, width()/2, height()), true);
        root().draw();
        // Right eye view on right side of screen.
        VR::eyeShift = VR::getEyeShift(+1);
        GLState::setActiveRect(Rectangleui(width()/2, 0, width()/2, height()), true);
        root().draw();
        break;
    case VR::MODE_CROSSEYE: // TODO Normal aspect
        // RIght eye view on left side of screen.
        VR::eyeShift = VR::getEyeShift(+1);
        GLState::setActiveRect(Rectangleui(0, 0, width()/2, height()), true);
        root().draw();
        // Left eye view on right side of screen.
        VR::eyeShift = VR::getEyeShift(-1);
        GLState::setActiveRect(Rectangleui(width()/2, 0, width()/2, height()), true);
        root().draw();
        break;
    case VR::MODE_OCULUS_RIFT: // TODO Normal aspect, head tracking, image warping
        // Left eye view on left side of screen.
        VR::eyeShift = VR::getEyeShift(-1);
        GLState::setActiveRect(Rectangleui(0, 0, width()/2, height()), true);
        root().draw();
        // Right eye view on right side of screen.
        VR::eyeShift = VR::getEyeShift(+1);
        GLState::setActiveRect(Rectangleui(width()/2, 0, width()/2, height()), true);
        root().draw();
        break;
    // Overlaid type stereo 3D modes below:
    case VR::MODE_GREEN_MAGENTA:
        // Left eye view
        VR::eyeShift = VR::getEyeShift(-1);
        // save previous glColorMask
        glPushAttrib(GL_COLOR_BUFFER_BIT);
        glColorMask(0, 1, 0, 1); // Left eye view green
        root().draw();
        // Right eye view
        VR::eyeShift = VR::getEyeShift(+1);
        glColorMask(1, 0, 1, 1); // Right eye view magenta
        root().draw();
        glPopAttrib(); // restore glColorMask
        break;
    case VR::MODE_RED_CYAN:
        // Left eye view
        VR::eyeShift = VR::getEyeShift(-1);
        // save previous glColorMask
        glPushAttrib(GL_COLOR_BUFFER_BIT);
        glColorMask(1, 0, 0, 1); // Left eye view red
        root().draw();
        // Right eye view
        VR::eyeShift = VR::getEyeShift(+1);
        glColorMask(0, 1, 1, 1); // Right eye view cyan
        root().draw();
        glPopAttrib(); // restore glColorMask
        break;
    case VR::MODE_QUAD_BUFFERED:
    {
        // TODO - attempt to enable stereo context at start up.
        GLboolean isStereoContext, isDoubleBuffered;
        glGetBooleanv(GL_STEREO, &isStereoContext);
        glGetBooleanv(GL_DOUBLEBUFFER, &isDoubleBuffered);
        if (isStereoContext) {
            // Left eye view
            VR::eyeShift = VR::getEyeShift(-1);
            if (isDoubleBuffered)
                glDrawBuffer(GL_BACK_LEFT);
            else
                glDrawBuffer(GL_FRONT_LEFT);
            root().draw();
            // Right eye view
            VR::eyeShift = VR::getEyeShift(+1);
            if (isDoubleBuffered)
                glDrawBuffer(GL_BACK_RIGHT);
            else
                glDrawBuffer(GL_FRONT_RIGHT);
            root().draw();
            if (isDoubleBuffered)
                glDrawBuffer(GL_BACK);
            else
                glDrawBuffer(GL_FRONT);
            break;
        }
        else {
            // Non-stereoscopic frame.
            root().draw();
            break;
        }
    }
    case VR::MODE_ROW_INTERLEAVED:
    {
        // Use absolute screen position of window to determine whether the
        // first scan line is odd or even.
        QPoint ulCorner(0, 0);
        ulCorner = canvas.mapToGlobal(ulCorner); // widget to screen coordinates
        bool rowParityIsEven = ((ulCorner.x() % 2) == 0);
        // TODO - use parity in shader or stencil...
        // Left eye view
        VR::eyeShift = VR::getEyeShift(-1);
        root().draw();
        // Right eye view
        VR::eyeShift = VR::getEyeShift(+1);
        root().draw();
        break;
    }
    case VR::MODE_COLUMN_INTERLEAVED: // TODO
    case VR::MODE_CHECKERBOARD: // TODO
    default:
        // Non-stereoscopic frame.
        root().draw();
        break;
    }

    // Restore default VR dynamic parameters
    GLState::setActiveRect(Rectangleui(), true);
    VR::eyeShift = 0;

    // Finish GL drawing and swap it on to the screen. Blocks until buffers
    // swapped.
    GL_DoUpdate();

    ClientApp::app().postFrame(); /// @todo what about multiwindow?

    PersistentCanvasWindow::canvasGLDraw(canvas);
    d->updateFpsNotification(frameRate());
}

void ClientWindow::canvasGLResized(Canvas &canvas)
{
    LOG_AS("ClientWindow");

    Canvas::Size size = canvas.size();
    LOG_TRACE("Canvas resized to ") << size.asText();

    GLState::top().setViewport(Rectangleui(0, 0, size.x, size.y));

    updateRootSize();
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

void ClientWindow::updateRootSize()
{
    Canvas::Size size = canvas().size();

    switch(VR::mode)
    {
    // Left-right screen split modes
    case VR::MODE_CROSSEYE:
    case VR::MODE_PARALLEL:
        // Adjust effective UI size for stereoscopic rendering.
        size.y *= 2;
        size *= .75f; // Make it a bit bigger.
        break;
    case VR::MODE_OCULUS_RIFT:
        // TODO - taskbar needs to elevate above bottom of screen
        // Adjust effective UI size for stereoscopic rendering.
        size.y *= 2;
        size *= 0.75f;
        break;
    // Allow UI to squish in top/bottom and SBS mode: 3D hardware will unsquish them
    case VR::MODE_TOP_BOTTOM:
    case VR::MODE_SIDE_BY_SIDE:
    default:
        break;
    }

    // Tell the widgets.
    d->root.setViewSize(size);
    d->busyRoot.setViewSize(size);
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

void ClientWindow::toggleFPSCounter()
{
    App::config().set(configName("showFps"), !isFPSCounterVisible());
}

void ClientWindow::showColorAdjustments()
{
    d->colorAdjust->open();
}

void ClientWindow::setSidebar(SidebarLocation location, GuiWidget *sidebar)
{
    DENG2_ASSERT(location == RightEdge);

    d->installSidebar(location, sidebar);
}

bool ClientWindow::hasSidebar(SidebarLocation location) const
{
    DENG2_ASSERT(location == RightEdge);

    return d->sidebar != 0;
}
