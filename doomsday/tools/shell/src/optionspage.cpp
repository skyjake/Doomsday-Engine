/** @file optionspage.cpp  Page for game options.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "optionspage.h"
#include <de/buttonwidget.h>
#include <de/choicewidget.h>
#include <de/dialogcontentstylist.h>
#include <de/gridlayout.h>
#include <de/lineeditwidget.h>
#include <de/togglewidget.h>
#include <de/hash.h>
#include <doomsday/doomsdayinfo.h>

using namespace de;

DE_GUI_PIMPL(OptionsPage)
{
    using GameOption = DoomsdayInfo::GameOption;
    using OptionWidgets = Hash<const GameOption *, GuiWidget *>;

    DialogContentStylist stylist;
    String               game;
    ButtonWidget *       acceptButton = nullptr;
    List<GameOption>     gameOptions;
    OptionWidgets        widgets;
    Record               gameState;
    IndirectRule *       layoutOrigin[2];

    Impl(Public *i) : Base(i)
    {
        for (auto &r : layoutOrigin) r = new IndirectRule;
        stylist.setContainer(self());
    }

    ~Impl()
    {
        for (auto &r : layoutOrigin) releaseRef(r);
    }

    void clear()
    {
        for (auto &elem : widgets)
        {
            GuiWidget::destroy(elem.second);
        }
        widgets.clear();

        GuiWidget::destroy(acceptButton);
        acceptButton = nullptr;
    }

    void initForGame(const String &gameId)
    {
        if (game == gameId) return;

        clear();
        game = gameId;
        gameOptions = DoomsdayInfo::gameOptions(game);

        auto &rect = self().rule();

        GridLayout layout(*layoutOrigin[0], *layoutOrigin[1]);
        layout.setGridSize(2, 0);
        layout.setColumnAlignment(0, ui::AlignRight);

        acceptButton = &self().addNew<ButtonWidget>();
        acceptButton->setSizePolicy(ui::Expand, ui::Expand);
        acceptButton->setText(_E(b) "Apply & Restart Map");
        acceptButton->setTextColor("dialog.default");
        acceptButton->setHoverTextColor("dialog.default", ButtonWidget::ReplaceColor);
        acceptButton->audienceForPress() += [this]() { apply(); };

        auto enableAcceptButton = [this]() { acceptButton->enable(); };

        for (const GameOption &opt : gameOptions)
        {
            String     label = opt.title + ":";
            GuiWidget *field;

            switch (opt.type)
            {
                case DoomsdayInfo::Toggle:
                {
                    auto *check = &self().addNew<ToggleWidget>();
                    check->setText(opt.title);
                    field = check;
                    label = "";
                    check->audienceForUserToggle() += enableAcceptButton;
                    break;
                }
                case DoomsdayInfo::Choice:
                {
                    auto *combo = &self().addNew<ChoiceWidget>();
                    field       = combo;
                    for (const auto &optValue : opt.allowedValues)
                    {
                        combo->items() << new ChoiceItem(optValue.label, optValue.value);
                    }
                    combo->audienceForUserSelection() += enableAcceptButton;
                    break;
                }
                case DoomsdayInfo::Text:
                {
                    auto *edit = &self().addNew<LineEditWidget>();
                    edit->rule().setInput(Rule::Width, rule("unit") * 60);
                    field = edit;
                    edit->audienceForContentChange() += enableAcceptButton;
                    break;
                }
            }

            layout << *LabelWidget::newWithText(label, &self()) << *field;
            widgets.insert(&opt, field);
        }

        acceptButton->rule()
            .setInput(Rule::Right, *layoutOrigin[0] + layout.width())
            .setInput(Rule::Top, *layoutOrigin[1] + layout.height() + rule("gap"));

        acceptButton->disable();

        // Now that we know the full size we can position the widgets.
        layoutOrigin[0]->setSource(rect.midX() - layout.width() / 2);
        layoutOrigin[1]->setSource(rect.midY() - layout.height() / 2);
    }

    bool checkRuleKeyword(const String &keyword) const
    {
        return gameState[DE_STR("rules")].value().asText().containsWord(keyword);
    }

    const GameOption::Value *selectValue(const GameOption &opt) const
    {
        const GameOption::Value *selected = &opt.allowedValues.at(0);
        for (size_t i = 1; i < opt.allowedValues.size(); ++i)
        {
            const auto &val = opt.allowedValues.at(i);
            if (checkRuleKeyword(val.ruleSemantic))
            {
                selected = &val;
                break;
            }
        }
        return selected;
    }

    GameOption::Value currentValueFromWidget(const GameOption &opt) const
    {
        const GuiWidget *widget = widgets[&opt];
        String           widgetValue;

        switch (opt.type)
        {
            case DoomsdayInfo::Toggle:
                widgetValue = opt.allowedValues
                                  .at(static_cast<const ToggleWidget *>(widget)->isActive() ? 1 : 0)
                                  .value;
                break;

            case DoomsdayInfo::Choice:
                widgetValue =
                    static_cast<const ChoiceWidget *>(widget)->selectedItem().data().asText();
                break;

            case DoomsdayInfo::Text:
                return GameOption::Value(static_cast<const LineEditWidget *>(widget)->text());
        }

        for (const auto &optValue : opt.allowedValues)
        {
            if (optValue.value == widgetValue) return optValue;
        }

        return {};
    }

    void updateValues(const Record &gameState)
    {
        this->gameState = gameState;

        // The widgets were previously created, but their current values need to
        // be updated to reflect the server state.

        for (const GameOption &opt : gameOptions)
        {
            GuiWidget *widget = widgets[&opt];

            switch (opt.type)
            {
                case DoomsdayInfo::Toggle:
                {
                    const auto *value = selectValue(opt);
                    auto *      check = static_cast<ToggleWidget *>(widget);
                    check->setActive(value != &opt.allowedValues.first());
                    break;
                }
                case DoomsdayInfo::Choice:
                {
                    const auto *value = selectValue(opt);
                    auto *      combo = static_cast<ChoiceWidget *>(widget);
                    for (size_t i = 0; i < opt.allowedValues.size(); ++i)
                    {
                        if (value == &opt.allowedValues.at(i))
                        {
                            combo->setSelected(i);
                        }
                    }
                    break;
                }
                case DoomsdayInfo::Text:
                {
                    auto *edit = static_cast<LineEditWidget *>(widget);
                    if (opt.defaultValue.ruleSemantic)
                    {
                        edit->setText(gameState[opt.defaultValue.ruleSemantic]);
                    }
                    break;
                }
            }
        }

        acceptButton->disable();
    }

    void apply()
    {
        acceptButton->disable();

        StringList commands;
        for (const GameOption &opt : gameOptions)
        {
            auto current = currentValueFromWidget(opt);
            if (!current.value.isEmpty())
            {
                commands << Stringf(opt.command, current.value.c_str());
            }
        }

        DE_NOTIFY_PUBLIC(Commands, i)
        {
            i->commandsSubmitted(commands);
        }
    }

    DE_PIMPL_AUDIENCE(Commands)
};

DE_AUDIENCE_METHOD(OptionsPage, Commands)

OptionsPage::OptionsPage()
    : GuiWidget("options")
    , d(new Impl(this))
{}

void OptionsPage::updateWithGameState(const Record &gameState)
{
    d->initForGame(gameState["mode"]);
    d->updateValues(gameState);
}
