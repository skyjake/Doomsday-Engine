/** @file opendialog.cpp  Dialog for opening server connection.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "opendialog.h"
#include "guishellapp.h"

#include <de/choicewidget.h>
#include <de/config.h>
#include <de/gridlayout.h>
#include <de/popupbuttonwidget.h>
#include <de/popupmenuwidget.h>
#include <de/lineeditwidget.h>

using namespace de;

static constexpr int MAX_HISTORY_SIZE = 10;

DE_GUI_PIMPL(OpenDialog)
, DE_OBSERVES(ServerFinder, Update)
{
    LineEditWidget *   address;
    PopupButtonWidget *moreButton;
    PopupMenuWidget *  historyMenu;
    ChoiceWidget *     localServers;
    StringList         history;
    bool               edited = false;

    Impl(Public *i) : Base(i)
    {
        auto &area = self().area();
        auto &cRule = area.contentRule();

        // Restore the historical entries.
        auto &cfg = Config::get();

        history = cfg.getStringList("OpenDialog.history", {"localhost"});
        tidyUpHistory();

        GridLayout layout(cRule.left(), cRule.top());
        layout.setGridSize(2, 0);
        layout.setColumnAlignment(0, ui::AlignRight);

        // Combobox with addresses and local servers.

        address = &area.addNew<LineEditWidget>("address");
        address->audienceForContentChange() += [this]() { textEdited(address->text()); };
        address->rule().setInput(Rule::Width, rule("unit") * 60);

        layout << *LabelWidget::newWithText("Address:", &area)
               << *address;

        historyMenu = &area.addNew<PopupMenuWidget>("history");

        moreButton = &area.addNew<PopupButtonWidget>("more");
        moreButton->setText("...");
        moreButton->rule().setLeftTop(address->rule().right(), address->rule().top());
        moreButton->setPopup(*historyMenu, ui::Right);

        // Insert old user-entered addresses into the box.
        for (String addr : history)
        {
            historyMenu->items() << makeHistoryItem(addr);
        }

        localServers = &area.addNew<ChoiceWidget>("local");
        localServers->setOpeningDirection(ui::Down);
        localServers->setNoSelectionHint("No servers on local network");
        localServers->setSelected(ui::Data::InvalidPos);
        localServers->audienceForUserSelection() += [this]() {
            address->setText(localServers->selectedItem().data().asText());
        };

        LabelWidget::appendSeparatorWithText("Local Network", &area, &layout);

        layout << *LabelWidget::newWithText("Servers:", &area) << *localServers;

        area.setContentSize(OperatorRule::maximum(layout.width(),
                                                  layout.widgets().at(0)->rule().width() +
                                                      layout.widgets().at(1)->rule().width() +
                                                      moreButton->rule().width()),
                            layout.height());

        self().buttons()
            << new DialogButtonItem(Accept | Default | Id1, "Connect")
            << new DialogButtonItem(Reject, "Cancel");
    }

    ui::ActionItem *makeHistoryItem(String text)
    {
        return new ui::ActionItem(text, [this, text]() { address->setText(text); });
    }

    void tidyUpHistory()
    {
        StringList tidied;
        for (String &host : history)
        {
            if (auto pos = host.indexOf('('))
            {
                host = host.left(pos - 1);
            }
            tidied << host;
        }
        history = tidied;
    }

    /**
     * Determines if a host is currently listed in the address combo box.
     */
    bool isListed(const Address &host) const
    {
        const String hostStr = host.asText();
        for (dsize i = 0; i < historyMenu->items().size(); ++i)
        {
            if (historyMenu->items().at(i).data().asText().compareWithoutCase(hostStr) == 0)
            {
                return true;
            }
        }
        return false;
    }

    void foundServersUpdated()
    {
        self().updateLocalList();
    }

    void textEdited(const String &text)
    {
        if (!edited)
        {
            edited = true;
            historyMenu->items().insert(0, makeHistoryItem(text));
        }
        else
        {
            historyMenu->items().at(0).setLabel(text);
        }
    }
};

OpenDialog::OpenDialog()
    : DialogWidget("open", WithHeading)
    , d(new Impl(this))
{
    heading().setText("Open Connection");
    updateLocalList(true /* autoselect first found server */);
    audienceForAccept() += [this]() { saveState(); };
}

String OpenDialog::address() const
{
//    int sel = d->address->currentIndex();
//    if (d->address->itemData(sel).canConvert<Address>())
//    {
//        return convert(d->address->itemData(sel).value<Address>().asText());
//    }

//    // User-entered item.
//    QString text = d->address->currentText();

//    // Omit parentheses at the end.
//    int pos = text.indexOf('(');
//    if (pos > 0)
//    {
//        text = text.left(pos - 1);
//    }

//    text = text.trimmed();
    return d->address->text();
}

void OpenDialog::updateLocalList(bool autoselect)
{
    ServerFinder &       finder   = GuiShellApp::app().serverFinder();
    bool                 selected = false;
    auto &               items    = d->localServers->items();
    KeyMap<String, Address> found;

    for (const auto &sv : finder.foundServers())
    {
        found.insert(sv.asText(), sv);
    }

    if (found.empty())
    {
        // Nothing found.
        d->localServers->setNoSelectionHint("No local servers found");
        items.clear();
    }
    else
    {
        d->localServers->setNoSelectionHint(Stringf("%d local server%s",
                                                    found.size(),
                                                    DE_PLURAL_S(found.size())));
//        d->localCount->setText(tr("<small>Found %1 local server%2.</small>")
//                               .arg(finder.foundServers().size())
//                               .arg(finder.foundServers().size() != 1? "s" : ""));

        // Update the list of servers.
        for (const auto &sv : found)
        {
            String label = Stringf("%s - %s (%d/%d)",
                                   sv.first.c_str(),
                                   finder.name(sv.second).left(CharPos(20)).c_str(),
                                   finder.playerCount(sv.second),
                                   finder.maxPlayers(sv.second));

            if (!d->isListed(sv.second))
            {
                items << new ChoiceItem(label, sv.first);

                // Autoselect the first one?
                if (autoselect && !selected)
                {
                    d->localServers->setSelected(items.size() - 1);
                    d->address->setText(sv.first);
                    selected = true;
                }
            }
        }
    }

    // Remove servers no longer present.
    for (dsize i = 0; i < items.size(); )
    {
        if (!found.contains(items.at(i).data().asText()))
        {
            items.remove(i);
            continue;
        }
        ++i;
    }
}

void OpenDialog::saveState()
{
    if (d->edited)
    {
        const String text = d->address->text();
        d->history.removeAll(text);
        d->history.prepend(text);

        // Make sure there aren't too many entries.
        while (d->history.size() > MAX_HISTORY_SIZE)
        {
            d->history.removeLast();
        }
    }

    Config::get().set("OpenDialog.history", new ArrayValue(d->history));
}

void OpenDialog::prepare()
{
    DialogWidget::prepare();
    root().setFocus(d->address);
}
