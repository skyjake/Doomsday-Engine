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
#include "ui/clientrootwidget.h"
#include "clientapp.h"
#include <de/DisplayMode>
#include <de/NumberValue>
#include <de/ConstantRule>
#include <QGLFormat>
#include <de/GLState>
#include <de/GLFramebuffer>
#include <de/Drawable>
#include <de/CompositorWidget>
#include <de/NotificationWidget>
#include <de/ProgressWidget>
#include <QCloseEvent>

#include "gl/sys_opengl.h"
#include "gl/gl_main.h"
#include "ui/widgets/gamewidget.h"
#include "ui/widgets/gameuiwidget.h"
#include "ui/widgets/busywidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/consolewidget.h"
#include "ui/widgets/gameselectionwidget.h"
#include "ui/dialogs/coloradjustmentdialog.h"
#include "ui/dialogs/alertdialog.h"
#include "CommandAction"
#include "ui/mouse_qt.h"

#include "dd_main.h"
#include "con_main.h"
#include "ui/vrwindowtransform.h"
#include "render/vr.h"

using namespace de;

DENG2_PIMPL(ClientWindow),
DENG2_OBSERVES(KeyEventSource,   KeyEvent),
DENG2_OBSERVES(MouseEventSource, MouseStateChange),
DENG2_OBSERVES(MouseEventSource, MouseEvent),
DENG2_OBSERVES(Canvas,           FocusChange),
DENG2_OBSERVES(App,              GameChange),
DENG2_OBSERVES(App,              StartupComplete)
{
    bool needMainInit;
    bool needRecreateCanvas;
    bool needRootSizeUpdate;

    Mode mode;

    /// Root of the nomal UI widgets of this window.
    ClientRootWidget root;
    CompositorWidget *compositor;
    GameWidget *game;
    GameUIWidget *gameUI;
    TaskBarWidget *taskBar;
    NotificationWidget *notifications;
    AlertDialog *alerts;
    ColorAdjustmentDialog *colorAdjust;
    LabelWidget *background;
    GameSelectionWidget *gameSelMenu;
    BusyWidget *busy;
    GuiWidget *sidebar;
    LabelWidget *cursor;
    ConstantRule *cursorX;
    ConstantRule *cursorY;
    bool cursorHasBeenHidden;

    // FPS notifications.
    LabelWidget *fpsCounter;
    float oldFps;

    /// @todo Switch dynamically between VR and plain.
    VRWindowTransform contentXf;

    Instance(Public *i)
        : Base(i)
        , needMainInit(true)
        , needRecreateCanvas(false)
        , needRootSizeUpdate(false)
        , mode(Normal)
        , root(thisPublic)
        , compositor(0)
        , game(0)
        , gameUI(0)
        , taskBar(0)
        , notifications(0)
        , alerts(0)
        , colorAdjust(0)
        , background(0)
        , gameSelMenu(0)
        , sidebar(0)
        , cursor(0)
        , cursorX(new ConstantRule(0))
        , cursorY(new ConstantRule(0))
        , cursorHasBeenHidden(false)
        , fpsCounter(0)
        , oldFps(0)
        , contentXf(*i)
    {
        /// @todo The decision whether to receive input notifications from the
        /// canvas is really a concern for the input drivers.

        App::app().audienceForGameChange += this;
        App::app().audienceForStartupComplete += this;

        // Listen to input.
        self.canvas().audienceForKeyEvent += this;
        self.canvas().audienceForMouseStateChange += this;
        self.canvas().audienceForMouseEvent += this;
    }

    ~Instance()
    {
        App::app().audienceForGameChange -= this;
        App::app().audienceForStartupComplete -= this;

        self.canvas().audienceForFocusChange -= this;
        self.canvas().audienceForMouseStateChange -= this;
        self.canvas().audienceForKeyEvent -= this;

        releaseRef(cursorX);
        releaseRef(cursorY);
    }

    Widget &container()
    {
        if(compositor)
        {
            return *compositor;
        }
        return root;
    }

    void setupUI()
    {
        Style &style = ClientApp::windowSystem().style();

        // Background for Ring Zero.
        background = new LabelWidget("background");
        background->setImageColor(Vector4f(0, 0, 0, 1));
        background->setImage(style.images().image("window.background"));
        background->setImageFit(ui::FitToSize);
        background->setSizePolicy(ui::Filled, ui::Filled);
        background->margins().set("");
        background->rule().setRect(root.viewRule());
        root.add(background);

        game = new GameWidget;
        game->rule().setRect(root.viewRule());
        // Initially the widget is disabled. It will be enabled when the window
        // is visible and ready to be drawn.
        game->disable();
        root.add(game);

        gameUI = new GameUIWidget;
        gameUI->rule().setRect(root.viewRule());
        gameUI->disable();
        container().add(gameUI);

        // Busy widget shows progress indicator and frozen game content.
        busy = new BusyWidget;
        busy->hide(); // normally hidden
        busy->rule().setRect(root.viewRule());
        root.add(busy);

        // Game selection.
        gameSelMenu = new GameSelectionWidget;
        gameSelMenu->rule()
                .setInput(Rule::AnchorX, root.viewLeft() + root.viewWidth() / 2)
                .setInput(Rule::AnchorY, root.viewTop() + root.viewHeight() / 2)
                .setInput(Rule::Width,   OperatorRule::minimum(root.viewWidth(),
                                                               style.rules().rule("gameselection.max.width")))
                .setAnchorPoint(Vector2f(.5f, .5f));
        container().add(gameSelMenu);

        // Common notification area.
        notifications = new NotificationWidget;
        notifications->rule()
                .setInput(Rule::Top,   root.viewTop()   + style.rules().rule("gap") - notifications->shift())
                .setInput(Rule::Right, game->rule().right() - style.rules().rule("gap"));
        container().add(notifications);

        // Alerts notification and popup.
        alerts = new AlertDialog;
        root.add(alerts);

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
        container().add(taskBar);

        // The game selection's height depends on the taskbar.
        gameSelMenu->rule().setInput(Rule::Height,
                                     OperatorRule::minimum(root.viewHeight(),
                                                           (taskBar->rule().top() - root.viewHeight() / 2) * 2,                                                     style.rules().rule("gameselection.max.height")));

        // Color adjustment dialog.
        colorAdjust = new ColorAdjustmentDialog;
        colorAdjust->setAnchor(root.viewWidth() / 2, root.viewTop());
        colorAdjust->setOpeningDirection(ui::Down);
        root.add(colorAdjust);

        taskBar->hide();

        // Mouse cursor is used with transformed content.
        cursor = new LabelWidget;
        cursor->setBehavior(Widget::Unhittable);
        cursor->margins().set(""); // no margins
        cursor->setImage(style.images().image("window.cursor"));
        cursor->setAlignment(ui::AlignTopLeft);
        cursor->rule()
                .setSize(Const(48), Const(48))
                .setLeftTop(*cursorX, *cursorY);
        cursor->hide();
        container().add(cursor);
    }

    void appStartupCompleted()
    {
        // Allow the background image to show.
        background->setImageColor(Vector4f(1, 1, 1, 1));
        taskBar->show();
    }

    void currentGameChanged(game::Game const &newGame)
    {
        if(newGame.isNull())
        {
            //game->hide();
            background->show();
            gameSelMenu->show();
        }
        else
        {
            //game->show();
            background->hide();
            gameSelMenu->hide();
        }

        // Check with Style if blurring is allowed.
        taskBar->console().enableBlur(taskBar->style().isBlurringAllowed());
    }

    void setMode(Mode const &newMode)
    {
        LOG_DEBUG("Switching to %s mode") << (newMode == Busy? "Busy" : "Normal");

        // Hide and show widgets as appropriate.
        switch(newMode)
        {
        case Busy:
            game->hide();
            game->disable();
            gameUI->hide();
            gameUI->disable();
            gameSelMenu->hide();
            taskBar->disable();

            busy->show();
            busy->enable();
            break;

        default:
            //busy->hide();
            // The busy widget will hide itself after a possible transition has finished.
            busy->disable();

            game->show();
            game->enable();
            gameUI->show();
            gameUI->enable();
            if(!App_GameLoaded()) gameSelMenu->show();
            taskBar->enable();
            break;
        }

        mode = newMode;
    }

    /**
     * Completes the initialization of the main window. This is called only after the
     * window is created and visible, so that OpenGL operations and actions on the native
     * window can be performed without restrictions.
     */
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

        /// @todo This should be called when a VR mode is actually used.
        contentXf.glInit();
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

        // Translate mouse coordinates for direct interaction.
        if(ev.type() == Event::MousePosition || ev.type() == Event::MouseButton ||
           ev.type() == Event::MouseWheel)
        {
            ev.setPos(contentXf.windowToLogicalCoords(event.pos()).toVector2i());
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
            game->rule()
                    .setInput(Rule::Right,  widget->rule().left());
            gameUI->rule()
                    .setInput(Rule::Right,  widget->rule().left());
            break;
        }

        sidebar = widget;
        container().insertBefore(sidebar, *notifications);
    }

    void uninstallSidebar(SidebarLocation location)
    {
        DENG2_ASSERT(sidebar != NULL);

        switch(location)
        {
        case RightEdge:
            game->rule().setInput(Rule::Right, root.viewRight());
            gameUI->rule().setInput(Rule::Right, root.viewRight());
            break;
        }

        container().remove(*sidebar);
        sidebar->guiDeleteLater();
        sidebar = 0;
    }

    enum DeferredTaskResult {
        Continue,
        AbortFrame
    };

    DeferredTaskResult performDeferredTasks()
    {
        if(BusyMode_Active())
        {
            // Let's not do anything risky in busy mode.
            return Continue;
        }

        // The canvas needs to be recreated when the GL format has changed
        // (e.g., multisampling).
        if(needRecreateCanvas)
        {
            needRecreateCanvas = false;
            if(self.setDefaultGLFormat())
            {
                self.recreateCanvas();
                // Wait until the new Canvas is ready (note: loop remains paused!).
                return AbortFrame;
            }
        }

        return Continue;
    }

    void updateRootSize()
    {
        DENG_ASSERT_IN_MAIN_THREAD();

        needRootSizeUpdate = false;

        Vector2ui const size = contentXf.logicalRootSize(self.canvas().size());

        // Tell the widgets.
        root.setViewSize(size);
    }

    void enableCompositor(bool enable)
    {
        DENG_ASSERT_IN_MAIN_THREAD();

        if((enable && compositor) || (!enable && !compositor))
        {
            return;
        }

        // All the children of the compositor need to be relocated.
        container().remove(*gameUI);
        container().remove(*gameSelMenu);
        if(sidebar) container().remove(*sidebar);
        container().remove(*notifications);
        container().remove(*taskBar);
        container().remove(*cursor);

        Widget::Children additional;

        // Relocate all popups to the new container (which need to stay on top).
        foreach(Widget *w, container().children())
        {
            if(PopupWidget *pop = w->maybeAs<PopupWidget>())
            {
                additional.append(pop);
                container().remove(*pop);
            }
        }

        if(enable && !compositor)
        {
            LOG_GL_VERBOSE("Offscreen UI composition enabled");

            compositor = new CompositorWidget;
            compositor->rule().setRect(root.viewRule());
            root.add(compositor);
        }
        else
        {
            DENG2_ASSERT(compositor != 0);
            DENG2_ASSERT(!compositor->childCount());

            root.remove(*compositor);
            compositor->guiDeleteLater();
            compositor = 0;

            LOG_GL_VERBOSE("Offscreen UI composition disabled");
        }

        container().add(gameUI);

        if(&container() == &root)
        {
            // Make sure the game UI doesn't show up over the busy transition.
            container().moveChildBefore(gameUI, *busy);
        }

        container().add(gameSelMenu);
        if(sidebar) container().add(sidebar);
        container().add(notifications);
        container().add(taskBar);

        // Also the other widgets.
        foreach(Widget *w, additional)
        {
            container().add(w);
        }

        // Fake cursor must be on top.
        container().add(cursor);

        if(mode == Normal)
        {
            root.update();
        }
    }

    void updateCompositor()
    {
        DENG_ASSERT_IN_MAIN_THREAD();

        if(!compositor) return;

        if(vrCfg.mode() == VRConfig::ModeOculusRift)
        {
            compositor->setCompositeProjection(Matrix4f::ortho(-1.1f, 2.2f, -1.1f, 2.2f));
        }
        else
        {
            // We'll simply cover the entire view.
            compositor->useDefaultCompositeProjection();
        }
    }

    void updateMouseCursor()
    {
        // The cursor is only needed if the content is warped.
        cursor->show(!self.canvas().isMouseTrapped() && vrCfg.mode() == VRConfig::ModeOculusRift);

        if(cursor->isVisible())
        {
            if(!cursorHasBeenHidden)
            {
                qApp->setOverrideCursor(QCursor(Qt::BlankCursor));
                cursorHasBeenHidden = true;
            }

            Vector2i cp = ClientApp::windowSystem().latestMousePosition();
            cursorX->set(cp.x);
            cursorY->set(cp.y);
        }
        else
        {
            if(cursorHasBeenHidden)
            {
                qApp->restoreOverrideCursor();
            }
            cursorHasBeenHidden = false;
        }
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

ClientRootWidget &ClientWindow::root()
{
    return d->root;
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

GameWidget &ClientWindow::game()
{
    return *d->game;
}

BusyWidget &ClientWindow::busy()
{
    return *d->busy;
}

AlertDialog &ClientWindow::alerts()
{
    return *d->alerts;
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
    LOG_DEBUG("Window is about to close, executing 'quit'");

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
    LOGDEV_GL_MSG("GL feature: Multisampling: %b") << GL_state.features.multisample;

    PersistentCanvasWindow::canvasGLReady(canvas);

    if(VRConfig::modeNeedsStereoGLFormat(vrCfg.mode()) && !canvas.format().stereo())
    {
        LOG_GL_WARNING("Current VR mode needs a stereo buffer, but it isn't supported");
    }

    // Now that the Canvas is ready for drawing we can enable the GameWidget.
    d->game->enable();
    d->gameUI->enable();

    // Configure a viewport immediately.
    GLState::current().setViewport(Rectangleui(0, 0, canvas.width(), canvas.height())).apply();

    LOG_DEBUG("GameWidget enabled");

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

    // Cursor position (if cursor is visible).
    d->updateMouseCursor();

    if(d->needRootSizeUpdate)
    {
        d->updateRootSize();
    }
    d->updateCompositor();

    d->contentXf.drawTransformed();

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

    GLState::current().setViewport(Rectangleui(0, 0, size.x, size.y));

    d->updateRootSize();
}

bool ClientWindow::setDefaultGLFormat() // static
{
    LOG_AS("DefaultGLFormat");

    // Configure the GL settings for all subsequently created canvases.
    QGLFormat fmt;
    fmt.setProfile(QGLFormat::CompatibilityProfile);
    fmt.setVersion(2, 1);
    fmt.setDepth(false); // depth and stencil handled in GLFramebuffer
    fmt.setStencil(false);
    //fmt.setDepthBufferSize(16);
    //fmt.setStencilBufferSize(8);
    fmt.setDoubleBuffer(true);

    if(VRConfig::modeNeedsStereoGLFormat(vrCfg.mode()))
    {
        // Only use a stereo format for modes that require it.
        LOG_GL_MSG("Using a stereoscopic frame buffer format");
        fmt.setStereo(true);
    }

    if(CommandLine_Exists("-novsync") || !Con_GetByte("vid-vsync"))
    {
        fmt.setSwapInterval(0); // vsync off
        LOG_GL_VERBOSE("Vertical sync off");
    }
    else
    {
        fmt.setSwapInterval(1);
        LOG_GL_VERBOSE("Vertical sync on");
    }

    // The value of the "vid-fsaa" variable is written to this settings
    // key when the value of the variable changes.
    int sampleCount = 1;
    bool configured = de::App::config().getb("window.fsaa");
    if(CommandLine_Exists("-nofsaa") || !configured)
    {
        LOG_GL_VERBOSE("Multisampling off");
    }
    else
    {
        sampleCount = 4; // four samples is fine?
        LOG_GL_VERBOSE("Multisampling on (%i samples)") << sampleCount;
    }
    GLFramebuffer::setDefaultMultisampling(sampleCount);

    if(fmt != QGLFormat::defaultFormat())
    {
        LOG_GL_VERBOSE("Applying new format...");
        QGLFormat::setDefaultFormat(fmt);
        return true;
    }
    else
    {
        LOG_GL_XVERBOSE("New format is the same as before");
        return false;
    }
}

void ClientWindow::draw()
{
    // Don't run the main loop until after the paint event has been dealt with.
    ClientApp::app().loop().pause();

    // Offscreen composition is only needed in Oculus Rift mode.
    d->enableCompositor(vrCfg.mode() == VRConfig::ModeOculusRift);

    if(d->performDeferredTasks() == Instance::AbortFrame)
    {
        // Shouldn't draw right now.
        return;
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

    Image_Init(img);
    img.size      = Vector2ui(grabbed.width(), grabbed.height());
    img.pixelSize = grabbed.depth()/8;

    img.pixels = (uint8_t *) malloc(grabbed.byteCount());
    std::memcpy(img.pixels, grabbed.constBits(), grabbed.byteCount());

    LOGDEV_GL_MSG("Grabbed Canvas contents %i x %i, byteCount:%i depth:%i format:%i")
            << grabbed.width() << grabbed.height()
            << grabbed.byteCount() << grabbed.depth() << grabbed.format();

    DENG_ASSERT(img.pixelSize != 0);
}

void ClientWindow::drawGameContent()
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    GLState::current().target().clear(GLTarget::ColorDepthStencil);

    d->root.drawUntil(*d->gameSelMenu);
}

void ClientWindow::updateCanvasFormat()
{
    d->needRecreateCanvas = true;

    // Save the relevant format settings.
    App::config().set("window.fsaa", Con_GetByte("vid-fsaa") != 0);
}

void ClientWindow::updateRootSize()
{
    // This will be done a bit later as the call may originate from another thread.
    d->needRootSizeUpdate = true;
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

void ClientWindow::addOnTop(GuiWidget *widget)
{
    d->container().add(widget);

    // Make sure the cursor remains the topmost widget.
    d->container().moveChildToLast(*d->cursor);
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
