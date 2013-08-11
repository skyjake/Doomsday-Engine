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
#include "ui/widgets/guirootwidget.h"
#include "ui/widgets/labelwidget.h"
#include "ui/widgets/buttonwidget.h"
#include "ui/widgets/consolecommandwidget.h"
#include "ui/widgets/popupmenuwidget.h"
#include "ui/widgets/variabletogglewidget.h"
#include "ui/widgets/blurwidget.h"
#include "ui/clientwindow.h"
#include "ui/commandaction.h"
#include "ui/signalaction.h"

#include "ui/ui_main.h"
#include "con_main.h"

#include <de/App>
#include <de/KeyEvent>
#include <de/Drawable>
#include <de/GLBuffer>
#include <de/ScalarRule>

#include "../../updater/versioninfo.h"
#include "dd_main.h"

using namespace de;
using namespace ui;

static TimeDelta OPEN_CLOSE_SPAN = 0.2;

DENG2_PIMPL(TaskBarWidget),
public IGameChangeObserver
{
    typedef DefaultVertexBuf VertexBuf;

    bool opened;
    ConsoleWidget *console;
    ButtonWidget *logo;
    LabelWidget *status;
    PopupMenuWidget *mainMenu;
    PopupMenuWidget *unloadMenu;
    ButtonWidget *panelItem;
    ButtonWidget *unloadItem;
    ScalarRule *vertShift;

    QScopedPointer<Action> openAction;
    QScopedPointer<Action> closeAction;
    bool mouseWasTrappedWhenOpening;

    // GL objects:
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    Matrix4f projMatrix;

    Instance(Public *i)
        : Base(i),
          opened(true),
          status(0),
          mainMenu(0),
          mouseWasTrappedWhenOpening(false),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4)
    {
        uColor = Vector4f(1, 1, 1, 1);
        self.set(Background(self.style().colors().colorf("background")));

        vertShift = new ScalarRule(0);

        audienceForGameChange += this;
    }

    ~Instance()
    {
        audienceForGameChange -= this;

        releaseRef(vertShift);
    }

    void glInit()
    {
        drawable.addBuffer(new VertexBuf);

        self.root().shaders().build(drawable.program(), "generic.color_ucolor")
                << uMvpMatrix
                << uColor;

        updateProjection();
    }

    void glDeinit()
    {
        drawable.clear();
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

    void updateProjection()
    {
        uMvpMatrix = self.root().projMatrix2D();
    }

    void currentGameChanged(Game &newGame)
    {
        updateStatus();

        if(!isNullGame(newGame))
        {
            panelItem->show();
            unloadItem->show();
        }
        else
        {
            panelItem->hide();
            unloadItem->hide();
        }
        mainMenu->menu().updateLayout(); // Include/exclude shown/hidden menu items.
    }

    void updateStatus()
    {
        if(App_GameLoaded())
        {
            status->setText(Str_Text(App_CurrentGame().identityKey()));
        }
        else
        {
            status->setText(tr("No game loaded"));
        }
    }

    void updateMenuItems()
    {
    }
};

TaskBarWidget::TaskBarWidget() : GuiWidget("taskbar"), d(new Instance(this))
{
#if 0
    // LegacyWidget is presently incompatible with blurring.
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
    d->console->button().rule()
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Width,  d->console->button().rule().height())
            .setInput(Rule::Bottom, rule().bottom())
            .setInput(Rule::Height, rule().height());

    d->console->commandLine().rule()
            .setInput(Rule::Left,   d->console->button().rule().right())
            .setInput(Rule::Bottom, rule().bottom());

    // DE logo.
    d->logo = new ButtonWidget;
    //d->logo->setAction(new CommandAction("panel"));
    d->logo->setImage(style().images().image("logo.px128"));
    d->logo->setImageScale(.475f);
    d->logo->setImageFit(FitToHeight | OriginalAspectRatio);

    VersionInfo currentVersion;
    if(String(DOOMSDAY_RELEASE_TYPE) == "Stable")
    {
        d->logo->setText(_E(b) + currentVersion.base());
    }
    else
    {
        d->logo->setText(_E(b) + currentVersion.base() + " " +
                         _E(l) + String("#%1").arg(currentVersion.build));
    }

    d->logo->setWidthPolicy(ui::Expand);
    d->logo->setTextAlignment(AlignLeft);
    d->logo->rule()
            .setInput(Rule::Height, rule().height())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, rule().bottom());
    add(d->logo);

    // Currently loaded game.
    d->status = new LabelWidget;
    d->status->set(bg);
    d->status->setWidthPolicy(ui::Expand);
    d->status->rule()
            .setInput(Rule::Height, rule().height())
            .setInput(Rule::Bottom, rule().bottom())
            .setInput(Rule::Right,  d->logo->rule().left());
    add(d->status);        

    // The command line extends all the way to the status indicator.
    d->console->commandLine().rule().setInput(Rule::Right, d->status->rule().left());

    d->updateStatus();

    // Taskbar height depends on the font size.
    rule().setInput(Rule::Height, style().fonts().font("default").height() + gap * 2);

    // The DE menu.
    d->mainMenu = new PopupMenuWidget("de-menu");
    d->mainMenu->setAnchor(d->logo->rule().left() + d->logo->rule().width() / 2,
                           d->logo->rule().top());

    // Set up items for the DE menu. Some of these are shown/hidden
    // depending on whether a game is loaded.
    d->panelItem = d->mainMenu->addItem(_E(b) + tr("Open Control Panel"), new CommandAction("panel"));
    d->mainMenu->addItem(tr("Toggle Fullscreen"), new CommandAction("togglefullscreen"));
    d->mainMenu->addItem(new VariableToggleWidget(tr("Show FPS"), App::config()["window.main.showFps"]));
    d->unloadItem = d->mainMenu->addItem(tr("Unload Game"), new SignalAction(this, SLOT(confirmUnloadGame())), false);
    d->mainMenu->addSeparator();
    d->mainMenu->addItem(tr("Check for Updates..."), new CommandAction("updateandnotify"));
    d->mainMenu->addItem(tr("Updater Settings..."), new CommandAction("updatesettings"));
    d->mainMenu->addSeparator();
    d->mainMenu->addItem(tr("Quit Doomsday"), new CommandAction("quit"));
    add(d->mainMenu);

    // Confirmation for unloading game.
    d->unloadMenu = new PopupMenuWidget("unload-menu");
    d->unloadMenu->setOpeningDirection(ui::Left);
    d->unloadMenu->setAnchor(d->mainMenu->rule().left(),
                             d->unloadItem->rule().top() + d->unloadItem->rule().height() / 2);
    d->unloadMenu->addSeparator(tr("Really unload the game?"));
    d->unloadMenu->addItem(tr("Unload") + " "_E(b) + tr("(discard progress)"), new SignalAction(this, SLOT(unloadGame())));
    d->unloadMenu->addItem(tr("Cancel"), new Action);
    add(d->unloadMenu);

    d->panelItem->hide();
    d->unloadItem->hide();
    d->updateMenuItems();

    d->logo->setAction(new SignalAction(this, SLOT(openMainMenu())));
}

ConsoleWidget &TaskBarWidget::console()
{
    return *d->console;
}

ConsoleCommandWidget &TaskBarWidget::commandLine()
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

void TaskBarWidget::setOpeningAction(Action *action)
{
    d->openAction.reset(action);
}

void TaskBarWidget::setClosingAction(Action *action)
{
    d->closeAction.reset(action);
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
    d->updateProjection();
}

void TaskBarWidget::drawContent()
{
    d->updateGeometry();
    //d->drawable.draw();
}

bool TaskBarWidget::handleEvent(Event const &event)
{
    Canvas &canvas = root().window().canvas();

    if(!canvas.isMouseTrapped() && event.type() == Event::MouseButton)
    {
        // Clicking outside the taskbar will trap the mouse automatically.
        MouseEvent const &mouse = event.as<MouseEvent>();
        if(mouse.state() == MouseEvent::Released && !hitTest(mouse.pos()))
        {
            if(root().focus())
            {
                // First click will remove UI focus, allowing LegacyWidget
                // to receive events.
                root().setFocus(0);
                return true;
            }

            if(App_GameLoaded())
            {
                // Allow game to use the mouse.
                canvas.trapMouse();
            }

            root().window().taskBar().close();
            return true;
        }
    }

    if(event.type() == Event::KeyPress)
    {
        KeyEvent const &key = event.as<KeyEvent>();

        // Shift-Esc opens and closes the task bar.
        if(key.ddKey() == DDKEY_ESCAPE)
        {
#if 0
            // Shift-Esc opens the console.
            if()
            {
                root().setFocus(&d->console->commandLine());
                if(!isOpen()) open(false /* no action */);
                return true;
            }
#endif
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
            else if(!UI_IsActive()) /// @todo Play nice with legacy engine UI (which is deprecated).
            {
                // Task bar is closed, so let's open it.
                if(key.modifiers().testFlag(KeyEvent::Shift) ||
                   !App_GameLoaded())
                {
                    // Automatically focus the command line.
                    root().setFocus(&d->console->commandLine());

                    open();
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

void TaskBarWidget::open(bool doAction)
{
    if(!d->opened)
    {
        d->opened = true;

        unsetBehavior(DisableEventDispatchToChildren);

        d->console->clearLog();

        d->vertShift->set(0, OPEN_CLOSE_SPAN);
        setOpacity(1, OPEN_CLOSE_SPAN);

        emit opened();

        if(doAction)
        {
            if(!d->openAction.isNull())
            {
                d->openAction->trigger();
            }
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
        d->mainMenu->close();

        // Clear focus now; callbacks/signal handlers may set the focus elsewhere.
        if(hasRoot()) root().setFocus(0);

        emit closed();

        if(!d->closeAction.isNull())
        {
            d->closeAction->trigger();
        }

        // Retrap the mouse if it was trapped when opening.
        if(hasRoot() && App_GameLoaded())
        {
            Canvas &canvas = root().window().canvas();
            if(d->mouseWasTrappedWhenOpening)
            {
                canvas.trapMouse();
            }
        }
    }
}

void TaskBarWidget::openMainMenu()
{
    d->mainMenu->open();
}

void TaskBarWidget::confirmUnloadGame()
{
    d->unloadMenu->open();
}

void TaskBarWidget::unloadGame()
{
    Con_Execute(CMDS_DDAY, "unload", false, false);
    d->mainMenu->close();
}
