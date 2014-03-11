/** @file tutorialdialog.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/tutorialwidget.h"
#include "ui/clientwindow.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/inputbindingwidget.h"
#include "dd_version.h"
#include "dd_main.h"

#include <de/MessageDialog>
#include <de/SignalAction>
#include <de/Untrapper>

using namespace de;

static TimeDelta const FLASH_SPAN = 0.6;

DENG_GUI_PIMPL(TutorialWidget)
{
    enum Step {
        Welcome,
        TaskBar,
        DEMenu,
        ConfigMenus,
        ConsoleKey,
        Finish
    };

    Step current;
    //LabelWidget *darken;
    MessageDialog *dlg;
    LabelWidget *highlight;
    QTimer flashing;
    bool taskBarInitiallyOpen;
    Untrapper untrapper;

    Instance(Public *i)
        : Base(i)
        , current(Welcome)
        , dlg(0)
        , taskBarInitiallyOpen(ClientWindow::main().taskBar().isOpen())
        , untrapper(ClientWindow::main())
    {
        self.add(highlight = new LabelWidget);
        highlight->set(Background(Background::GradientFrame,
                                  style().colors().colorf("accent"), 6));
        highlight->setOpacity(0);

        flashing.setSingleShot(false);
        flashing.setInterval(FLASH_SPAN.asMilliSeconds());
    }

    void flash()
    {
        if(highlight->opacity().target() > .5f)
        {
            highlight->setOpacity(.2f, FLASH_SPAN);
        }
        else
        {
            highlight->setOpacity(.8f, FLASH_SPAN);
        }
    }

    void startHighlight(GuiWidget const &w)
    {
        highlight->rule().setRect(w.rule());
        highlight->setOpacity(.2f);
        highlight->show();
        flashing.start();
        flash();
    }

    void stopHighlight()
    {
        highlight->hide();
        flashing.stop();
    }

    void deinitStep()
    {
        if(dlg)
        {
            dlg->close(0);
            dlg = 0;
        }

        stopHighlight();

        ClientWindow &win = ClientWindow::main();
        switch(current)
        {
        case DEMenu:
            win.taskBar().closeMainMenu();
            break;

        case ConfigMenus:
            win.taskBar().closeConfigMenu();
            break;

        default:
            break;
        }
    }

    void initStep(Step s)
    {
        deinitStep();

        if(s == Finish)
        {
            self.stop();
            return;
        }

        current = s;
        dlg = new MessageDialog;
        dlg->useInfoStyle();
        dlg->setDeleteAfterDismissed(true);
        dlg->setClickToClose(false);
        QObject::connect(dlg, SIGNAL(accepted(int)), thisPublic, SLOT(continueToNextStep()));
        QObject::connect(dlg, SIGNAL(rejected(int)), thisPublic, SLOT(stop()));
        dlg->buttons()
                << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default,
                                        (current == Finish - 1)? tr("Done") : tr("Continue"))
                << new DialogButtonItem(DialogWidget::Reject | DialogWidget::Action,  tr("Skip Tutorial"));

        // Insert the content for the dialog.
        ClientWindow &win = ClientWindow::main();
        switch(current)
        {
        case Welcome:
            dlg->title().setText(tr("Welcome to Doomsday"));
            dlg->message().setText(tr("This tutorial will give you a brief walkthrough of the "
                                      "major features of Doomsday's UI. You will also get a "
                                      "chance to pick a shortcut key for opening the console.\n\n"
                                      "The tutorial can be restarted later via the application menu."));
                                   //.arg(_E(b) DOOMSDAY_NICENAME _E(.)));
            dlg->setAnchor(self.rule().midX(), self.rule().top());
            dlg->setOpeningDirection(ui::Down);
            break;

        case TaskBar:
            dlg->title().setText(tr("Task Bar"));
            dlg->message().setText(tr("The task bar is where you find all the important functionality: loading "
                                      "and switching games, joining a multiplayer game, "
                                      "configuration settings, "
                                      "and a console command line for advanced users.\n\n"
                                      "Press %1 to access the task bar at any time.")
                                   .arg(_E(b) "Shift-Esc" _E(.)));

            win.taskBar().open();
            win.taskBar().closeMainMenu();
            win.taskBar().closeConfigMenu();
            dlg->setAnchor(self.rule().midX(), win.taskBar().rule().top());
            dlg->setOpeningDirection(ui::Up);
            startHighlight(win.taskBar());
            break;

        case DEMenu:
            dlg->title().setText(tr("Application Menu"));
            dlg->message().setText(tr("Click the DE icon in the bottom right corner to open "
                                      "the application menu. "
                                      "You can check for available updates, switch games, or look for "
                                      "ongoing multiplayer games."));
            win.taskBar().openMainMenu();
            dlg->setAnchorAndOpeningDirection(root().guiFind("de-menu")->rule(), ui::Left);
            startHighlight(*root().guiFind("de-button"));
            break;

        case ConfigMenus:
            dlg->title().setText(tr("Settings"));
            dlg->message().setText(tr("Configuration menus are found under buttons with a gear icon. "
                                      "The task bar's configuration button has the settings for "
                                      "all of Doomsday's subsystems."));
            win.taskBar().openConfigMenu();
            dlg->setAnchorAndOpeningDirection(root().guiFind("conf-menu")->rule(), ui::Left);
            startHighlight(*root().guiFind("conf-button"));
            break;

        case ConsoleKey: {
            dlg->title().setText(tr("Console"));
            String msg = tr("The console is a \"Quake style\" command line prompt where "
                            "you enter commands and change variable values. To get started, "
                            "try typing %1 in the console.").arg(_E(b) "help" _E(.));
            if(App_GameLoaded())
            {
                msg += "\n\nBelow you can see the current keyboard shortcut for accessing the console quickly. "
                        "To change it, click in the box and then press the key or key combination you "
                        "want to assign as the shortcut.";
                InputBindingWidget *bind = InputBindingWidget::newTaskBarShortcut();
                bind->useInfoStyle();
                dlg->area().add(bind);
            }
            dlg->message().setText(msg);
            dlg->setAnchor(win.taskBar().console().commandLine().rule().left() +
                           style().rules().rule("gap"),
                           win.taskBar().rule().top());
            dlg->setOpeningDirection(ui::Up);
            dlg->updateLayout();
            startHighlight(win.taskBar().console().commandLine());
            break; }

        default:
            break;
        }

        self.root().addOnTop(dlg);
        dlg->open();
    }
};

TutorialWidget::TutorialWidget()
    : GuiWidget("tutorial"), d(new Instance(this))
{
    connect(&d->flashing, SIGNAL(timeout()), this, SLOT(flashHighlight()));
}

void TutorialWidget::start()
{
    // Blur the rest of the view.
    GuiWidget &blur = ClientWindow::main().taskBarBlur();
    blur.show();
    blur.setOpacity(0);
    blur.setOpacity(1, .5);

    d->initStep(Instance::Welcome);
}

void TutorialWidget::stop()
{
    if(!d->taskBarInitiallyOpen)
    {
        ClientWindow::main().taskBar().close();
    }

    d->deinitStep();

    // Animate away and unfade darkening.
    ClientWindow::main().taskBarBlur().setOpacity(0, .5);

    QTimer::singleShot(500, this, SLOT(dismiss()));
}

void TutorialWidget::dismiss()
{
    ClientWindow::main().taskBarBlur().hide();
    hide();
    guiDeleteLater();
}

void TutorialWidget::flashHighlight()
{
    d->flash();
}

bool TutorialWidget::handleEvent(Event const &event)
{
    GuiWidget::handleEvent(event);

    // Eat everything!
    return true;
}

void TutorialWidget::continueToNextStep()
{
    d->initStep(Instance::Step(d->current + 1));
}
