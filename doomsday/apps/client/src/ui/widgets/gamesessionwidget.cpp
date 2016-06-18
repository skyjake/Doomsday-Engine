/** @file gamesessionwidget.cpp
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

#include "ui/widgets/gamesessionwidget.h"
#include "ui/clientwindow.h"
#include "clientapp.h"

#include <de/App>
#include <de/PopupButtonWidget>
#include <de/DocumentPopupWidget>
#include <de/SignalAction>
#include <doomsday/doomsdayapp.h>

#include <QFileDialog>

using namespace de;

DENG2_PIMPL(GameSessionWidget)
, DENG2_OBSERVES(DoomsdayApp, GameUnload)
{
    PopupStyle popupStyle;
    ButtonWidget *load;
    PopupButtonWidget *info;
    PopupButtonWidget *funcs = nullptr;
    DocumentPopupWidget *doc = nullptr;
    PopupMenuWidget *menu = nullptr;
    ButtonWidget *actionButton = nullptr;

    Instance(Public *i, PopupStyle ps, ui::Direction popupOpeningDirection)
        : Base(i)
        , popupStyle(ps)
    {
        // Set up the buttons.
        self.add(load = new ButtonWidget);
        self.add(info = new PopupButtonWidget);
        if (popupStyle == PopupMenu)
        {
            self.add(funcs = new PopupButtonWidget);
        }

        load->disable();
        load->setBehavior(Widget::ContentClipping);
        load->setAlignment(ui::AlignLeft);
        load->setTextAlignment(ui::AlignRight);
        load->setTextLineAlignment(ui::AlignLeft);
        load->setImageScale(self.toDevicePixels(1)); /// @todo We don't have 2x game logos.

        info->setWidthPolicy(ui::Expand);
        info->setAlignment(ui::AlignBottom);
        info->setText(_E(s)_E(B) + tr("..."));

        // Set up the info/actions popup widget.
        if (popupStyle == PopupWithDataFileButton)
        {
            actionButton = new ButtonWidget;
            self.add(doc = new DocumentPopupWidget(actionButton));
        }
        else
        {
            self.add(doc = new DocumentPopupWidget);
        }
        doc->document().setMaximumLineWidth(doc->rule("document.popup.width").valuei());
        info->setPopup(*doc, popupOpeningDirection);
        info->setOpener([this] (PopupWidget *) {
            self.updateInfoContent();
            doc->open();
        });

        if (popupStyle == PopupMenu)
        {
            self.add(menu = new PopupMenuWidget);
            funcs->setPopup(*menu, ui::Right);
        }

        DoomsdayApp::app().audienceForGameUnload() += this;
    }

    ~Instance()
    {
        //DoomsdayApp::app().audienceForGameUnload() -= this;

        if (menu) menu->dismiss();
        doc->dismiss();
    }

    void aboutToUnloadGame(Game const &)
    {
        doc->close(0);
        if (menu) menu->close(0);
    }
};

GameSessionWidget::GameSessionWidget(PopupStyle ps,
                                     ui::Direction popupOpeningDirection)
    : d(new Instance(this, ps, popupOpeningDirection))
{
    Font const &font = style().fonts().font("default");
    rule().setInput(Rule::Height, OperatorRule::maximum(font.lineSpacing() * 3 +
                                                        font.height() + margins().height(),
                                                        d->load->contentHeight()));

    // Button for extra information.
    d->load->rule()
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Top,    rule().top())
            .setInput(Rule::Bottom, rule().bottom())
            .setInput(Rule::Right,  d->info->rule().left());

    d->info->rule()
            .setInput(Rule::Top,    rule().top())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, rule().bottom());

    if (d->popupStyle == PopupMenu)
    {
        d->funcs->rule()
                .setInput(Rule::Top,    rule().top())
                .setInput(Rule::Right,  rule().right())
                .setInput(Rule::Height, d->info->rule().width())
                .setInput(Rule::Width,  d->info->rule().width());

        d->info->rule().setInput(Rule::Top, d->funcs->rule().bottom());
    }
}

GameSessionWidget::PopupStyle GameSessionWidget::popupStyle() const
{
    return d->popupStyle;
}

ButtonWidget &GameSessionWidget::loadButton()
{
    return *d->load;
}

ButtonWidget &GameSessionWidget::infoButton()
{
    return *d->info;
}

ButtonWidget &GameSessionWidget::menuButton()
{
    return *d->funcs;
}

DocumentWidget &GameSessionWidget::document()
{
    DENG2_ASSERT(d->doc);
    return d->doc->document();
}

PopupMenuWidget &GameSessionWidget::menu()
{
    DENG2_ASSERT(d->menu);
    return *d->menu;
}

void GameSessionWidget::setDataFileAction(DataFileAction action)
{
    if (action == Select)
    {
        d->actionButton->setText(tr("Data Files..."));
        d->actionButton->setAction(new SignalAction(this, SLOT(browseDataFiles())));
    }
    else
    {
        d->actionButton->setText(tr("Reset"));
        d->actionButton->setAction(new SignalAction(this, SLOT(clearDataFiles())));
    }
}

void GameSessionWidget::updateInfoContent()
{
    // overridden by derived classes
}

void GameSessionWidget::browseDataFiles()
{
    ClientApp::app().beginNativeUIMode();

    QFileDialog dlg(&ClientWindow::main(),
                    tr("Select Additional Data Files"));
    dlg.setReadOnly(true);
    dlg.setFileMode(QFileDialog::ExistingFiles);
    dlg.setNameFilter("Data files (*.wad *.deh *.ded *.lmp *.pk3)");
    dlg.setLabelText(QFileDialog::Accept, tr("Select"));
    if (dlg.exec())
    {
        StringList paths;
        for (QString const &p : dlg.selectedFiles()) paths << p;
        setDataFiles(paths);
    }

    ClientApp::app().endNativeUIMode();
    d->info->popup()->close();
}

void GameSessionWidget::clearDataFiles()
{
    setDataFiles(StringList());
}

void GameSessionWidget::setDataFiles(StringList const &/*paths*/)
{
    // overridden by derived classes
}
