/** @file gamecolumnwidget.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/home/gamecolumnwidget.h"
#include "ui/home/headerwidget.h"

using namespace de;

DENG2_PIMPL(GameColumnWidget)
{
    HeaderWidget *header;

    Instance(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.scrollArea();

        area.add(header = new HeaderWidget);
        header->rule()
                .setInput(Rule::Left,  area.contentRule().left())
                .setInput(Rule::Top,   area.contentRule().top())
                .setInput(Rule::Width, area.contentRule().width());
    }
};

GameColumnWidget::GameColumnWidget(String  const &name,
                                   String  const &author,
                                   String  const &gameTitle,
                                   DotPath const &logoId)
    : ColumnWidget(name)
    , d(new Instance(this))
{
    scrollArea().setContentSize(maximumContentWidth(),
                                d->header->rule().height());

    d->header->title().setText(String(_E(s) "%1\n" _E(.)_E(w) "%2")
                               .arg(author)
                               .arg(gameTitle));
    if(!logoId.isEmpty())
    {
        d->header->setLogoImage(logoId);
    }

    /// @todo Get these from the games def.
    if(name == "doom-column")
    {
        d->header->info().setText("id Software released DOOM for MS-DOS in 1993. It soon became a massive hit and is regarded as the game that popularized the first-person shooter genre. Since then the franchise has been continued in several sequels, starting with DOOM II: Hell on Earth in 1994. DOOM and many of its follow-ups have been ported to numerous other platforms, and to this day remains a favorite among gamers.");
    }
    else if(name == "heretic-column")
    {
        d->header->info().setText("Raven Software released Heretic in 1994. It used a modified version of id Software's DOOM engine. The game featured such enhancements as inventory management and the ability to look up and down. Ambient sound effects were used to improve the atmosphere of the game world.");
    }
    else if(name == "hexen-column")
    {
        d->header->info().setText("Raven Software released Hexen in 1996. The company had continued making heavy modifications to the DOOM engine, and Hexen introduced such sophisticated features as a scripting language for game events. The maps were well-designed and interconnected with each other, resulting in a more intriguing game world and more complex puzzles to solve.");
    }
    else
    {
        d->header->info().setText("Thanks to its excellent modding support, DOOM was used as a basis for many games and community projects.");
    }
}

void GameColumnWidget::setHighlighted(bool highlighted)
{
    ColumnWidget::setHighlighted(highlighted);

    d->header->setOpacity(highlighted? 1 : .7, .5);
}
