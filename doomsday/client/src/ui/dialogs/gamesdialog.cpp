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

#include "CommandAction"

#include <de/SignalAction>

using namespace de;

DENG_GUI_PIMPL(GamesDialog)
{
    GameSelectionWidget *gameSel; //MenuWidget *list;

    Instance(Public *i) : Base(i)
    {
        self.area().add(gameSel = new GameSelectionWidget("games"));

        gameSel->enableScrolling(false);
        gameSel->setTitleFont("heading");
        gameSel->setTitleColor("accent", "text", ButtonWidget::ReplaceColor);
        gameSel->rule().setInput(Rule::Height, gameSel->contentRule().height());
    }
};

GamesDialog::GamesDialog(String const &name)
    : DialogWidget(name/*, WithHeading*/), d(new Instance(this))
{
    connect(d->gameSel, SIGNAL(gameSessionSelected()), this, SLOT(accept()));

    //heading().setText(tr("Games"));

    //LabelWidget *lab = LabelWidget::newWithText(tr("Games from Master Server and local network:"), &area());

    //d->gameSel->rule().setInput(Rule::Height, d->gameSel->contentRule().height()/*style().rules().rule("gameselection.max.height")*/);

    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(1, 0);
    //layout.setColumnAlignment(0, ui::AlignRight);

    layout << *d->gameSel;

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(Default | Accept, tr("Close"))
            << new DialogButtonItem(Action | Id1,
                                    style().images().image("gear"),
                                    new SignalAction(this, SLOT(showSettings())));
}

void GamesDialog::showSettings()
{
    NetworkSettingsDialog *dlg = new NetworkSettingsDialog;
    dlg->setAnchorAndOpeningDirection(buttonWidget(Id1)->rule(), ui::Up);
    dlg->setDeleteAfterDismissed(true);
    dlg->exec(root());
}

void GamesDialog::preparePanelForOpening()
{
    DialogWidget::preparePanelForOpening();

    d->gameSel->rule()
            .setInput(Rule::Width,  OperatorRule::minimum(
                          style().rules().rule("gameselection.max.width"),
                          root().viewWidth() - margins().width()));
}
