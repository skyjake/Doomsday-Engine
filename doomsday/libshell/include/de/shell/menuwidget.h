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
 *
 * Actions added to the menu are considered shortcuts and triggering them will
 * cause the menu to close (if it is closable).
 */
class LIBSHELL_PUBLIC MenuWidget : public TextWidget
{
    Q_OBJECT

public:
    enum Preset
    {
        Popup,      ///< Menu initially hidden, will popup on demand.
        AlwaysOpen  ///< Menu initially shown, stays open.
    };

    enum BorderStyle
    {
        NoBorder,
        LineBorder
    };

public:
    MenuWidget(Preset preset, String const &name = "");

    ~MenuWidget();

    int itemCount() const;

    /**
     * Appends an item into the menu as the last item.
     *
     * @param action  Action to add as a shortcut for triggering the item.
     * @param shortcutLabel  Label to show, representing the action shortcut to the user.
     */
    void appendItem(Action *action, String const &shortcutLabel = "");

    /**
     * Inserts an item into the menu.
     *
     * @param pos  Index of the new item.
     * @param action  Action to add as a shortcut for triggering the item.
     * @param shortcutLabel  Label to show, representing the action shortcut to the user.
     */
    void insertItem(int pos, Action *action, String const &shortcutLabel = "");

    void appendSeparator();

    void insertSeparator(int pos);

    void clear();

    void removeItem(int pos);

    Action &itemAction(int pos) const;

    int findLabel(String const &label) const;

    bool hasLabel(String const &label) const;

    void setCursor(int pos);

    void setCursorByLabel(String const &label);

    int cursor() const;

    /**
     * Allows or disallows the menu to close when receiving an unhandled control
     * key.
     *
     * @param canBeClosed  @c true to allow closing, @c false to disallow.
     */
    void setClosable(bool canBeClosed);

    void setSelectionAttribs(TextCanvas::Char::Attribs const &attribs);

    void setBackgroundAttribs(TextCanvas::Char::Attribs const &attribs);

    void setBorder(BorderStyle style);

    void setBorderAttribs(TextCanvas::Char::Attribs const &attribs);

    Vector2i cursorPosition() const;

    // Events.
    void draw();
    bool handleEvent(Event const &);

public slots:
    void open();
    void close();

signals:
    void closed();

private:
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_MENUWIDGET_H
