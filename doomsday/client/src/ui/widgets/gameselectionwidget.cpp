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
#include "ui/widgets/actionitem.h"
#include "ui/commandaction.h"
#include "clientapp.h"
#include "games.h"
#include "dd_main.h"

#include <QMap>

using namespace de;

DENG2_PIMPL(GameSelectionWidget),
DENG2_OBSERVES(Games, Addition),
DENG2_OBSERVES(App, StartupComplete),
DENG2_OBSERVES(ContextWidgetOrganizer, WidgetCreation)
{
    struct GameItem : public ui::ActionItem {
        GameItem(Game const &gameRef, de::String const &label, de::Action *action)
            : ui::ActionItem(label, action), game(gameRef) {}
        Game const &game;
    };

    Instance(Public *i) : Base(i)
    {
        App_Games().audienceForAddition += this;
        App::app().audienceForStartupComplete += this;

        self.organizer().audienceForWidgetCreation += this;
    }

    ~Instance()
    {
        App_Games().audienceForAddition -= this;
        App::app().audienceForStartupComplete -= this;
    }

    void gameAdded(Game &game)
    {
        self.items().append(makeItemForGame(game));
    }

    ui::Item *makeItemForGame(Game &game)
    {
        String const idKey = Str_Text(game.identityKey());

        CommandAction *loadAction = new CommandAction(String("load ") + idKey);
        String label = String(_E(b) "%1" _E(.)_E(s)_E(C) " %2\n"
                           _E(.)_E(.)_E(l)_E(D) "%3")
                .arg(Str_Text(game.title()))
                .arg(Str_Text(game.author()))
                .arg(idKey);

        GameItem *item = new GameItem(game, label, loadAction);

        /// @todo The name of the plugin should be accessible via the plugin loader.
        String plugName;
        if(idKey.contains("heretic"))
        {
            plugName = "libheretic";
        }
        else if(idKey.contains("hexen"))
        {
            plugName = "libhexen";
        }
        else
        {
            plugName = "libdoom";
        }
        if(self.style().images().has("logo.game." + plugName))
        {
            item->setImage(self.style().images().image("logo.game." + plugName));
        }

        return item;
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        ButtonWidget &b = widget.as<ButtonWidget>();

        b.setBehavior(Widget::ContentClipping);
        b.setAlignment(ui::AlignLeft);
        b.setTextLineAlignment(ui::AlignLeft);
        b.setHeightPolicy(ui::Expand);
        b.setOpacity(.3f, .5f);
    }

    void appStartupCompleted()
    {
        updateGameAvailability();
    }

    void updateGameAvailability()
    {
        for(uint i = 0; i < self.items().size(); ++i)
        {
            GameItem const &item = self.items().at(i).as<GameItem>();

            GuiWidget *w = self.organizer().itemWidget(item);
            DENG2_ASSERT(w != 0);

            if(item.game.allStartupFilesFound())
            {
                w->setOpacity(1.f, .5f);
                w->enable();
            }
            else
            {
                w->setOpacity(.3f, .5f);
                w->disable();
            }
        }

        self.items().sort();
    }
};

GameSelectionWidget::GameSelectionWidget(String const &name)
    : MenuWidget(name), d(new Instance(this))
{
    setGridSize(3, ui::Filled, 6, ui::Filled);
}
