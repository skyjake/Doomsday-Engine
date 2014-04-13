/** @file gamesdialog.cpp  Dialog for viewing and loading available games.
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

#include "ui/dialogs/gamesdialog.h"
#include "ui/widgets/gameselectionwidget.h"
#include "ui/dialogs/networksettingsdialog.h"
#include "ui/dialogs/manualconnectiondialog.h"
#include "CommandAction"

#include <de/SignalAction>

using namespace de;

DENG_GUI_PIMPL(GamesDialog)
{
    Mode mode;
    GameSelectionWidget *gameSel; //MenuWidget *list;

    Instance(Public *i, Mode m)
        : Base(i)
        , mode(m)
    {
        self.area().add(gameSel = new GameSelectionWidget("games"));

        gameSel->enableScrolling(false);
        gameSel->setTitleFont("heading");
        gameSel->setTitleColor("accent", "text", ButtonWidget::ReplaceColor);
        gameSel->rule().setInput(Rule::Height, gameSel->contentRule().height());
    }
};

GamesDialog::GamesDialog(Mode mode, String const &name)
    : DialogWidget(name/*, WithHeading*/)
    , d(new Instance(this, mode))
{
    connect(d->gameSel, SIGNAL(gameSessionSelected(de::ui::Item const *)), this, SLOT(selectSession(de::ui::Item const *)));

    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(1, 0);

    if(d->mode == ShowAll)
    {
        // Include the filter in the layout.
        d->gameSel->filter().rule().setInput(Rule::Width, d->gameSel->rule().width());
        layout << d->gameSel->filter();

        // Disallow changing the filter.
        d->gameSel->filter().setFilter(GameFilterWidget::AllGames, GameFilterWidget::Permanent);
    }
    else
    {
        // Disallow changing the filter.
        d->gameSel->filter().hide();
        d->gameSel->filter().setFilter(d->mode == ShowSingleplayerOnly?
                                           GameFilterWidget::Singleplayer :
                                           GameFilterWidget::Multiplayer,
                                       GameFilterWidget::Permanent);

        switch(d->mode)
        {
        case ShowSingleplayerOnly:
            d->gameSel->subsetFold("available")->open();
            d->gameSel->subsetFold("incomplete")->close();
            break;

        case ShowMultiplayerOnly:
            d->gameSel->subsetFold("multi")->open();
            break;

        default:
            break;
        }
    }
    layout << *d->gameSel;

    area().setContentSize(layout.width(), layout.height());

    // Buttons appropriate for the mode:
    buttons() << new DialogButtonItem(Default | Accept, tr("Close"));

    if(d->mode != ShowSingleplayerOnly)
    {
        buttons() << new DialogButtonItem(Action | Id2, tr("Connect Manually..."),
                                          new SignalAction(this, SLOT(connectManually())));

        // Multiplayer settings.
        buttons() << new DialogButtonItem(Action | Id1, style().images().image("gear"),
                                          new SignalAction(this, SLOT(showSettings())));
    }
}

void GamesDialog::showSettings()
{
    NetworkSettingsDialog *dlg = new NetworkSettingsDialog;
    dlg->setAnchorAndOpeningDirection(buttonWidget(Id1)->rule(), ui::Up);
    dlg->setDeleteAfterDismissed(true);
    dlg->exec(root());
}

void GamesDialog::connectManually()
{
    ManualConnectionDialog *dlg = new ManualConnectionDialog;
    dlg->setAnchorAndOpeningDirection(buttonWidget(Id2)->rule(), ui::Up);
    dlg->setDeleteAfterDismissed(true);
    dlg->enableJoinWhenSelected(false); // we'll do it ourselves
    connect(dlg, SIGNAL(selected(de::ui::Item const *)), this, SLOT(sessionSelectedManually(de::ui::Item const *)));
    dlg->exec(root());
}

void GamesDialog::selectSession(ui::Item const *item)
{
    setAcceptanceAction(d->gameSel->makeAction(*item));
    accept();
}

void GamesDialog::sessionSelectedManually(ui::Item const *item)
{
    ManualConnectionDialog *dlg = (ManualConnectionDialog *) sender();
    setAcceptanceAction(dlg->makeAction(*item));
    accept();
}

void GamesDialog::preparePanelForOpening()
{
    DialogWidget::preparePanelForOpening();

    d->gameSel->rule()
            .setInput(Rule::Width,  OperatorRule::minimum(
                          style().rules().rule("gameselection.max.width"),
                          root().viewWidth() - margins().width()));
}
