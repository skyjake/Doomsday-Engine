/** @file tutorialwidget.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <doomsday/doomsdayapp.h>

#include <de/messagedialog.h>
#include <de/untrapper.h>
#include <de/popupmenuwidget.h>
#include <de/progresswidget.h>
#include <de/notificationareawidget.h>
#include <de/styleproceduralimage.h>
#include <de/timer.h>

using namespace de;

static constexpr TimeSpan FLASH_SPAN = 600_ms;

DE_GUI_PIMPL(TutorialWidget)
{
    enum Step {
        Welcome,
        HomeScreen,
        Notifications,
        TaskBar,
        DEMenu,
        ConfigMenus,
        RendererAppearance,
        ConsoleKey,
        Finish
    };

    Step                         current   = Welcome;
    MessageDialog *              dlg       = nullptr;
    LabelWidget *                highlight = nullptr;
    NotificationAreaWidget *     notifs    = nullptr; ///< Fake notifications just for an example.
    UniqueWidgetPtr<LabelWidget> exampleAlert;
    Timer                        flashing;
    bool                         taskBarInitiallyOpen;
    Untrapper                    untrapper;

    Impl(Public *i)
        : Base(i)
        , taskBarInitiallyOpen(ClientWindow::main().taskBar().isOpen())
        , untrapper(ClientWindow::main())
    {
        // Create an example alert (lookalike).
        /// @todo There could be a class for an alert notification widget. -jk
        exampleAlert.reset(new LabelWidget);
        exampleAlert->setSizePolicy(ui::Expand, ui::Expand);
        exampleAlert->setImage(style().images().image("alert"));
        exampleAlert->setOverrideImageSize(style().fonts().font("default").height());
        exampleAlert->setImageColor(style().colors().colorf("accent"));

        // Highlight rectangle.
        self().add(highlight = new LabelWidget);
        highlight->set(Background(Background::GradientFrame,
                                  style().colors().colorf("accent"), 6));
        highlight->setOpacity(0);

        flashing.setSingleShot(false);
        flashing.setInterval(FLASH_SPAN);
    }

    void startHighlight(const GuiWidget &w)
    {
        highlight->rule().setRect(w.rule());
        highlight->setOpacity(0);
        highlight->show();
        flashing.start();
        flash();
    }

    /**
     * Animates the highlight flash rectangle. Called periodically.
     */
    void flash()
    {
        if (highlight->opacity().target() == 0)
        {
            highlight->setOpacity(.8f, FLASH_SPAN + .1, .1);
        }
        else if (highlight->opacity().target() > .5f)
        {
            highlight->setOpacity(.2f, FLASH_SPAN);
        }
        else
        {
            highlight->setOpacity(.8f, FLASH_SPAN);
        }
    }

    void stopHighlight()
    {
        highlight->hide();
        flashing.stop();
    }

    /**
     * Counts the total number of steps currently available.
     */
    int stepCount() const
    {
        int count = 0;
        for (Step s = Welcome; s != Finish; s = advanceStep(s)) count++;
        return count;
    }

    int stepOrdinal(Step s) const
    {
        int ord = stepCount() - 1;
        for (; s != Finish; s = advanceStep(s)) ord--;
        return ord;
    }

    /**
     * Determines which step follows step @a s.
     * @param s  Step.
     * @return The next step.
     */
    Step advanceStep(Step s) const
    {
        s = Impl::Step(s + 1);
        validateStep(s);
        return s;
    }

    Step previousStep(Step s) const
    {
        if (s == Welcome) return s;
        Step prev = Welcome;
        while (prev != Finish)
        {
            Step following = advanceStep(prev);
            if (following == s) break;
            prev = following;
        }
        return prev;
    }

    /**
     * Checks if step @a s is valid for the current engine state and if not,
     * skips to the next valid state.
     *
     * @param s  Current step.
     */
    void validateStep(Step &s) const
    {
        for (;;)
        {
            bool skip = false;
            if (!App_GameLoaded()) // in Home
            {
                if (s == RendererAppearance) skip = true;
            }
            else // A game is loaded.
            {
                if (s == HomeScreen) skip = true;
            }
            if (!skip) break;

            s = Step(s + 1);
        }
    }

    void initStep(Step s)
    {
        deinitStep();

        // Jump to the next valid step, if necessary.
        validateStep(s);

        if (s == Finish)
        {
            self().stop();
            return;
        }

        current = s;
        const bool isFirstStep = (current == Welcome);
        const bool isLastStep  = (current == Finish - 1);

        dlg = new MessageDialog;
        dlg->useInfoStyle();
        dlg->setDeleteAfterDismissed(true);
        dlg->setClickToClose(false);
        dlg->audienceForAccept() += [this]() { self().continueToNextStep(); };
        dlg->audienceForReject() += [this]() { self().stop(); };
        dlg->buttons() << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default | DialogWidget::Id1,
                                               isLastStep? "Done" : "Next");
        if (!isFirstStep)
        {
            dlg->buttons() << new DialogButtonItem(DialogWidget::Action | DialogWidget::Id2, "",
                                                   [this]() { self().backToPreviousStep(); });

            auto &prevBtn = *dlg->buttonWidget(DialogWidget::Id2);
            prevBtn.setImage(new StyleProceduralImage("fold", prevBtn, 90));
            prevBtn.setImageColor(style().colors().colorf("inverted.text"));
        }

        if (!isLastStep)
        {
            dlg->buttons() << new DialogButtonItem(DialogWidget::Reject | DialogWidget::Action,
                                                   "Close");

            auto &nextBtn = *dlg->buttonWidget(DialogWidget::Id1);
            nextBtn.setImage(new StyleProceduralImage("fold", nextBtn, -90));
            nextBtn.setImageColor(style().colors().colorf("inverted.text"));
            nextBtn.setTextAlignment(ui::AlignLeft);
        }

        // Insert the content for the dialog.
        ClientWindow &win = ClientWindow::main();
        switch (current)
        {
        case Welcome:
            dlg->title().setText("Welcome to Doomsday");
            dlg->message().setText("This tutorial will give you a brief walkthrough of the "
                                   "major features of Doomsday's UI. You will also get a "
                                   "chance to pick a shortcut key for opening the console.\n\n"
                                   "The tutorial can be restarted later via the application menu.");
            //.arg(_E(b) DOOMSDAY_NICENAME _E(.)));
            dlg->setAnchor(self().rule().midX(), self().rule().top());
            dlg->setOpeningDirection(ui::Down);
            break;

        case HomeScreen:
            dlg->title().setText("Game Library");
            dlg->message().setText("Here you can browse the library of available games "
                                   "and configure engine settings. You can also join ongoing "
                                   "multiplayer games and manage your mods and resource packages. "
                                   "You can unload the current game at "
                                   "any time to get back to the Game Library.");
            startHighlight(*root().guiFind("home"));
            break;

        case Notifications:
            // Fake notification area that doesn't have any the real currently showed
            // notifications.
            notifs = new NotificationAreaWidget("tutorial-notifications");
            notifs->useDefaultPlacement(ClientWindow::main().game().rule(), Const(0));
            root().addOnTop(notifs);
            notifs->showChild(*exampleAlert);

            dlg->title().setText("Notifications");
            dlg->message().setText(
                "The notification area shows the current notifications. "
                "For example, this one here is an example of a warning or an error "
                "that has occurred. You can click on the notification icons to "
                "get more information.\n\nOther possible notifications include the current "
                "FPS, ongoing downloads, and available updates.");
            dlg->setAnchorAndOpeningDirection(exampleAlert->rule(), ui::Down);
            startHighlight(*exampleAlert);
            break;

        case TaskBar:
            dlg->title().setText("Task Bar");
            dlg->message().setText(
                Stringf("The task bar is where you find all the important functionality: loading "
                        "and switching games, joining a multiplayer game, "
                        "configuration settings, "
                        "and a console command line for advanced users.\n\n"
                        "Press %s to access the task bar at any time.",
                        _E(b) "Shift-Esc" _E(.)));

            win.taskBar().open();
            win.taskBar().closeMainMenu();
            win.taskBar().closeConfigMenu();
            dlg->setAnchor(self().rule().midX(), win.taskBar().rule().top());
            dlg->setOpeningDirection(ui::Up);
            startHighlight(win.taskBar());
            break;

        case DEMenu:
            dlg->title().setText("Application Menu");
            dlg->message().setText(
                "Click the DE icon in the bottom right corner to open "
                "the application menu. "
                "You can check for available updates, switch games, or look for "
                "ongoing multiplayer games. You can also unload the current game "
                "and return to Doomsday's Game Library.");
            win.taskBar().openMainMenu();
            dlg->setAnchorAndOpeningDirection(root().guiFind("de-menu")->rule(), ui::Left);
            startHighlight(*root().guiFind("de-button"));
            break;

        case ConfigMenus:
            dlg->title().setText("Settings");
            dlg->message().setText("Configuration menus are found under buttons with a gear icon. "
                                      "The task bar's configuration button has the settings for "
                                   "all of Doomsday's subsystems.");
            win.taskBar().openConfigMenu();
            dlg->setAnchorAndOpeningDirection(root().guiFind("conf-menu")->rule(), ui::Left);
            startHighlight(*root().guiFind("conf-button"));
            break;

        case RendererAppearance:
            dlg->title().setText("Appearance");
            dlg->message().setText(Stringf("By default Doomsday applies many visual "
                                      "embellishments to how the game world appears. These "
                                      "can be configured individually in the Renderer "
                                      "Appearance editor, or you can use one of the built-in "
                                           "default profiles: %s, %s, or %s.",
                                           _E(b) "Defaults" _E(.),
                                           _E(b) "Vanilla" _E(.),
                                           _E(b) "Amplified" _E(.)));
            win.taskBar().openConfigMenu();
            win.root().guiFind("conf-menu")->as<PopupMenuWidget>().menu()
                    .organizer().itemWidget("Renderer")->as<ButtonWidget>().trigger();
            dlg->setAnchorAndOpeningDirection(
                        win.root().guiFind("renderersettings")->guiFind("appearance-label")->rule(), ui::Left);
            startHighlight(*root().guiFind("profile-picker"));
            break;

        case ConsoleKey: {
            dlg->title().setText("Console");
            String msg = "The console is a \"Quake style\" command line prompt where "
                            "you enter commands and change variable values. To get started, "
                         "try typing " _E(b) "help" _E(.) " in the console.";
            if (App_GameLoaded())
            {
                // Event bindings are currently stored per-game, so we can't set a
                // binding unless a game is loaded.
                msg +=
                    "\n\nBelow you can see the current keyboard shortcut for accessing the console "
                    "quickly. "
                          "To change it, click in the box and then press the key or key combination you "
                    "want to assign as the shortcut.";
                InputBindingWidget *bind = InputBindingWidget::newTaskBarShortcut();
                bind->invertStyle();
                dlg->area().add(bind);
            }
            dlg->message().setText(msg);
            dlg->setAnchor(win.taskBar().console().commandLine().rule().left() +
                           rule("gap"),
                           win.taskBar().rule().top());
            dlg->setOpeningDirection(ui::Up);
            dlg->updateLayout();
            startHighlight(win.taskBar().console().commandLine());
            break; }

        default:
            break;
        }

        // Progress indication.
        auto *progress = new ProgressWidget;
        progress->setColor("inverted.text");
        progress->setRange(Rangei(0, stepCount()));
        progress->setProgress(stepOrdinal(current) + 1, 0.0);
        progress->setMode(ProgressWidget::Dots);
        progress->rule()
                .setInput(Rule::Top,    dlg->buttonsMenu().rule().top())
                .setInput(Rule::Bottom, dlg->buttonsMenu().rule().bottom() -
                                        dlg->buttonsMenu().margins().bottom())
                .setInput(Rule::Left,   dlg->rule().left())
                .setInput(Rule::Right,  dlg->rule().right());
        dlg->add(progress);

        GuiRootWidget &root = self().root();

        // Keep the tutorial above any dialogs etc. that might've been opened.
        root.moveToTop(self());

        root.addOnTop(dlg);
        dlg->open();
    }

    /**
     * Cleans up after a tutorial step is done.
     */
    void deinitStep()
    {
        // Get rid of the previous dialog.
        if (dlg)
        {
            dlg->close(0.0);
            dlg = 0;
        }

        stopHighlight();

        ClientWindow &win = ClientWindow::main();
        switch (current)
        {
        case Notifications:
            notifs->hideChild(*exampleAlert);
            Loop::timer(0.500, [this]() { notifs->guiDeleteLater(); notifs = nullptr; });
            break;

        case DEMenu:
            win.taskBar().closeMainMenu();
            break;

        case ConfigMenus:
            win.taskBar().closeConfigMenu();
            break;

        case RendererAppearance:
            win.taskBar().closeConfigMenu();
            break;

        default:
            break;
        }
    }
};

TutorialWidget::TutorialWidget()
    : GuiWidget("tutorial"), d(new Impl(this))
{
    d->flashing += [this]() { flashHighlight(); };
}

void TutorialWidget::start()
{
    // Blur the rest of the view.
    ClientWindow::main().fadeInTaskBarBlur(.5);

    d->initStep(Impl::Welcome);
}

void TutorialWidget::stop()
{
    if (!d->taskBarInitiallyOpen)
    {
        ClientWindow::main().taskBar().close();
    }

    d->deinitStep();

    // Animate away and unfade darkening.
    ClientWindow::main().fadeOutTaskBarBlur(.5);

    Loop::timer(0.500, [this]() { dismiss(); });
}

void TutorialWidget::dismiss()
{
    hide();
    guiDeleteLater();
}

void TutorialWidget::flashHighlight()
{
    d->flash();
}

bool TutorialWidget::handleEvent(const Event &event)
{
    GuiWidget::handleEvent(event);

    // Eat everything!
    return true;
}

void TutorialWidget::continueToNextStep()
{
    d->initStep(d->advanceStep(d->current));
}

void TutorialWidget::backToPreviousStep()
{
    d->initStep(d->previousStep(d->current));
}
