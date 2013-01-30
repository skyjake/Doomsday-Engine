/** @file menuwidget.h  Menu with shortcuts.
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

#ifndef LIBSHELL_MENUWIDGET_H
#define LIBSHELL_MENUWIDGET_H

#include "TextWidget"
#include "Action"
#include "TextCanvas"

namespace de {
namespace shell {

/**
 * Menu with Action instances as items.
 *
 * The width of the widget is automatically determined based on how much space
 * is needed for the items and their possible shortcut labels. The height of
 * the widget depends on the number of items in the menu.
 */
class MenuWidget : public TextWidget
{
public:
    MenuWidget(String const &name = "");

    ~MenuWidget();

    int itemCount() const;

    void appendItem(Action *action, String const &shortcutLabel = "");

    void appendSeparator();

    void insertItem(int pos, Action *action, String const &shortcutLabel = "");

    void insertSeparator(int pos);

    void clear();

    void removeItem(int pos);

    Action &itemAction(int pos) const;

    void setCursor(int pos);

    int cursor() const;

    void setSelectionAttribs(TextCanvas::Char::Attribs const &attribs);

    void setBackgroundAttribs(TextCanvas::Char::Attribs const &attribs);

    void setBorderAttribs(TextCanvas::Char::Attribs const &attribs);

    Vector2i cursorPosition() const;

    // Events.
    void draw();
    bool handleEvent(Event const *);

private:
    struct Instance;
    Instance *d;
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_MENUWIDGET_H
