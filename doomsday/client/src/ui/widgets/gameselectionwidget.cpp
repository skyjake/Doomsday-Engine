/** @file gameselectionwidget.cpp
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

#include "ui/widgets/gameselectionwidget.h"
#include "ui/commandaction.h"
#include "clientapp.h"
#include "games.h"
#include "dd_main.h"

#include <QMap>

using namespace de;

DENG2_PIMPL(GameSelectionWidget),
DENG2_OBSERVES(Games, Addition),
DENG2_OBSERVES(App, StartupComplete)
{
    typedef QMap<Game *, ButtonWidget *> Buttons;
    Buttons buttons;

    Instance(Public *i) : Base(i)
    {
        App_Games().audienceForAddition += this;
        App::app().audienceForStartupComplete += this;
    }

    ~Instance()
    {
        App_Games().audienceForAddition -= this;
        App::app().audienceForStartupComplete -= this;
    }

    void gameAdded(Game &game)
    {
        ButtonWidget *b = addItemForGame(game);
        buttons.insert(&game, b);
    }

    ButtonWidget *addItemForGame(Game &game)
    {
        CommandAction *loadAction = new CommandAction(String("load ") +
                                                      Str_Text(game.identityKey()));
        ButtonWidget *b = self.addItem(
                    String(_E(b) "%1" _E(.)_E(s)_E(C) " %2\n"
                           _E(.)_E(.)_E(l)_E(D) "%3")
                    .arg(Str_Text(game.title()))
                    .arg(Str_Text(game.author()))
                    .arg(Str_Text(game.identityKey())),
                    loadAction);

        //b->setBehavior(Widget::ContentClipping);
        b->setAlignment(ui::AlignTopLeft);
        b->setTextLineAlignment(ui::AlignLeft);
        b->setHeightPolicy(ui::Expand);
        return b;
    }

    void appStartupCompleted()
    {
        updateGameAvailability();
    }

    void updateGameAvailability()
    {
        DENG2_FOR_EACH(Buttons, i, buttons)
        {
            if(i.key()->allStartupFilesFound())
            {
                i.value()->setOpacity(1.f, .5f);
            }
            else
            {
                i.value()->setOpacity(.3f, .5f);
            }
        }
    }
};

GameSelectionWidget::GameSelectionWidget(String const &name)
    : MenuWidget(name), d(new Instance(this))
{
    setGridSize(3, ui::Filled, 6, ui::Filled);
}
