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
#include <de/Hash>
#include <doomsday/DoomsdayInfo>

using namespace de;

DE_PIMPL(OptionsPage)
{
    using GameOption = DoomsdayInfo::GameOption;

    String game;
//    QWidget *base = nullptr;
//    QFormLayout *layout = nullptr;
//    QVBoxLayout *vbox = nullptr;
//    QDialogButtonBox *buttons = nullptr;
    List<GameOption> gameOptions;
    Hash<const GameOption *, GuiWidget *> widgets;
    Record gameState;

    Impl(Public *i) : Base(i)
    {}

    void clear()
    {
//        qDeleteAll(widgets.values());
//        widgets.clear();

//        delete base;
//        base = nullptr;

//        delete buttons;
//        buttons = nullptr;

//        while (vbox && vbox->count() > 0)
//        {
//            delete vbox->takeAt(0);
//        }
    }

    void initForGame(const String &gameId)
    {
        if (game == gameId) return;

        clear();
        game = gameId;
        gameOptions = DoomsdayInfo::gameOptions(game);

//        base   = new QWidget;
//        base->setMaximumWidth(320);
//        layout = new QFormLayout(base);

//        buttons = new QDialogButtonBox;
//        buttons->setMaximumWidth(320);
//        auto *acceptButton = buttons->addButton("Apply && Restart Map", QDialogButtonBox::AcceptRole);
//        QObject::connect(acceptButton, &QPushButton::pressed, [this] () { apply(); });

//        foreach (GameOption const &opt, gameOptions)
//        {
//            QString label = convert(opt.title + ":");
//            QWidget *field;

//            switch (opt.type)
//            {
//            case shell::DoomsdayInfo::Toggle: {
//                auto *check = new QCheckBox(convert(opt.title));
//                field = check;
//                label = "";
//                QObject::connect(check, &QCheckBox::toggled,
//                                 [this] (bool) { buttons->setEnabled(true); });
//                break; }

//            case shell::DoomsdayInfo::Choice: {
//                auto *combo = new QComboBox;
//                field = combo;
//                foreach (auto optValue, opt.allowedValues)
//                {
//                    combo->addItem(convert(optValue.label), convert(optValue.value));
//                }
//                QObject::connect(combo, static_cast<void (QComboBox::*)(int)>
//                                        (&QComboBox::currentIndexChanged),
//                                 [this] (int) { buttons->setEnabled(true); });
//                break; }

//            case shell::DoomsdayInfo::Text: {
//                auto *edit = new QLineEdit;
//                edit->setMinimumWidth(180);
//                field = edit;
//                QObject::connect(edit, &QLineEdit::textEdited,
//                                 [this] (QString) { buttons->setEnabled(true); });
//                break; }
//            }

//            layout->addRow(label, field);
//            widgets.insert(&opt, field);
//        }
//        base->setLayout(layout);

//        vbox->setContentsMargins(36, 36, 36, 36);
//        vbox->addStretch(1);
//        vbox->addWidget(base, 0, Qt::AlignCenter);
//        vbox->addWidget(buttons, 0, Qt::AlignCenter);
//        vbox->addStretch(1);

//        buttons->setEnabled(false);
    }

    bool checkRuleKeyword(String const &keyword) const
    {
        return gameState["rules"].value().asText().containsWord(keyword);
    }

    GameOption::Value const *selectValue(GameOption const &opt) const
    {
        GameOption::Value const *selected = &opt.allowedValues.at(0);
        for (size_t i = 1; i < opt.allowedValues.size(); ++i)
        {
            auto const &val = opt.allowedValues.at(i);
            if (checkRuleKeyword(val.ruleSemantic))
            {
                selected = &val;
                break;
            }
        }
        return selected;
    }

    GameOption::Value currentValueFromWidget(GameOption const &opt) const
    {
        const GuiWidget *widget = widgets[&opt];
        String widgetValue;

        switch (opt.type)
        {
        case DoomsdayInfo::Toggle:
//            widgetValue = convert(opt.allowedValues.at
//                    (static_cast<QCheckBox const *>(widget)->isChecked()? 1 : 0).value);
            break;

        case DoomsdayInfo::Choice:
//            widgetValue = static_cast<QComboBox const *>(widget)->currentData().toString();
            break;

        case DoomsdayInfo::Text:
//            return GameOption::Value(convert(static_cast<QLineEdit const *>(widget)->text()));
            break;
        }

        for (auto const &optValue : opt.allowedValues)
        {
            if (optValue.value == widgetValue)
                return optValue;
        }

        return {};
    }

    void updateValues(Record const &gameState)
    {
        this->gameState = gameState;

        // The widgets were previously created, but their current values need to
        // be updated to reflect the server state.

        for (const GameOption &opt : gameOptions)
        {
            GuiWidget *widget = widgets[&opt];

            switch (opt.type)
            {
            case DoomsdayInfo::Toggle: {
                auto const *value = selectValue(opt);
//                QCheckBox *check = static_cast<QCheckBox *>(widget);
//                check->setChecked(value != &opt.allowedValues.first());
                break; }

            case DoomsdayInfo::Choice: {
                auto const *value = selectValue(opt);
//                QComboBox *combo = static_cast<QComboBox *>(widget);
//                for (int i = 0; i < opt.allowedValues.sizei(); ++i)
//                {
//                    if (value == &opt.allowedValues.at(i))
//                    {
//                        combo->setCurrentIndex(i);
//                    }
//                }
                break; }

            case DoomsdayInfo::Text: {
//                QLineEdit *edit = static_cast<QLineEdit *>(widget);
//                if (!opt.defaultValue.ruleSemantic.isEmpty())
//                {
//                    edit->setText(convert(gameState[opt.defaultValue.ruleSemantic]));
//                }
                break; }
            }
        }

//        buttons->setEnabled(false);
    }

    void apply()
    {
//        buttons->setEnabled(false);

        StringList commands;
        for (const GameOption &opt : gameOptions)
        {
            auto current = currentValueFromWidget(opt);
            if (!current.value.isEmpty())
            {
//                commands << convert(opt.command).arg(convert(current.value));
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
{
//    d->vbox = new QVBoxLayout;
//    setLayout(d->vbox);
}

void OptionsPage::updateWithGameState(Record const &gameState)
{
    d->initForGame(gameState["mode"]);
    d->updateValues(gameState);
}
