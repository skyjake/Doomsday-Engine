/** @file shell/choicewidget.h  Widget for selecting an item from multiple choices.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBSHELL_CHOICETEDGET_H
#define LIBSHELL_CHOICETEDGET_H

#include "labelwidget.h"
#include "de/list.h"

namespace de { namespace term {

/**
 * Widget for selecting an item from multiple choices.
 *
 * @ingroup textUi
 */
class DE_PUBLIC ChoiceWidget : public LabelWidget
{
public:
    typedef StringList Items;

public:
    ChoiceWidget(const String &name = String());

    void setItems(const Items &items);
    void setPrompt(const String &prompt);
    void select(int pos);

    Items     items() const;
    int       selection() const;
    List<int> selections() const;

    /**
     * Determines if the selection menu is currently visible.
     */
    bool isOpen() const;

    Vec2i cursorPosition() const;

    // Events.
    void focusLost();
    void focusGained();
    void draw();
    bool handleEvent(const Event &event);

    void updateSelectionFromMenu();
    void menuClosed();

private:
    DE_PRIVATE(d)
};

}} // namespace de::term

#endif // LIBSHELL_CHOICETEDGET_H
