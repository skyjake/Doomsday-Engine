/** @file choicewidget.h  Widget for selecting an item from multiple choices.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_CHOICEWIDGET_H
#define LIBSHELL_CHOICEWIDGET_H

#include "LabelWidget"
#include <QList>

namespace de {
namespace shell {

/**
 * Widget for selecting an item from multiple choices.
 */
class LIBSHELL_PUBLIC ChoiceWidget : public LabelWidget
{
    Q_OBJECT

public:
    typedef QList<String> Items;

public:
    ChoiceWidget(String const &name = "");

    void setItems(Items const &items);

    void setPrompt(String const &prompt);

    Items items() const;

    void select(int pos);

    int selection() const;

    QList<int> selections() const;

    /**
     * Determines if the selection menu is currently visible.
     */
    bool isOpen() const;

    Vector2i cursorPosition() const;

    // Events.
    void focusLost();
    void focusGained();
    void draw();
    bool handleEvent(Event const &event);

protected slots:
    void updateSelectionFromMenu();
    void menuClosed();

private:
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_CHOICEWIDGET_H
