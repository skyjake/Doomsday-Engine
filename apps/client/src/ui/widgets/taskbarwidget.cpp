/** @file taskbarwidget.cpp
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/commandaction.h"
#include "clientapp.h"
#include "dd_main.h"
#include "network/serverlink.h"
#include "ui/clientrootwidget.h"
#include "ui/clientwindow.h"
#include "ui/dialogs/aboutdialog.h"
#include "ui/dialogs/audiosettingsdialog.h"
#include "ui/dialogs/datafilesettingsdialog.h"
#include "ui/dialogs/inputsettingsdialog.h"
#include "ui/dialogs/manualconnectiondialog.h"
#include "ui/dialogs/networksettingsdialog.h"
#include "ui/dialogs/renderersettingsdialog.h"
#include "ui/dialogs/uisettingsdialog.h"
#include "ui/dialogs/videosettingsdialog.h"
#include "ui/dialogs/vrsettingsdialog.h"
#include "ui/home/homewidget.h"
#include "ui/inputsystem.h"
#include "ui/progress.h"
#include "ui/ui_main.h"
#include "ui/widgets/consolecommandwidget.h"
#include "ui/widgets/multiplayerstatuswidget.h"
#include "ui/widgets/packagessidebarwidget.h"
#include "ui/widgets/tutorialwidget.h"
#include "updater/updatersettingsdialog.h"

#include <doomsday/filesys/fs_main.h>
#include <doomsday/console/exec.h>

#include <de/animationrule.h>
#include <de/blurwidget.h>
#include <de/buttonwidget.h>
#include <de/callbackaction.h>
#include <de/config.h>
#include <de/directorylistdialog.h>
#include <de/drawable.h>
#include <de/glbuffer.h>
#include <de/keyevent.h>
#include <de/painter.h>
#include <de/popupmenuwidget.h>
#include <de/sequentiallayout.h>
#include <de/ui/subwidgetitem.h>

using namespace de;
using namespace de::ui;

static constexpr TimeSpan OPEN_CLOSE_SPAN = 200_ms;

enum MenuItemPositions
{
    // DE menu:
    POS_HOME              = 1,
    POS_CONNECT           = 2,
    POS_GAMES_SEPARATOR   = 3,
    POS_UNLOAD            = 4,
    POS_PACKAGES          = 7,
    POS_PACKAGES_NOTE     = 8,

    // Config menu:
    POS_RENDERER_SETTINGS = 0,
    POS_3D_VR_SETTINGS    = 1,
    POS_CONFIG_SEPARATOR  = 2,
    POS_AUDIO_SETTINGS    = 4,
    POS_INPUT_SETTINGS    = 5,
    POS_DATA_FILES        = 8,
    POS_UI_SETTINGS       = 9,
};

DE_GUI_PIMPL(TaskBarWidget)
, DE_OBSERVES(DoomsdayApp, GameChange)
, DE_OBSERVES(ServerLink, Join)
, DE_OBSERVES(ServerLink, Leave)
, DE_OBSERVES(PanelWidget, AboutToOpen) // update menu
{
    typedef DefaultVertexBuf VertexBuf;

    enum LayoutMode {
        NormalLayout,           ///< Taskbar widgets are full-sized.
        CompressedLayout,       ///< Cull some redundant information.
        ExtraCompressedLayout   ///< Hide current game indicator.
    };
    LayoutMode layoutMode;

    bool opened;

    GuiWidget *backBlur;
    ConsoleWidget *console;
    PopupButtonWidget *logo;
    PopupButtonWidget *conf;
    PopupButtonWidget *multi;
    LabelWidget *status;
    PopupMenuWidget *mainMenu;
    PopupMenuWidget *configMenu;
    MultiplayerStatusWidget *multiMenu;

    AnimationRule *vertShift;
    bool mouseWasTrappedWhenOpening;
    int minSpace;
    int maxSpace;

    // GL objects:
    //GuiVertexBuilder verts;
    //Drawable drawable;
    //GLUniform uMvpMatrix;
    //GLUniform uColor;
    //Mat4f projMatrix;

    Impl(Public *i)
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
        //, uMvpMatrix("uMvpMatrix", GLUniform::Mat4)
        //, uColor    ("uColor",     GLUniform::Vec4)
    {
        //uColor = Vec4f(1);
        self().set(Background(style().colors().colorf("background")));

        vertShift = new AnimationRule(0);

        DoomsdayApp::app().audienceForGameChange() += this;
        ClientApp::serverLink().audienceForJoin()  += this;
        ClientApp::serverLink().audienceForLeave() += this;

        updateStyle();
    }

    ~Impl()
    {
        releaseRef(vertShift);
    }

    void updateStyle()
    {
        minSpace = rule("console.commandline.width.min").valuei();
        maxSpace = rule("console.commandline.width.max").valuei();
    }

    void updateLayoutMode()
    {
        LayoutMode wanted = layoutMode;

        // Does the command line have enough space?
        if (console->commandLine().rule().width().valuei() < minSpace)
        {
            wanted = (layoutMode == NormalLayout?     CompressedLayout :
                      layoutMode == CompressedLayout? ExtraCompressedLayout :
                                                      layoutMode);
        }
        else if (console->commandLine().rule().width().valuei() > maxSpace)
        {
            wanted = (layoutMode == CompressedLayout?      NormalLayout :
                      layoutMode == ExtraCompressedLayout? CompressedLayout :
                                                           layoutMode);
        }

        if (layoutMode != wanted)
        {
            layoutMode = wanted;
            updateLogoButtonText();

            // Adjust widget visibility and rules.
            switch (layoutMode)
            {
            case NormalLayout:
            case CompressedLayout:
                status->show();
                break;

            case ExtraCompressedLayout:
                status->hide();
                break;
            }

            self().updateCommandLineLayout();
            self().requestGeometry();
            console->commandLine().requestGeometry();
        }
    }

    void glInit()
    {
        //drawable.addBuffer(new VertexBuf);

        /*shaders().build(drawable.program(), "generic.color_ucolor")
                << uMvpMatrix
                << uColor;

        updateProjection();*/
    }

    void glDeinit()
    {
        //drawable.clear();
        //verts.clear();
    }

    void updateLogoButtonText()
    {
        String text;

        if (layoutMode == NormalLayout)
        {
            const Version currentVersion = Version::currentBuild();
            if (String(DOOMSDAY_RELEASE_TYPE) == "Stable")
            {
                text = _E(b) + currentVersion.compactNumber();
            }
            else
            {
                text = _E(b) + currentVersion.compactNumber() + " " +
                       _E(l) + Stringf("#%i", currentVersion.build);
            }
        }
        else
        {
            // Remove the version number if we're short of space.
        }

        logo->setText(text);
    }

    /*void updateProjection()
    {
        uMvpMatrix = root().projMatrix2D();
    }*/

    void updateGeometry()
    {
        /*
        Rectanglei pos;
        if (self().hasChangedPlace(pos) || self().geometryRequested())
        {
            self().requestGeometry(false);

            verts.clear();
            self().glMakeGeometry(verts);
            //drawable.buffer<VertexBuf>().setVertices(gfx::TriangleStrip, verts, gfx::Static);
        }*/
    }

    GuiWidget &itemWidget(PopupMenuWidget *menu, uint pos) const
    {
        return *menu->menu().organizer().itemWidget(pos);
    }

    void showOrHideMenuItems()
    {
        const Game &game = App_CurrentGame();

        //itemWidget(mainMenu, POS_GAMES)            .show(!game.isNull());
        itemWidget(mainMenu, POS_UNLOAD)           .show(!game.isNull());
        itemWidget(mainMenu, POS_GAMES_SEPARATOR)  .show(!game.isNull());
        itemWidget(mainMenu, POS_PACKAGES)         .show(!game.isNull());
        itemWidget(mainMenu, POS_PACKAGES_NOTE)    .show(!game.isNull());
        //itemWidget(mainMenu, POS_IWAD_FOLDER)      .show(game.isNull());
        itemWidget(mainMenu, POS_HOME)             .show(!game.isNull());
        //itemWidget(mainMenu, POS_CONNECT)          .show(game.isNull());

        itemWidget(configMenu, POS_RENDERER_SETTINGS).show(!game.isNull());
        itemWidget(configMenu, POS_3D_VR_SETTINGS)   .show(!game.isNull());
        itemWidget(configMenu, POS_CONFIG_SEPARATOR) .show(!game.isNull());
        //itemWidget(configMenu, POS_AUDIO_SETTINGS)   .show(!game.isNull());
        itemWidget(configMenu, POS_INPUT_SETTINGS)   .show(!game.isNull());
        itemWidget(configMenu, POS_DATA_FILES)       .show(game.isNull());

        if (self().hasRoot())
        {
            configMenu->menu().updateLayout();
            mainMenu->menu().updateLayout(); // Include/exclude shown/hidden menu items.
        }
    }

    void currentGameChanged(const Game &)
    {
        updateStatus();
        showOrHideMenuItems();
        backBlur->show(style().isBlurringAllowed());
    }

    void networkGameJoined()
    {
        multi->show();
        self().updateCommandLineLayout();
    }

    void networkGameLeft()
    {
        multi->hide();
        self().updateCommandLineLayout();
    }

    void updateStatus()
    {
        if (const auto *prof = DoomsdayApp::currentGameProfile())
        {
            status->setText(prof->name().truncateWithEllipsis(30));
        }
        else if (App_GameLoaded())
        {
            status->setText(App_CurrentGame().id());
        }
        else
        {
            status->setText("No game loaded");
        }
    }

    void panelAboutToOpen(PanelWidget &)
    {
        if (self().root().window().as<ClientWindow>().isGameMinimized())
        {
            mainMenu->items().at(POS_HOME).setLabel("Hide Home");
        }
        else
        {
            mainMenu->items().at(POS_HOME).setLabel("Show Home");
        }
    }

    DE_PIMPL_AUDIENCES(Open, Close)
};

DE_AUDIENCE_METHODS(TaskBarWidget, Open, Close)

#if defined (DE_HAVE_UPDATER)
static PopupWidget *makeUpdaterSettings()
{
    return new UpdaterSettingsDialog(UpdaterSettingsDialog::WithApplyAndCheckButton);
}
#endif

TaskBarWidget::TaskBarWidget() : GuiWidget("taskbar"), d(new Impl(this))
{
    Background bg(style().colors().colorf("background"));

    const Rule &gap = rule("gap");

    d->backBlur = new LabelWidget;
    d->backBlur->rule()
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Bottom, rule().bottom())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Top,    rule().top());
    add(d->backBlur);

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
    d->logo = new PopupButtonWidget("de-button");
    d->logo->setImage(style().images().image("logo.px128"));
    d->logo->setImageScale(.475f);
    d->logo->setImageFit(FitToHeight | OriginalAspectRatio);

    d->updateLogoButtonText();

    d->logo->setWidthPolicy(ui::Expand);
    d->logo->setTextAlignment(AlignLeft);
    d->logo->rule().setInput(Rule::Height, rule().height());
    add(d->logo);

    // Settings.
    d->conf = new PopupButtonWidget("conf-button");
    d->conf->setImage(style().images().image("gear"));
    d->conf->setSizePolicy(ui::Expand, ui::Filled);
    d->conf->rule().setInput(Rule::Height, rule().height());
    add(d->conf);

    // Currently loaded game.
    d->status = new LabelWidget;
    d->status->set(bg);
    d->status->setWidthPolicy(ui::Expand);
    d->status->rule().setInput(Rule::Height, rule().height());
    add(d->status);

    d->updateStatus();

    // Multiplayer.
    d->multi = new PopupButtonWidget;
    d->multi->hide(); // hidden when not connected
    d->multi->setImage(style().images().image("network"));
    d->multi->setTextAlignment(ui::AlignRight);
    d->multi->setText("MP");
    d->multi->setSizePolicy(ui::Expand, ui::Filled);
    d->multi->rule().setInput(Rule::Height, rule().height());
    add(d->multi);

    // Taskbar height depends on the font size.
    rule().setInput(Rule::Height, style().fonts().font("default").height() + gap * 2);

    // Settings menu.
    add(d->configMenu = new PopupMenuWidget("conf-menu"));
    d->conf->setPopup(*d->configMenu);

    // Multiplayer menu.
    add(d->multiMenu = new MultiplayerStatusWidget);
    d->multi->setPopup(*d->multiMenu);

    // The DE menu.
    add(d->mainMenu = new PopupMenuWidget("de-menu"));
    d->mainMenu->audienceForAboutToOpen() += d;
    d->logo->setPopup(*d->mainMenu);

    // Game unloading confirmation submenu.
    auto *unloadMenu = new ui::SubmenuItem(style().images().image("close.ring"),
                                           "Unload Game", ui::Left);
    unloadMenu->items()
            << new ui::Item(ui::Item::Separator, "Really unload the game?")
            << new ui::ActionItem("Unload " _E(b) "(discard progress)", [this]() { unloadGame(); })
            << new ui::ActionItem("Cancel", [this]() { d->mainMenu->menu().dismissPopups(); });

    /*
     * Set up items for the config and DE menus. Some of these are shown/hidden
     * depending on whether a game is loaded.
     */
    d->configMenu->items()
            << new ui::SubwidgetItem(style().images().image("renderer"),  "Renderer",       ui::Left, makePopup<RendererSettingsDialog>)
            << new ui::SubwidgetItem(style().images().image("vr"),        "3D & VR",        ui::Left, makePopup<VRSettingsDialog>)
            << new ui::Item(ui::Item::Separator)
            << new ui::SubwidgetItem(style().images().image("display"),   "Video",          ui::Left, makePopup<VideoSettingsDialog>)
            << new ui::SubwidgetItem(style().images().image("audio"),     "Audio",          ui::Left, makePopup<AudioSettingsDialog>)
            << new ui::SubwidgetItem(style().images().image("input"),     "Input",          ui::Left, makePopup<InputSettingsDialog>)
            << new ui::SubwidgetItem(style().images().image("network"),   "Network",        ui::Left, makePopup<NetworkSettingsDialog>)
            << new ui::Item(ui::Item::Separator)
            << new ui::SubwidgetItem(style().images().image("package.icon"), "Data Files",     ui::Left, makePopup<DataFileSettingsDialog>)
            << new ui::SubwidgetItem(style().images().image("home.icon"), "User Interface", ui::Left, makePopup<UISettingsDialog>);
#if defined (DE_HAVE_UPDATER)
    d->configMenu->items()
            << new ui::SubwidgetItem(style().images().image("updater"),   "Updater",        ui::Left, makeUpdaterSettings);
#endif

    auto *helpMenu = new ui::SubmenuItem("Help", ui::Left);
    helpMenu->items()
            << new ui::ActionItem("Doomsday Manual...",
                              new CallbackAction([]() {
                                  ClientApp::app().openInBrowser("https://manual.dengine.net/");
                              }))
            << new ui::ActionItem("Show Tutorial", [this]() { showTutorial(); });
            //<< new ui::VariableToggleItem("Menu Annotations", App::config("ui.showAnnotations"))

    d->mainMenu->items()
            << new ui::Item(ui::Item::Separator, "Games")
            << new ui::ActionItem(style().images().image("home.icon"), "",
                                  [this]() { showOrHideHome(); })
            << new ui::ActionItem("Connect to Server...", [this]() { connectToServerManually(); })
            << new ui::Item(ui::Item::Separator)
            << unloadMenu                           // hidden with null-game
            << new ui::Item(ui::Item::Separator)
            << new ui::Item(ui::Item::Separator, "Resources")
            << new ui::ActionItem("Browse Mods...", [this]() { openPackagesSidebar(); })
            << new ui::Item(ui::Item::Annotation,
                            "Load/unload data files and view package information.")
            << new ui::ActionItem("Clear Cache", []() { DoomsdayApp::app().clearCache(); })
            << new ui::Item(ui::Item::Annotation,
                            "Forces a refresh of resource file metadata.")
            << new ui::Item(ui::Item::Separator)
            << new ui::Item(ui::Item::Separator, "Doomsday")
#if defined (DE_HAVE_UPDATER)
            << new ui::ActionItem("Check for Updates", new CommandAction("updateandnotify"))
#endif
            << new ui::ActionItem("About Doomsday", [this]() { showAbout(); })
            << helpMenu
#if !defined (DE_MOBILE)
            << new ui::Item(ui::Item::Separator)
            << new ui::ActionItem("Quit Doomsday", new CommandAction("quit!"))
#endif
            ;

    d->showOrHideMenuItems();

    // Set the initial command line layout.
    updateCommandLineLayout();

    d->console->audienceForCommandMode() += [this]() { updateCommandLineLayout(); };
    d->console->audienceForGotFocus() += [this]() { closeMainMenu(); closeConfigMenu(); };

    // Initially closed.
    close();
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

const Rule &TaskBarWidget::shift()
{
    return *d->vertShift;
}

void TaskBarWidget::initialize()
{
    GuiWidget::initialize();

    if (style().isBlurringAllowed())
    {
        d->backBlur->set(Background(root().window().as<ClientWindow>().taskBarBlur(), Vec4f(1)));
    }
    else
    {
        d->backBlur->set(Background(Vec4f(0, 0, 0, 1)));
    }
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

/*void TaskBarWidget::viewResized()
{
    GuiWidget::viewResized();
    //d->updateProjection();
}*/

void TaskBarWidget::update()
{
    GuiWidget::update();
    d->updateLayoutMode();
}

void TaskBarWidget::drawContent()
{
    d->updateGeometry();
}

bool TaskBarWidget::handleEvent(const Event &event)
{
    ClientWindow &window = root().window().as<ClientWindow>();

    if (!window.eventHandler().isMouseTrapped()
            && event.type() == Event::MouseButton
            && !window.hasSidebar()
            && !window.isGameMinimized())
    {
        // Clicking outside the taskbar will trap the mouse automatically.
        const MouseEvent &mouse = event.as<MouseEvent>();
        if (mouse.state() == MouseEvent::Released && !hitTest(mouse.pos()))
        {
            /*if (root().focus())
            {
                // First click will remove UI focus, allowing GameWidget
                // to receive events.
                root().setFocus(0);
                return true;
            }*/

            if (App_GameLoaded())
            {
                // Allow game to use the mouse.
                window.eventHandler().trapMouse();
            }

            window.taskBar().close();
            return true;
        }
    }

    if (event.type() == Event::MouseButton)
    {
        // Eat all button events occurring inside the task bar area.
        if (hitTest(event))
        {
            return true;
        }
    }

    // Don't let modifier keys fall through to the game.
    if (isOpen() && event.isKey() && event.as<KeyEvent>().isModifier())
    {
        // However, let the bindings system know about the modifier state.
        ClientApp::input().trackEvent(event);
        return true;
    }

    if (event.type() == Event::KeyPress)
    {
        const KeyEvent &key = event.as<KeyEvent>();

        // Shift-Esc opens and closes the task bar.
        if (key.ddKey() == DDKEY_ESCAPE)
        {
            if (isOpen())
            {
                // First press of Esc will just dismiss the console.
                if (d->console->isLogOpen() && !key.modifiers().testFlag(KeyEvent::Shift))
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
                if (key.modifiers().testFlag(KeyEvent::Shift) ||
                   !App_GameLoaded())
                {
                    if (!window.hasSidebar() && !window.isGameMinimized())
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
    if (!d->opened)
    {
        d->opened = true;

        unsetBehavior(DisableEventDispatchToChildren);

        d->console->zeroLogHeight();

        d->vertShift->set(0, OPEN_CLOSE_SPAN);
        setOpacity(1, OPEN_CLOSE_SPAN);

        DE_NOTIFY(Open, i) i->taskBarOpened();
    }

    // Untrap the mouse if it is trapped.
    if (hasRoot())
    {
        auto &handler = root().window().eventHandler();
        d->mouseWasTrappedWhenOpening = handler.isMouseTrapped();
        if (handler.isMouseTrapped())
        {
            handler.trapMouse(false);
        }

        if (!App_GameLoaded())
        {
            root().setFocus(&d->console->commandLine());
        }
    }
}

void TaskBarWidget::openAndPauseGame()
{
    root().window().as<ClientWindow>().game().pause();
    open();
}

void TaskBarWidget::close()
{
    if (d->opened)
    {
        d->opened = false;

        setBehavior(DisableEventDispatchToChildren);

        // Slide the task bar down.
        d->vertShift->set(rule().height().valuei() +
                          rule(RuleBank::UNIT).valuei(), OPEN_CLOSE_SPAN);
        setOpacity(0, OPEN_CLOSE_SPAN);

        d->console->closeLog();
        d->console->closeMenu();
        d->console->commandLine().dismissContentToHistory();
        closeMainMenu();
        closeConfigMenu();

        // Clear focus now; callbacks/signal handlers may set the focus elsewhere.
        if (hasRoot()) root().setFocus(0);

        DE_NOTIFY(Close, i) i->taskBarClosed();

        // Retrap the mouse if it was trapped when opening.
        if (hasRoot())
        {
            auto &window = root().window().as<ClientWindow>();
            if (App_GameLoaded() && !window.hasSidebar() && !window.isGameMinimized())
            {
                if (d->mouseWasTrappedWhenOpening)
                {
                    window.eventHandler().trapMouse();
                }
            }
        }
    }
}

void TaskBarWidget::openConfigMenu()
{
    d->mainMenu->close(0.0);
    d->configMenu->open();
}

void TaskBarWidget::closeConfigMenu()
{
    d->configMenu->close();
}

void TaskBarWidget::openMainMenu()
{
    d->configMenu->close(0.0);
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
    root().window().glActivate();

    AboutDialog *about = new AboutDialog;
    about->setDeleteAfterDismissed(true);
    root().addOnTop(about);
    about->open();

    root().window().glDone();
}

#if defined (DE_HAVE_UPDATER)
void TaskBarWidget::showUpdaterSettings()
{
    /// @todo This has actually little to do with the taskbar. -jk
    UpdaterSettingsDialog *dlg = new UpdaterSettingsDialog(UpdaterSettingsDialog::WithApplyAndCheckButton);
    dlg->setDeleteAfterDismissed(true);
    root().addOnTop(dlg);
    dlg->open();
}
#endif

void TaskBarWidget::showOrHideHome()
{
    DE_ASSERT(App_GameLoaded());

    // Minimize the game, switch to MP column in Home.
    auto &win = ClientWindow::main();
    if (!win.isGameMinimized())
    {
        win.game().pause();
        win.setGameMinimized(true);
        win.home().moveOnscreen(1.0);
        close();
    }
    else
    {
        win.home().moveOffscreen(1.0);
    }
}

void TaskBarWidget::connectToServerManually()
{
    auto *dlg = new ManualConnectionDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->exec(root());
}

void TaskBarWidget::showTutorial()
{
    if (BusyMode_Active()) return;

    d->mainMenu->close();

    // The widget will dispose of itself when finished.
    TutorialWidget *tutorial = new TutorialWidget;
    root().addOnTop(tutorial);
    tutorial->rule().setRect(root().viewRule());
    tutorial->start();
}

void TaskBarWidget::openPackagesSidebar()
{
    auto *sidebar = new PackagesSidebarWidget; // ClientWindow gets ownership
    sidebar->open();
}

void TaskBarWidget::updateCommandLineLayout()
{
    SequentialLayout layout(rule().right(), rule().top(), ui::Left);
    layout << *d->logo << *d->conf;

    if (!d->multi->behavior().testFlag(Hidden))
    {
        layout << *d->multi;
    }
    if (!d->status->behavior().testFlag(Hidden))
    {
        layout << *d->status;
    }

    // The command line extends the rest of the way.
    RuleRectangle &cmdRule = d->console->commandLine().rule();
    cmdRule.setInput(Rule::Left,   d->console->buttons().rule().right())
           .setInput(Rule::Bottom, rule().bottom())
           .setInput(Rule::Right,  layout.widgets().last()->rule().left());

    // Just use a plain background for this editor.
    d->console->commandLine().set(Background(style().colors().colorf("background")));
}
