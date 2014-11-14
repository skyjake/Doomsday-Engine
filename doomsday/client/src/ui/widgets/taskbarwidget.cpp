/** @file taskbarwidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/consolecommandwidget.h"
#include "ui/widgets/multiplayermenuwidget.h"
#include "ui/widgets/tutorialwidget.h"
#include "ui/dialogs/aboutdialog.h"
#include "ui/dialogs/videosettingsdialog.h"
#include "ui/dialogs/audiosettingsdialog.h"
#include "ui/dialogs/inputsettingsdialog.h"
#include "ui/dialogs/networksettingsdialog.h"
#include "ui/dialogs/renderersettingsdialog.h"
#include "ui/dialogs/manualconnectiondialog.h"
#include "ui/dialogs/vrsettingsdialog.h"
#include "ui/dialogs/gamesdialog.h"
#include "updater/updatersettingsdialog.h"
#include "ui/clientwindow.h"
#include "ui/clientrootwidget.h"
#include "clientapp.h"
#include "CommandAction"
#include "client/cl_def.h" // clientPaused
#include "ui/ui_main.h"
#include "versioninfo.h"
#include "dd_main.h"

#include <doomsday/console/exec.h>

#include <de/KeyEvent>
#include <de/Drawable>
#include <de/GLBuffer>
#include <de/ScalarRule>
#include <de/SignalAction>
#include <de/ui/SubwidgetItem>
#include <de/SequentialLayout>
#include <de/ButtonWidget>
#include <de/PopupMenuWidget>
#include <de/BlurWidget>

using namespace de;
using namespace ui;

static TimeDelta OPEN_CLOSE_SPAN = 0.2;

enum MenuItemPositions
{
    // DE menu:
    POS_GAMES             = 0,
    POS_UNLOAD            = 1,
    POS_GAMES_SEPARATOR   = 2,
    POS_MULTIPLAYER       = 3,
    POS_CONNECT           = 4,

    // Config menu:
    POS_RENDERER_SETTINGS = 0,
    POS_VR_SETTINGS       = 1,
    POS_CONFIG_SEPARATOR  = 2,

    POS_AUDIO_SETTINGS    = 4,
    POS_INPUT_SETTINGS    = 5
};

DENG_GUI_PIMPL(TaskBarWidget)
, DENG2_OBSERVES(App, GameChange)
, DENG2_OBSERVES(ServerLink, Join)
, DENG2_OBSERVES(ServerLink, Leave)
{
    typedef DefaultVertexBuf VertexBuf;

    enum LayoutMode {
        NormalLayout,           ///< Taskbar widgets are full-sized.
        CompressedLayout,       ///< Cull some redundant information.
        ExtraCompressedLayout   ///< Hide current game indicator.
    };
    LayoutMode layoutMode;

    bool opened;

    ConsoleWidget *console;
    ButtonWidget *logo;
    ButtonWidget *conf;
    ButtonWidget *multi;
    LabelWidget *status;
    PopupMenuWidget *mainMenu;
    PopupMenuWidget *configMenu;
    MultiplayerMenuWidget *multiMenu;

    ScalarRule *vertShift;
    bool mouseWasTrappedWhenOpening;
    int minSpace;
    int maxSpace;

    // GL objects:
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    Matrix4f projMatrix;

    Instance(Public *i)
        : Base(i)
        , layoutMode(NormalLayout)
        , opened(true)
        , logo(0)
        , conf(0)
        , status(0)
        , mainMenu(0)
        , configMenu(0)
        , multiMenu(0)
        , mouseWasTrappedWhenOpening(false)
        , uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        , uColor    ("uColor",     GLUniform::Vec4)
    {
        uColor = Vector4f(1, 1, 1, 1);
        self.set(Background(style().colors().colorf("background")));

        vertShift = new ScalarRule(0);

        App::app().audienceForGameChange() += this;
        ClientApp::serverLink().audienceForJoin += this;
        ClientApp::serverLink().audienceForLeave += this;

        updateStyle();
    }

    ~Instance()
    {
        App::app().audienceForGameChange() -= this;
        ClientApp::serverLink().audienceForJoin -= this;
        ClientApp::serverLink().audienceForLeave -= this;

        releaseRef(vertShift);
    }

    void updateStyle()
    {
        minSpace = style().rules().rule("console.commandline.width.min").valuei();
        maxSpace = style().rules().rule("console.commandline.width.max").valuei();
    }

    void updateLayoutMode()
    {
        LayoutMode wanted = layoutMode;

        // Does the command line have enough space?
        if(console->commandLine().rule().width().valuei() < minSpace)
        {
            wanted = (layoutMode == NormalLayout?     CompressedLayout :
                      layoutMode == CompressedLayout? ExtraCompressedLayout :
                                                      layoutMode);
        }
        else if(console->commandLine().rule().width().valuei() > maxSpace)
        {
            wanted = (layoutMode == CompressedLayout?      NormalLayout :
                      layoutMode == ExtraCompressedLayout? CompressedLayout :
                                                           layoutMode);
        }

        if(layoutMode != wanted)
        {
            layoutMode = wanted;
            updateLogoButtonText();

            // Adjust widget visibility and rules.
            switch(layoutMode)
            {
            case NormalLayout:
            case CompressedLayout:
                status->show();
                break;

            case ExtraCompressedLayout:
                status->hide();
                break;
            }

            self.updateCommandLineLayout();
            self.requestGeometry();
            console->commandLine().requestGeometry();
        }
    }

    void glInit()
    {
        drawable.addBuffer(new VertexBuf);

        shaders().build(drawable.program(), "generic.color_ucolor")
                << uMvpMatrix
                << uColor;

        updateProjection();
    }

    void glDeinit()
    {
        drawable.clear();
    }

    void updateLogoButtonText()
    {
        String text;

        if(layoutMode == NormalLayout)
        {
            VersionInfo currentVersion;
            if(String(DOOMSDAY_RELEASE_TYPE) == "Stable")
            {
                text = _E(b) + currentVersion.base();
            }
            else
            {
                text = _E(b) + currentVersion.base() + " " +
                       _E(l) + String("#%1").arg(currentVersion.build);
            }
        }
        else
        {
            // Remove the version number if we're short of space.
        }

        logo->setText(text);
    }

    void updateProjection()
    {
        uMvpMatrix = root().projMatrix2D();
    }

    void updateGeometry()
    {
        Rectanglei pos;
        if(self.hasChangedPlace(pos) || self.geometryRequested())
        {
            self.requestGeometry(false);

            VertexBuf::Builder verts;
            self.glMakeGeometry(verts);
            drawable.buffer<VertexBuf>().setVertices(gl::TriangleStrip, verts, gl::Static);
        }
    }

    GuiWidget &itemWidget(PopupMenuWidget *menu, uint pos) const
    {
        return *menu->menu().organizer().itemWidget(pos);
    }

    void showOrHideMenuItems()
    {
        de::Game &game = App_CurrentGame();

        itemWidget(mainMenu, POS_GAMES)            .show(!game.isNull());
        itemWidget(mainMenu, POS_UNLOAD)           .show(!game.isNull());
        itemWidget(mainMenu, POS_GAMES_SEPARATOR)  .show(!game.isNull());
        itemWidget(mainMenu, POS_MULTIPLAYER)      .show(!game.isNull());
        itemWidget(mainMenu, POS_CONNECT)          .show(game.isNull());

        itemWidget(configMenu, POS_RENDERER_SETTINGS).show(!game.isNull());
        itemWidget(configMenu, POS_VR_SETTINGS)      .show(!game.isNull());
        itemWidget(configMenu, POS_CONFIG_SEPARATOR) .show(!game.isNull());
        itemWidget(configMenu, POS_AUDIO_SETTINGS)   .show(!game.isNull());
        itemWidget(configMenu, POS_INPUT_SETTINGS)   .show(!game.isNull());

        if(self.hasRoot())
        {
            configMenu->menu().updateLayout();
            mainMenu->menu().updateLayout(); // Include/exclude shown/hidden menu items.
        }
    }

    void currentGameChanged(game::Game const &)
    {
        updateStatus();
        showOrHideMenuItems();
    }

    void networkGameJoined()
    {
        multi->show();
        self.updateCommandLineLayout();
    }

    void networkGameLeft()
    {
        multi->hide();
        self.updateCommandLineLayout();
    }

    void updateStatus()
    {
        if(App_GameLoaded())
        {
            status->setText(App_CurrentGame().identityKey());
        }
        else
        {
            status->setText(tr("No game loaded"));
        }
    }
};

PopupWidget *makeUpdaterSettings() {
    return new UpdaterSettingsDialog(UpdaterSettingsDialog::WithApplyAndCheckButton);
}

TaskBarWidget::TaskBarWidget() : GuiWidget("taskbar"), d(new Instance(this))
{
#if 0
    // GameWidget is presently incompatible with blurring.
    BlurWidget *blur = new BlurWidget("taskbar_blur");
    add(blur);
    Background bg(*blur, style().colors().colorf("background"));
#else
    Background bg(style().colors().colorf("background"));
#endif

    Rule const &gap = style().rules().rule("gap");

    d->console = new ConsoleWidget;
    d->console->rule()
            .setInput(Rule::Left, rule().left() + d->console->shift());
    add(d->console);

    //d->console->log().set(bg);

    // Position the console button and command line in the task bar.
    d->console->buttons().rule()
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Bottom, rule().bottom())
            .setInput(Rule::Height, rule().height());

    // DE logo.
    d->logo = new ButtonWidget("de-button");
    d->logo->setImage(style().images().image("logo.px128"));
    d->logo->setImageScale(.475f);
    d->logo->setImageFit(FitToHeight | OriginalAspectRatio);

    d->updateLogoButtonText();

    d->logo->setWidthPolicy(ui::Expand);
    d->logo->setTextAlignment(AlignLeft);
    d->logo->rule().setInput(Rule::Height, rule().height());
    add(d->logo);

    // Settings.
    ButtonWidget *conf = new ButtonWidget("conf-button");
    d->conf = conf;
    conf->setImage(style().images().image("gear"));
    conf->setSizePolicy(ui::Expand, ui::Filled);
    conf->rule().setInput(Rule::Height, rule().height());
    add(conf);

    // Currently loaded game.
    d->status = new LabelWidget;
    d->status->set(bg);
    d->status->setWidthPolicy(ui::Expand);
    d->status->rule().setInput(Rule::Height, rule().height());
    add(d->status);        

    d->updateStatus();

    // Multiplayer.
    d->multi = new ButtonWidget;
    d->multi->hide(); // hidden when not connected
    d->multi->setAction(new SignalAction(this, SLOT(openMultiplayerMenu())));
    d->multi->setImage(style().images().image("network"));
    d->multi->setTextAlignment(ui::AlignRight);
    d->multi->setText(tr("MP"));
    d->multi->setSizePolicy(ui::Expand, ui::Filled);
    d->multi->rule().setInput(Rule::Height, rule().height());
    add(d->multi);

    // Taskbar height depends on the font size.
    rule().setInput(Rule::Height, style().fonts().font("default").height() + gap * 2);

    // Settings menu.
    add(d->configMenu = new PopupMenuWidget("conf-menu"));
    d->configMenu->setAnchorAndOpeningDirection(conf->rule(), ui::Up);

    // Multiplayer menu.
    add(d->multiMenu = new MultiplayerMenuWidget);
    d->multiMenu->setAnchorAndOpeningDirection(d->multi->rule(), ui::Up);

    // The DE menu.
    add(d->mainMenu = new PopupMenuWidget("de-menu"));
    d->mainMenu->setAnchorAndOpeningDirection(d->logo->rule(), ui::Up);

    // Game unloading confirmation submenu.
    ui::SubmenuItem *unloadMenu = new ui::SubmenuItem(tr("Unload Game"), ui::Left);
    unloadMenu->items()
            << new ui::Item(ui::Item::Separator, tr("Really unload the game?"))
            << new ui::ActionItem(tr("Unload") + " " _E(b) + tr("(discard progress)"), new SignalAction(this, SLOT(unloadGame())))
            << new ui::ActionItem(tr("Cancel"), new SignalAction(&d->mainMenu->menu(), SLOT(dismissPopups())));

    /*
     * Set up items for the config and DE menus. Some of these are shown/hidden
     * depending on whether a game is loaded.
     */
    d->configMenu->items()
            << new ui::SubwidgetItem(style().images().image("renderer"), tr("Renderer"), ui::Left, makePopup<RendererSettingsDialog>)
            << new ui::SubwidgetItem(style().images().image("vr"),       tr("3D & VR"),  ui::Left, makePopup<VRSettingsDialog>)
            << new ui::Item(ui::Item::Separator)
            << new ui::SubwidgetItem(style().images().image("display"),  tr("Video"),    ui::Left, makePopup<VideoSettingsDialog>)
            << new ui::SubwidgetItem(style().images().image("audio"),    tr("Audio"),    ui::Left, makePopup<AudioSettingsDialog>)
            << new ui::SubwidgetItem(style().images().image("input"),    tr("Input"),    ui::Left, makePopup<InputSettingsDialog>)
            << new ui::SubwidgetItem(style().images().image("network"),  tr("Network"),  ui::Left, makePopup<NetworkSettingsDialog>)
            << new ui::SubwidgetItem(style().images().image("updater"),  tr("Updater"),  ui::Left, makeUpdaterSettings);

    d->mainMenu->items()
            << new ui::ActionItem(tr("Switch Game..."), new SignalAction(this, SLOT(switchGame())))
            << unloadMenu                           // hidden with null-game
            << new ui::Item(ui::Item::Separator)
            << new ui::ActionItem(tr("Multiplayer Games..."), new SignalAction(this, SLOT(showMultiplayer())))
            << new ui::ActionItem(tr("Connect to Server..."), new SignalAction(this, SLOT(connectToServerManually())))
            << new ui::Item(ui::Item::Separator)
            << new ui::Item(ui::Item::Separator, tr("Help"))
            << new ui::ActionItem(tr("Show Tutorial"), new SignalAction(this, SLOT(showTutorial())))
            << new ui::VariableToggleItem(tr("Menu Annotations"), App::config("ui.showAnnotations"))
            << new ui::Item(ui::Item::Annotation, tr("Hides these brief descriptions about menu functions."))
            << new ui::Item(ui::Item::Separator)
            << new ui::Item(ui::Item::Separator, tr("Application"))
            << new ui::ActionItem(tr("Check for Updates..."), new CommandAction("updateandnotify"))
            << new ui::ActionItem(tr("About Doomsday"), new SignalAction(this, SLOT(showAbout())))
            << new ui::Item(ui::Item::Separator)
            << new ui::ActionItem(tr("Quit Doomsday"), new CommandAction("quit"));

    d->showOrHideMenuItems();

    conf->setAction(new SignalAction(this, SLOT(openConfigMenu())));
    d->logo->setAction(new SignalAction(this, SLOT(openMainMenu())));

    // Set the initial command line layout.
    updateCommandLineLayout();

    connect(d->console, SIGNAL(commandModeChanged()), this, SLOT(updateCommandLineLayout()));
    connect(d->console, SIGNAL(commandLineGotFocus()), this, SLOT(closeMainMenu()));
    connect(d->console, SIGNAL(commandLineGotFocus()), this, SLOT(closeConfigMenu()));
}

ConsoleWidget &TaskBarWidget::console()
{
    return *d->console;
}

CommandWidget &TaskBarWidget::commandLine()
{
    return d->console->commandLine();
}

ButtonWidget &TaskBarWidget::logoButton()
{
    return *d->logo;
}

bool TaskBarWidget::isOpen() const
{
    return d->opened;
}

Rule const &TaskBarWidget::shift()
{
    return *d->vertShift;
}

void TaskBarWidget::glInit()
{
    LOG_AS("TaskBarWidget");
    d->glInit();
}

void TaskBarWidget::glDeinit()
{
    d->glDeinit();
}

void TaskBarWidget::viewResized()
{
    GuiWidget::viewResized();
    d->updateProjection();
}

void TaskBarWidget::update()
{
    GuiWidget::update();
    d->updateLayoutMode();
}

void TaskBarWidget::drawContent()
{
    d->updateGeometry();
}

bool TaskBarWidget::handleEvent(Event const &event)
{
    Canvas &canvas = root().window().canvas();
    ClientWindow &window = root().window().as<ClientWindow>();

    if(!canvas.isMouseTrapped() && event.type() == Event::MouseButton &&
       !window.hasSidebar())
    {
        // Clicking outside the taskbar will trap the mouse automatically.
        MouseEvent const &mouse = event.as<MouseEvent>();
        if(mouse.state() == MouseEvent::Released && !hitTest(mouse.pos()))
        {
            if(root().focus())
            {
                // First click will remove UI focus, allowing GameWidget
                // to receive events.
                root().setFocus(0);
                return true;
            }

            if(App_GameLoaded())
            {
                // Allow game to use the mouse.
                canvas.trapMouse();
            }

            window.taskBar().close();
            return true;
        }
    }

    if(event.type() == Event::MouseButton)
    {
        // Eat all button events occurring inside the task bar area.
        if(hitTest(event))
        {
            return true;
        }
    }

    // Don't let modifier keys fall through to the game.
    if(isOpen() && event.isKey() && event.as<KeyEvent>().isModifier())
    {
        // However, let the bindings system know about the modifier state.
        ClientApp::inputSystem().trackEvent(event);
        return true;
    }

    if(event.type() == Event::KeyPress)
    {
        KeyEvent const &key = event.as<KeyEvent>();

        // Shift-Esc opens and closes the task bar.
        if(key.ddKey() == DDKEY_ESCAPE)
        {
            if(isOpen())
            {
                // First press of Esc will just dismiss the console.
                if(d->console->isLogOpen() && !key.modifiers().testFlag(KeyEvent::Shift))
                {
                    d->console->commandLine().setText("");
                    d->console->closeLog();
                    root().setFocus(0);
                    return true;
                }
                // Also closes the console log.
                close();
                return true;
            }
            else
            {
                // Task bar is closed, so let's open it.
                if(key.modifiers().testFlag(KeyEvent::Shift) ||
                   !App_GameLoaded())
                {
                    if(!window.hasSidebar())
                    {
                        // Automatically focus the command line, unless an editor is open.
                        root().setFocus(&d->console->commandLine());
                    }

                    open();
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

void TaskBarWidget::open()
{
    if(!d->opened)
    {
        d->opened = true;

        unsetBehavior(DisableEventDispatchToChildren);

        d->console->clearLog();

        d->vertShift->set(0, OPEN_CLOSE_SPAN);
        setOpacity(1, OPEN_CLOSE_SPAN);

        emit opened();
    }

    // Untrap the mouse if it is trapped.
    if(hasRoot())
    {
        Canvas &canvas = root().window().canvas();
        d->mouseWasTrappedWhenOpening = canvas.isMouseTrapped();
        if(canvas.isMouseTrapped())
        {
            canvas.trapMouse(false);
        }

        if(!App_GameLoaded())
        {
            root().setFocus(&d->console->commandLine());
        }
    }
}

void TaskBarWidget::openAndPauseGame()
{
    if(App_GameLoaded() && !clientPaused)
    {
        Con_Execute(CMDS_DDAY, "pause", true, false);
    }
    open();
}

void TaskBarWidget::close()
{
    if(d->opened)
    {
        d->opened = false;

        setBehavior(DisableEventDispatchToChildren);

        // Slide the task bar down.
        d->vertShift->set(rule().height().valuei() +
                          style().rules().rule("unit").valuei(), OPEN_CLOSE_SPAN);
        setOpacity(0, OPEN_CLOSE_SPAN);

        d->console->closeLog();
        d->console->closeMenu();
        d->console->commandLine().dismissContentToHistory();
        closeMainMenu();
        closeConfigMenu();

        // Clear focus now; callbacks/signal handlers may set the focus elsewhere.
        if(hasRoot()) root().setFocus(0);

        emit closed();

        // Retrap the mouse if it was trapped when opening.
        if(hasRoot() && App_GameLoaded() && !root().window().as<ClientWindow>().hasSidebar())
        {
            Canvas &canvas = root().window().canvas();
            if(d->mouseWasTrappedWhenOpening)
            {
                canvas.trapMouse();
            }
        }
    }
}

void TaskBarWidget::openConfigMenu()
{
    d->mainMenu->close(0);
    d->configMenu->open();
}

void TaskBarWidget::closeConfigMenu()
{
    d->configMenu->close();
}

void TaskBarWidget::openMainMenu()
{
    d->configMenu->close(0);
    d->mainMenu->open();
}

void TaskBarWidget::closeMainMenu()
{
    d->mainMenu->close();
}

void TaskBarWidget::openMultiplayerMenu()
{
    d->multiMenu->open();
}

void TaskBarWidget::unloadGame()
{
    Con_Execute(CMDS_DDAY, "unload", false, false);
    d->mainMenu->close();
}

void TaskBarWidget::showAbout()
{
    AboutDialog *about = new AboutDialog;
    about->setDeleteAfterDismissed(true);
    root().addOnTop(about);
    about->open();
}

void TaskBarWidget::showUpdaterSettings()
{
    /// @todo This has actually little to do with the taskbar. -jk
    UpdaterSettingsDialog *dlg = new UpdaterSettingsDialog(UpdaterSettingsDialog::WithApplyAndCheckButton);
    dlg->setDeleteAfterDismissed(true);
    root().addOnTop(dlg);
    dlg->open();
}

void TaskBarWidget::switchGame()
{
    GamesDialog *games = new GamesDialog(GamesDialog::ShowSingleplayerOnly);
    games->setDeleteAfterDismissed(true);
    games->exec(root());
}

void TaskBarWidget::showMultiplayer()
{
    GamesDialog *games = new GamesDialog(GamesDialog::ShowMultiplayerOnly);
    games->setDeleteAfterDismissed(true);
    if(isOpen())
    {
        games->exec(root());
    }
    else
    {
        root().addOnTop(games);
        games->open();
    }
}

void TaskBarWidget::connectToServerManually()
{
    ManualConnectionDialog *dlg = new ManualConnectionDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->exec(root());
}

void TaskBarWidget::showTutorial()
{
    if(BusyMode_Active()) return;

    // The widget will dispose of itself when finished.
    TutorialWidget *tutorial = new TutorialWidget;
    root().addOnTop(tutorial);
    tutorial->rule().setRect(root().viewRule());
    tutorial->start();
}

void TaskBarWidget::updateCommandLineLayout()
{
    SequentialLayout layout(rule().right(), rule().top(), ui::Left);
    layout << *d->logo << *d->conf;

    if(!d->multi->behavior().testFlag(Hidden))
    {
        layout << *d->multi;
    }
    if(!d->status->behavior().testFlag(Hidden))
    {
        layout << *d->status;
    }

    // The command line extends the rest of the way.
    RuleRectangle &cmdRule = d->console->commandLine().rule();
    cmdRule.setInput(Rule::Left,   d->console->buttons().rule().right())
           .setInput(Rule::Bottom, rule().bottom())
           .setInput(Rule::Right,  layout.widgets().last()->as<GuiWidget>().rule().left());

    // Just use a plain background for this editor.
    d->console->commandLine().set(Background(style().colors().colorf("background")));
}
