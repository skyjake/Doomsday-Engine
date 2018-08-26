/** @file clientwindow.cpp  Top-level window with UI widgets.
 * @ingroup base
 *
 * @todo Platform-specific behavior should be encapsulated in subclasses, e.g.,
 * MacWindowBehavior. This would make the code easier to follow and more adaptable
 * to the quirks of each platform.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/viewcompositor.h"
#include "clientapp.h"
#include "clientplayer.h"

//#include <QSurfaceFormat>
//#include <QTimer>
//#include <QCloseEvent>
#include <de/CompositorWidget>
#include <de/Config>
#include <de/ConstantRule>
#include <de/DisplayMode>
#include <de/Drawable>
#include <de/FadeToBlackWidget>
#include <de/FileSystem>
#include <de/GLTextureFramebuffer>
#include <de/GLState>
#include <de/GLInfo>
#include <de/LogBuffer>
#include <de/NotificationAreaWidget>
#include <de/NumberValue>
//#include <de/SignalAction>
#include <de/VRWindowTransform>
#include <de/concurrency.h>
#include <doomsday/console/exec.h>
#include "api_console.h"

#include "gl/sys_opengl.h"
#include "gl/gl_main.h"
#include "ui/widgets/gamewidget.h"
#include "ui/widgets/busywidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/consolewidget.h"
#include "ui/widgets/privilegedlogwidget.h"
#include "ui/home/homewidget.h"
#include "ui/dialogs/coloradjustmentdialog.h"
#include "ui/dialogs/alertdialog.h"
#include "ui/inputdevice.h"
#include "ui/inputsystem.h"
#include "ui/clientwindowsystem.h"
#include "CommandAction"
#include "ui/mouse_qt.h"
#include "dd_main.h"
#include "render/vr.h"

using namespace de;

DE_PIMPL(ClientWindow)
, DE_OBSERVES(App, StartupComplete)
, DE_OBSERVES(DoomsdayApp, GameChange)
, DE_OBSERVES(MouseEventSource, MouseStateChange)
, DE_OBSERVES(WindowEventHandler, FocusChange)
, DE_OBSERVES(GLWindow, Init)
, DE_OBSERVES(GLWindow, Resize)
, DE_OBSERVES(GLWindow, Swap)
, DE_OBSERVES(Variable, Change)
, DENG2_OBSERVES(FileSystem, Busy)
#if !defined (DE_MOBILE)
, DE_OBSERVES(PersistentGLWindow, AttributeChange)
#endif
{
    bool uiSetupDone        = false;
    bool needMainInit       = true;
    bool needRootSizeUpdate = false;
    Mode mode               = Normal;

    /// Root of the nomal UI widgets of this window.
    ClientRootWidget root;
    GameWidget *game = nullptr;
    LabelWidget *nowPlaying = nullptr;
    TaskBarWidget *taskBar = nullptr;
    LabelWidget *taskBarBlur = nullptr; ///< Blur everything below the task bar.
    NotificationAreaWidget *notifications = nullptr;
    ButtonWidget *quitButton = nullptr;
    AlertDialog *alerts = nullptr;
    ColorAdjustmentDialog *colorAdjust = nullptr;
    HomeWidget *home = nullptr;
    SafeWidgetPtr<FadeToBlackWidget> fader;
    BusyWidget *busy = nullptr;
    GuiWidget *sidebar = nullptr;
    PrivilegedLogWidget *privLog = nullptr;
    LabelWidget *cursor = nullptr;
    ConstantRule *cursorX;
    ConstantRule *cursorY;
    AnimationRule *gameWidth;
    AnimationRule *gameHeight;
    AnimationRule *homeDelta;
    AnimationRule *quitX;
    bool cursorHasBeenHidden = false;
    bool isGameMini = false;

    // FPS notifications.
    UniqueWidgetPtr<LabelWidget> fpsCounter;
    float oldFps = 0;

    // File system notification.
    UniqueWidgetPtr<ProgressWidget> fsBusy;

    /// @todo Switch dynamically between VR and plain.
    VRWindowTransform contentXf;

    Impl(Public *i)
        : Base      (i)
        , root      (i)
        , cursorX   (new ConstantRule(0))
        , cursorY   (new ConstantRule(0))
        , gameWidth (new AnimationRule(0, Animation::EaseBoth))
        , gameHeight(new AnimationRule(0, Animation::EaseBoth))
        , homeDelta (new AnimationRule(0, Animation::EaseBoth))
        , quitX     (new AnimationRule(0, Animation::EaseBoth))
        , contentXf (*i)
    {
        self().setTransform(contentXf);

        /// @todo The decision whether to receive input notifications from the
        /// canvas is really a concern for the input drivers.

        DoomsdayApp::app().audienceForGameChange() += this;
        App::app().audienceForStartupComplete() += this;

        // Listen to input.
        self().eventHandler().audienceForMouseStateChange() += this;

        for (const String &s : configVariableNames())
        {
            App::config(s).audienceForChange() += this;
        }
    }

    ~Impl() override
    {
        releaseRef(cursorX);
        releaseRef(cursorY);
        releaseRef(gameWidth);
        releaseRef(gameHeight);
        releaseRef(homeDelta);
        releaseRef(quitX);
    }

    StringList configVariableNames() const
    {
#if !defined (DE_MOBILE)
        return {self().configName("fsaa"), self().configName("vsync")};
#else
        return StringList();
#endif
    }

    void setupUI()
    {
        Style &style = ClientApp::windowSystem().style();

        // This is a shared widget so it may be accessed during initialization of other widgets;
        // create it first.
        taskBarBlur = new LabelWidget("taskbar-blur");

        gameWidth ->set(root.viewWidth());
        gameHeight->set(root.viewHeight());

        game = new GameWidget;
        game->rule().setInput(Rule::Left,   root.viewLeft())
                    .setInput(Rule::Right,  root.viewLeft() + *gameWidth)
                    .setInput(Rule::Bottom, root.viewBottom())
                    .setInput(Rule::Height, *gameHeight);
        // Initially the widget is disabled. It will be enabled when the window
        // is visible and ready to be drawn.
        game->disable();
        root.add(game);

        /*gameUI = new GameUIWidget;
        gameUI->rule().setRect(game->rule());
        gameUI->disable();
        root.add(gameUI);*/

        auto *miniGameControls = new LabelWidget;
        {
            miniGameControls->set(GuiWidget::Background(style.colors().colorf("background")));
            miniGameControls->rule()
                    .setInput(Rule::Width,  root.viewWidth()/2 -   style.rules().rule(RuleBank::UNIT))
                    .setInput(Rule::Left,   game->rule().right() + style.rules().rule(RuleBank::UNIT))
                    .setInput(Rule::Top,    game->rule().top());

            ButtonWidget *backToGame = new ButtonWidget;
            backToGame->setText("Back to Game");
            backToGame->setSizePolicy(ui::Expand, ui::Expand);
            backToGame->setActionFn([this] () { home->moveOffscreen(1.0); });
            backToGame->setColorTheme(GuiWidget::Inverted);

            nowPlaying = new LabelWidget;
            nowPlaying->setSizePolicy(ui::Expand, ui::Expand);
            nowPlaying->margins()/*.setLeftRight("")*/.setBottom("dialog.gap");
            nowPlaying->setMaximumTextWidth(miniGameControls->rule().width());
            nowPlaying->setTextLineAlignment(ui::AlignLeft);
            nowPlaying->setTextColor("text");
            nowPlaying->setFont("heading");

            AutoRef<Rule> combined = nowPlaying->rule().height() + backToGame->rule().height();

            nowPlaying->rule()
                    .setMidAnchorX(miniGameControls->rule().midX())
                    .setInput(Rule::Top, miniGameControls->rule().midY() - combined/2);
            backToGame->rule()
                    .setInput(Rule::Left, nowPlaying->rule().left())
                    .setInput(Rule::Top,  nowPlaying->rule().bottom());

            miniGameControls->add(nowPlaying);
            miniGameControls->add(backToGame);
            root.add(miniGameControls);
        }

        // Busy widget shows progress indicator and frozen game content.
        busy = new BusyWidget;
        busy->setGameWidget(*game);
        busy->hide(); // normally hidden
        busy->rule().setRect(root.viewRule());
        root.add(busy);

        home = new HomeWidget;
        home->rule().setInput(Rule::Left,   root.viewLeft())
                    .setInput(Rule::Right,  root.viewRight())
                    .setInput(Rule::Top,    root.viewTop())
                    .setInput(Rule::Bottom, root.viewBottom() + *homeDelta);
        root.add(home);

#if !defined (DE_MOBILE)
        // Busy progress should be visible over the Home.
        busy->progress().orphan();
        busy->progress().rule().setRect(game->rule());
        root.add(&busy->progress());
#endif

        // Common notification area.
        notifications = new NotificationAreaWidget;
        notifications->useDefaultPlacement(root.viewRule(), *quitX);
        root.add(notifications);

        // Quit shortcut.
        quitButton = new ButtonWidget;
        quitButton->setBehavior(Widget::Focusable, false);
        quitButton->setText(_E(b)_E(D) "QUIT");
        quitButton->setFont("small");
        quitButton->setSizePolicy(ui::Expand, ui::Fixed);
        quitButton->setStyleImage("close.ringless", "default");
        quitButton->setImageColor(style.colors().colorf("accent"));
        quitButton->setTextAlignment(ui::AlignLeft);
        quitButton->set(GuiWidget::Background(style.colors().colorf("background")));
        quitButton->setActionFn([]() { Con_Execute(CMDS_DDAY, "quit!", false, false); });
        quitButton->rule()
                .setInput(Rule::Top,    root.viewTop() + style.rules().rule("gap"))
                .setInput(Rule::Left,   root.viewRight() + *quitX)
                .setInput(Rule::Height, style.fonts().font("default").height() +
                          style.rules().rule("gap") * 2);
        quitButton->setOpacity(0);
        root.add(quitButton);

        // Alerts notification and popup.
        alerts = new AlertDialog;
        root.add(alerts);

        // FPS counter for the notification area.
        fpsCounter.reset(new LabelWidget);
        fpsCounter->setSizePolicy(ui::Expand, ui::Expand);
        fpsCounter->setAlignment(ui::AlignRight);

        // File system busy indicator.
        fsBusy.reset(new ProgressWidget);
        fsBusy->setSizePolicy(ui::Expand, ui::Expand);
        fsBusy->setMode(ProgressWidget::Indefinite);
        fsBusy->useMiniStyle();
        fsBusy->setText("Files");
        fsBusy->setTextAlignment(ui::AlignRight);

        // Everything behind the task bar can be blurred with this widget.
        if (style.isBlurringAllowed())
        {
            taskBarBlur->set(GuiWidget::Background(Vec4f(1, 1, 1, 1), GuiWidget::Background::Blurred));
        }
        taskBarBlur->rule().setRect(root.viewRule());
        taskBarBlur->setAttribute(GuiWidget::DontDrawContent);
        root.add(taskBarBlur);

        // Taskbar is over almost everything else.
        taskBar = new TaskBarWidget;
        taskBar->rule()
                .setInput(Rule::Left,   root.viewLeft())
                .setInput(Rule::Bottom, root.viewBottom() + taskBar->shift())
                .setInput(Rule::Width,  root.viewWidth());
        root.add(taskBar);

        miniGameControls->rule().setInput(Rule::Bottom, taskBar->rule().top());

        // Privileged work-in-progress log.
        privLog = new PrivilegedLogWidget;
        privLog->rule().setRect(root.viewRule());
        root.add(privLog);

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
        root.add(cursor);

        /*
#ifdef DE_DEBUG
        auto debugPrint = [] (Widget &w)
        {
            int depth = 0;
            for (auto *p = w.parent(); p; p = p->parent()) ++depth;
            qDebug() << QByteArray(4 * depth, ' ').constData()
                     << (w.parent()? w.parent()->children().indexOf(&w) : -1)
                     << w.name() << "childCount:" << w.childCount();
        };

        root.walkInOrder(Widget::Forward, [&debugPrint] (Widget &w)
        {
            debugPrint(w);
            return LoopContinue;
        });

        qDebug() << "AND BACKWARDS:";

        root.children().last()->walkInOrder(Widget::Backward, [&debugPrint] (Widget &w)
        {
            debugPrint(w);
            return LoopContinue;
        });
#endif
        */

#if !defined (DE_MOBILE)
        self().audienceForAttributeChange() += this;
#endif

        uiSetupDone = true;
    }

    void fileSystemBusyStatusChanged(FS::BusyStatus bs) override
    {
        notifications->showOrHide(*fsBusy, bs == FS::Busy);
    }

    void appStartupCompleted() override
    {
        taskBar->show();

        // Show the tutorial if it hasn't been automatically shown yet.
        if (!App::config().getb("tutorial.shown", false))
        {
            App::config().set("tutorial.shown", true);
            LOG_NOTE("Starting tutorial (not shown before)");
            Loop::timer(0.500, [this]() { taskBar->showTutorial(); });
        }
        else
        {
            taskBar->close();
        }

        FS::get().audienceForBusy() += this;
        Loop::timer(1.0, [this] () { showOrHideQuitButton(); });
    }

    void currentGameChanged(Game const &newGame) override
    {
        minimizeGame(false);
        showOrHideQuitButton();

        if (!newGame.isNull())
        {
            nowPlaying->setText(_E(l) "Now playing\n" _E(b) +
                                DoomsdayApp::currentGameProfile()->name());

            root.clearFocusStack();
            root.setFocus(nullptr);
        }
        else // Back to Home.
        {
            // Release all buffered frames.
            busy->releaseTransitionFrame();
        }

        // Check with Style if blurring is allowed.
        taskBar->console().enableBlur(taskBar->style().isBlurringAllowed());
        self().hideTaskBarBlur(); // update background blur mode

        activateOculusRiftModeIfConnected();
    }

    void activateOculusRiftModeIfConnected()
    {
        if (vrCfg().oculusRift().isHMDConnected() && vrCfg().mode() != VRConfig::OculusRift)
        {
            LOG_NOTE("HMD connected, automatically switching to Oculus Rift mode");

            Con_SetInteger("rend-vr-mode", VRConfig::OculusRift);
            vrCfg().oculusRift().moveWindowToScreen(OculusRift::HMDScreen);
        }
        else if (!vrCfg().oculusRift().isHMDConnected() && vrCfg().mode() == VRConfig::OculusRift)
        {
            LOG_NOTE("HMD not connected, disabling VR mode");

            Con_SetInteger("rend-vr-mode", VRConfig::Mono);
            vrCfg().oculusRift().moveWindowToScreen(OculusRift::DefaultScreen);
        }
    }

    void setMode(Mode const &newMode)
    {
        LOG_DEBUG("Switching to %s mode") << (newMode == Busy? "Busy" : "Normal");

        // Hide and show widgets as appropriate.
        switch (newMode)
        {
        case Busy:
            game->hide();
            game->disable();
            taskBar->disable();
            quitButton->setOpacity(0, 1.0);

            busy->show();
            busy->enable();
            break;

        case Normal:
#if defined (DE_HAVE_BUSYRUNNER)
            // The busy widget will hide itself after a possible transition has finished.
            busy->disable();
#else
            busy->hide();
#endif

            game->show();
            game->enable();
            taskBar->enable();
            quitButton->setOpacity(1, 1.0);
            break;
        }

        mode = newMode;
    }

    void windowInit(GLWindow &) override
    {
        Sys_GLConfigureDefaultState();
        GL_Init2DState();

        // Update the capability flags.
        LOGDEV_GL_MSG("GL feature: Multisampling: %b") << (GLTextureFramebuffer::defaultMultisampling() > 1);

        // Now that the window is ready for drawing we can enable the GameWidget.
        game->enable();

        // Configure a viewport immediately.
        GLState::current().setViewport(Rectangleui(0, 0, self().pixelWidth(), self().pixelHeight()));

        LOG_DEBUG("GameWidget enabled");

        if (needMainInit)
        {
            needMainInit = false;

#if !defined (DE_MOBILE)
            self().raise();
//            self().requestActivate();
#endif
            self().eventHandler().audienceForFocusChange() += this;

            DD_FinishInitializationAfterWindowReady();

            vrCfg().oculusRift().glPreInit();
            contentXf.glInit();
        }
    }

    void windowResized(GLWindow &) override
    {
        LOG_AS("ClientWindow");

        Size size = self().pixelSize();
        LOG_VERBOSE("Window resized to %s pixels") << size.asText();

        GLState::current().setViewport(Rectangleui(0, 0, size.x, size.y));

        updateRootSize();

        // Show or hide the Quit button depending on the window mode.
        showOrHideQuitButton();
    }

    void windowSwapped(GLWindow &) override
    {
        if (DoomsdayApp::app().isShuttingDown()) return;

        ClientApp::app().postFrame(); /// @todo what about multiwindow?

        // Frame has been shown, now we can do post-frame updates.
        updateFpsNotification(self().frameRate());
        completeFade();
    }

#if !defined (DE_MOBILE)
    void windowAttributesChanged(PersistentGLWindow &) override
    {
        showOrHideQuitButton();
    }
#endif

    void showOrHideQuitButton()
    {
        TimeSpan const SPAN = 0.6;
        if (self().isFullScreen() && !DoomsdayApp::isGameLoaded())
        {
            quitX->set(-quitButton->rule().width() - Style::get().rules().rule("gap"), SPAN);
        }
        else
        {
            quitX->set(0, SPAN);
        }
    }

    void mouseStateChanged(MouseEventSource::State state) override
    {
//        Mouse_Trap(state == MouseEventSource::Trapped);

//        DE_ASSERT_FAIL("Change mouse state with SDL?");
    }

#if 0
    /**
     * Handles an event that was not eaten by any widget.
     *
     * @param event  Event to handle.
     */
    bool handleFallbackEvent(Event const &ev)
    {
        if (const MouseEvent *mouse = maybeAs<MouseEvent>(ev))
        {
            // Fall back to legacy handling.
            switch (ev.type())
            {
            case Event::MouseButton:
                if (game->hitTest(ev))
                {
//                    Mouse_Qt_SubmitButton(
//                                mouse->button() == MouseEvent::Left?     IMB_LEFT   :
//                                mouse->button() == MouseEvent::Middle?   IMB_MIDDLE :
//                                mouse->button() == MouseEvent::Right?    IMB_RIGHT  :
//                                mouse->button() == MouseEvent::XButton1? IMB_EXTRA1 :
//                                mouse->button() == MouseEvent::XButton2? IMB_EXTRA2 : IMB_MAXBUTTONS,
//                                mouse->state() == MouseEvent::Pressed);
                    return true;
                }
                break;

            case Event::MouseMotion:
//                Mouse_Qt_SubmitMotion(IMA_POINTER, mouse->pos().x, mouse->pos().y);
                return true;

            case Event::MouseWheel:
                if (game->hitTest(ev) && mouse->wheelMotion() == MouseEvent::Steps)
                {
                    // The old input system can only do wheel step events.
//                    Mouse_Qt_SubmitMotion(IMA_WHEEL, mouse->wheel().x, mouse->wheel().y);
                    return true;
                }
                break;

            default:
                break;
            }
        }
        return false;
    }
#endif

    void windowFocusChanged(GLWindow &, bool hasFocus) override
    {
        LOG_DEBUG("windowFocusChanged focus:%b fullscreen:%b hidden:%b minimized:%b")
                << hasFocus << self().isFullScreen() << self().isHidden() << self().isMinimized();

        if (!hasFocus)
        {
            InputSystem::get().forAllDevices([] (InputDevice &device)
            {
                device.reset();
                return LoopContinue;
            });
            InputSystem::get().clearEvents();

            self().eventHandler().trapMouse(false);
        }
        else if (self().isFullScreen() && !taskBar->isOpen() && DoomsdayApp::isGameLoaded())
        {
            // Trap the mouse again in fullscreen mode.
            self().eventHandler().trapMouse();
        }

        if (Config::get().getb("audio.pauseOnFocus", true))
        {
            AudioSystem::get().pauseMusic(!hasFocus);
        }

        // Generate an event about this.
        ddevent_t ev{};
        ev.device         = -1;
        ev.type           = E_FOCUS;
        ev.focus.gained   = hasFocus;
        ev.focus.inWindow = 1;         /// @todo Ask WindowSystem for an identifier number.
        InputSystem::get().postEvent(ev);
    }

    void updateFpsNotification(float fps)
    {
        notifications->showOrHide(*fpsCounter, self().isFPSCounterVisible());

        if (!fequal(oldFps, fps))
        {
            fpsCounter->setText(Stringf("%.1f " _E(l) "FPS", fps));
            oldFps = fps;
        }
    }

    void variableValueChanged(Variable &variable, Value const &newValue) override
    {
        if (variable.name() == "fsaa")
        {
            //self().updateCanvasFormat();
        }
        else if (variable.name() == "vsync")
        {
/*#ifdef WIN32
            self().updateCanvasFormat();
            DE_UNUSED(newValue);
#else*/
            GL_SetVSync(newValue.isTrue());
//#endif
        }
    }

    void installSidebar(SidebarLocation location, GuiWidget *widget)
    {
        // Get rid of the old sidebar.
        if (sidebar)
        {
            uninstallSidebar(location);
        }
        if (!widget) return;

        // Maximize game to hide the "Now playing" controls.
        home->moveOffscreen(1.0);

        DE_ASSERT(sidebar == NULL);

        // Attach the widget.
        switch (location)
        {
        case RightEdge:
            widget->rule()
                    .setInput(Rule::Top,    root.viewTop())
                    .setInput(Rule::Right,  root.viewRight())
                    .setInput(Rule::Bottom, taskBar->rule().top());
            game->rule()
                    .setInput(Rule::Right,  widget->rule().left());
            notifications->rule()
                    .setInput(Rule::Right,  widget->rule().left() - Style::get().rules().rule("gap"));
            /*gameUI->rule()
                    .setInput(Rule::Right,  widget->rule().left());*/
            break;
        }

        sidebar = widget;
        root.insertBefore(sidebar, *notifications);
    }

    void uninstallSidebar(SidebarLocation location)
    {
        DE_ASSERT(sidebar != NULL);

        switch (location)
        {
        case RightEdge:
            game->rule().setInput(Rule::Right, root.viewLeft() + *gameWidth);
            notifications->useDefaultPlacement(root.viewRule(), *quitX);
            break;
        }

        root.remove(*sidebar);
        sidebar->guiDeleteLater();
        sidebar = 0;
    }

    void updateRootSize()
    {
        DE_ASSERT_IN_MAIN_THREAD();

        needRootSizeUpdate = false;

        Vec2ui const size = contentXf.logicalRootSize(self().pixelSize());

        // Tell the widgets.
        root.setViewSize(size);
    }

    void minimizeGame(bool mini)
    {
        TimeSpan const SPAN = 1.0;

        if (mini && !isGameMini)
        {
            // Get rid of the sidebar, if it's open.
            self().setSidebar(RightEdge, nullptr);

            auto const &unit = Style::get().rules().rule(RuleBank::UNIT);

            gameWidth ->set(root.viewWidth()/2 - unit, SPAN);
            gameHeight->set(root.viewHeight()/4,       SPAN);
            homeDelta ->set(-*gameHeight - unit,       SPAN);
            isGameMini = true;
        }
        else if (!mini && isGameMini)
        {
            gameWidth ->set(root.viewWidth(),  SPAN);
            gameHeight->set(root.viewHeight(), SPAN);
            homeDelta ->set(0,                 SPAN);
            isGameMini = false;
        }
    }

    void updateMouseCursor()
    {
#if !defined (DE_MOBILE)
        // The cursor is only needed if the content is warped.
        cursor->show(!self().eventHandler().isMouseTrapped() && VRConfig::modeAppliesDisplacement(vrCfg().mode()));

        // Show or hide the native mouse cursor.
        if (cursor->isVisible())
        {
            if (!cursorHasBeenHidden)
            {
//                qApp->setOverrideCursor(QCursor(Qt::BlankCursor));
                DE_GUI_APP->setMouseCursor(GuiApp::None);
                cursorHasBeenHidden = true;
            }

            Vec2i cp = ClientApp::windowSystem().latestMousePosition();
            cursorX->set(cp.x);
            cursorY->set(cp.y);
        }
        else
        {
            if (cursorHasBeenHidden)
            {
//                qApp->restoreOverrideCursor();
                DE_GUI_APP->setMouseCursor(GuiApp::Arrow);
            }
            cursorHasBeenHidden = false;
        }
#endif
    }

    void setupFade(FadeDirection fadeDir, TimeSpan const &span)
    {
        if (!fader)
        {
            fader.reset(new FadeToBlackWidget);
            fader->rule().setRect(root.viewRule());
            root.add(fader); // on top of everything else
        }
        if (fadeDir == FadeFromBlack)
        {
            fader->initFadeFromBlack(span);
        }
        else
        {
            fader->initFadeToBlack(span);
        }
        fader->start();
    }

    void completeFade()
    {
        // Check if the fade is done.
        if (fader)
        {
            fader->disposeIfDone();
        }
    }
};

ClientWindow::ClientWindow(String const &id)
    : BaseWindow(id)
    , d(new Impl(this))
{
#if defined (DE_MOBILE)
    setMain(this);
    WindowSystem::get().addWindow("main", this);
#endif

    audienceForInit()   += d;
    audienceForResize() += d;
    audienceForSwap()   += d;

#if defined (WIN32)
    // Set an icon for the window.
    setIcon(QIcon(":/doomsday.ico"));
#endif

    d->setupUI();

#if defined (DE_MOBILE)
    // Stay out from under the virtual keyboard.
    connect(this, &GLWindow::rootDimensionsChanged, [this] (QRect rect)
    {
        d->root.rootOffset().setValue(Vec2f(0, int(rect.height()) - int(pixelSize().y)), 0.3);
    });
#endif
}

bool ClientWindow::isUICreated() const
{
    return d->uiSetupDone;
}

ClientRootWidget &ClientWindow::root()
{
    return d->root;
}

TaskBarWidget &ClientWindow::taskBar()
{
    return *d->taskBar;
}

GuiWidget &ClientWindow::taskBarBlur()
{
    return *d->taskBarBlur;
}

ConsoleWidget &ClientWindow::console()
{
    return d->taskBar->console();
}

NotificationAreaWidget &ClientWindow::notifications()
{
    return *d->notifications;
}

HomeWidget &ClientWindow::home()
{
    return *d->home;
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

void ClientWindow::setGameMinimized(bool minimize)
{
    d->minimizeGame(minimize);
}

bool ClientWindow::isGameMinimized() const
{
    return d->isGameMini;
}

void ClientWindow::windowAboutToClose()
{
    if (BusyMode_Active())
    {
        // Oh well, we can't cancel busy mode...
        // TODO: Wait until busy mode ends.
        return;
    }

    Sys_Quit();
}

void ClientWindow::preDraw()
{
    // NOTE: This occurs during the Canvas paintGL event.

    ClientApp::app().preFrame(); /// @todo what about multiwindow?

    DE_ASSERT_IN_RENDER_THREAD();
    LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();

    // Cursor position (if cursor is visible).
    d->updateMouseCursor();

    if (d->needRootSizeUpdate)
    {
        d->updateRootSize();
    }

    BaseWindow::preDraw();
}

Vec2f ClientWindow::windowContentSize() const
{
    return Vec2f(d->root.viewWidth().value(), d->root.viewHeight().value());
}

void ClientWindow::drawWindowContent()
{
#if defined (DE_MOBILE)
    {

    }
#endif

    DE_ASSERT_IN_RENDER_THREAD();
    root().draw();
    LIBGUI_ASSERT_GL_OK();
}

void ClientWindow::postDraw()
{
    /// @note This method is called during the GLWindow paintGL event.
    DE_ASSERT_IN_RENDER_THREAD();

    BaseWindow::postDraw();

    // OVR will handle presentation in Oculus Rift mode.
    if (ClientApp::vr().mode() != VRConfig::OculusRift)
    {
        // Finish GL drawing and swap it on to the screen. Blocks until buffers
        // swapped.
        GL_FinishFrame();
    }
}

bool ClientWindow::setDefaultGLFormat() // static
{
    LOG_AS("DefaultGLFormat");

#if 0
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();

    if (CommandLine_Exists("-novsync") || !App::config().getb("window.main.vsync"))
    {
        fmt.setSwapInterval(0);
    }
    else
    {
        fmt.setSwapInterval(1);
    }

    /// @todo Multisampling should only be enabled on the game view FBO. The rest of
    /// the UI is always single-sampled.

    if (fmt != QSurfaceFormat::defaultFormat())
    {
        LOG_GL_VERBOSE("Applying new format...");
        QSurfaceFormat::setDefaultFormat(fmt);
        return true;
    }
    else
    {
        LOG_GL_XVERBOSE("New format is the same as before", "");
        return false;
    }
#endif
    return false;
}

void ClientWindow::grab(image_t &img, bool halfSized) const
{
    /// @todo This method should be removed, just use grabImage() directly.

    DE_ASSERT_IN_MAIN_THREAD();

    Vec2ui outputSize = (halfSized? Vec2ui(pixelWidth()/2, pixelHeight()/2) : Vec2ui());
    const Image grabbed = grabImage(outputSize);

    Image_Init(img);
    img.size      = Vec2ui(grabbed.width(), grabbed.height());
    img.pixelSize = grabbed.depth()/8;

    img.pixels = reinterpret_cast<uint8_t *>(malloc(grabbed.byteCount()));
    std::memcpy(img.pixels, grabbed.bits(), grabbed.byteCount());

    LOGDEV_GL_MSG("Grabbed Canvas contents %i x %i, byteCount:%i depth:%i format:%i")
            << grabbed.width() << grabbed.height()
            << grabbed.byteCount() << grabbed.depth() << grabbed.format();

    DE_ASSERT(img.pixelSize != 0);
}

void ClientWindow::fadeInTaskBarBlur(TimeSpan span)
{
    d->taskBarBlur->setAttribute(GuiWidget::DontDrawContent, UnsetFlags);
    d->taskBarBlur->setOpacity(0);
    d->taskBarBlur->setOpacity(1, span);
}

void ClientWindow::fadeOutTaskBarBlur(TimeSpan span)
{
    d->taskBarBlur->setOpacity(0, span);
    Loop::timer(span, [this](){ hideTaskBarBlur(); });
}

void ClientWindow::hideTaskBarBlur()
{
    d->taskBarBlur->setAttribute(GuiWidget::DontDrawContent);
    if (d->taskBar->style().isBlurringAllowed())
    {
        d->taskBarBlur->setOpacity(1);
    }
    else
    {
        d->taskBarBlur->setOpacity(0);
    }
}

void ClientWindow::updateRootSize()
{
    // This will be done a bit later as the call may originate from another thread.
    d->needRootSizeUpdate = true;
}

ClientWindow &ClientWindow::main()
{
    return static_cast<ClientWindow &>(BaseWindow::main());
}

bool ClientWindow::mainExists()
{
    return BaseWindow::mainExists();
}

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
    d->root.add(widget);

    // Make sure the cursor remains the topmost widget.
    d->root.moveChildToLast(*d->cursor);
}

void ClientWindow::setSidebar(SidebarLocation location, GuiWidget *sidebar)
{
    DE_ASSERT(location == RightEdge);

    d->installSidebar(location, sidebar);
}

bool ClientWindow::hasSidebar(SidebarLocation location) const
{
    DE_ASSERT(location == RightEdge);
    DE_UNUSED(location);

    return d->sidebar != 0;
}

GuiWidget &ClientWindow::sidebar(SidebarLocation location) const
{
    DE_ASSERT(location == RightEdge);
    DE_UNUSED(location);
    DE_ASSERT(d->sidebar != nullptr);

    return *d->sidebar;
}

//bool ClientWindow::handleFallbackEvent(Event const &event)
//{
//    return d->handleFallbackEvent(event);
//}

void ClientWindow::fadeContent(FadeDirection fadeDirection, TimeSpan const &duration)
{
    d->setupFade(fadeDirection, duration);
}

FadeToBlackWidget *ClientWindow::contentFade()
{
    return d->fader;
}

#undef M_ScreenShot
DE_EXTERN_C int M_ScreenShot(char const *name, int flags)
{
    de::String fullName(name);
    if(fullName.fileNameExtension().isEmpty())
    {
        fullName += ".png"; // Default format.
    }

    // By default, place the file in the runtime folder.
    NativePath const shotPath(App::app().nativeHomePath() / fullName);

    if (flags & DD_SCREENSHOT_CHECK_EXISTS)
    {
        return (shotPath.exists() ? 1 : 0);
    }

    ClientWindow::main().grabToFile(shotPath);
    return 1;
}
