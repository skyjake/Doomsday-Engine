/** @file shell/menuwidget.h  Menu with shortcuts.
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

#ifndef LIBSHELL_MENUTEDGET_H
#define LIBSHELL_MENUTEDGET_H

#include "widget.h"
#include "action.h"
#include "textcanvas.h"

namespace de { namespace term {

/**
 * Menu with Action instances as items.
 *
 * The width of the widget is automatically determined based on how much space
 * is needed for the items and their possible shortcut labels. The height of
 * the widget depends on the number of items in the menu.
 *
 * Actions added to the menu are considered shortcuts and triggering them will
 * cause the menu to close (if it is closable).
 *
 * @ingroup textUi
 */
class DE_PUBLIC MenuWidget : public Widget
{
public:
    enum Preset {
        Popup,     ///< Menu initially hidden, will popup on demand.
        AlwaysOpen ///< Menu initially shown, stays open.
    };

    enum BorderStyle { NoBorder, LineBorder };

    DE_AUDIENCE(Close, void menuClosed())

public:
    MenuWidget(Preset preset, const String &name = {});

    int itemCount() const;

    /**
     * Appends an item into the menu as the last item.
     *
     * @param action  Action to add as a shortcut for triggering the item.
     * @param shortcutLabel  Label to show, representing the action shortcut to the user.
     */
    void appendItem(RefArg<Action> action, const String &shortcutLabel = "");

    /**
     * Inserts an item into the menu.
     *
     * @param pos  Index of the new item.
     * @param action  Action to add as a shortcut for triggering the item.
     * @param shortcutLabel  Label to show, representing the action shortcut to the user.
     */
    void insertItem(int pos, RefArg<Action> action, const String &shortcutLabel = "");

    void appendSeparator();

    void insertSeparator(int pos);

    void clear();

    void removeItem(int pos);

    Action &itemAction(int pos) const;

    int findLabel(const String &label) const;

    bool hasLabel(const String &label) const;

    void setCursor(int pos);

    void setCursorByLabel(const String &label);

    int cursor() const;

    /**
     * Allows or disallows the menu to close when receiving an unhandled control
     * key.
     *
     * @param canBeClosed  @c true to allow closing, @c false to disallow.
     */
    void setClosable(bool canBeClosed);

    void setSelectionAttribs(const TextCanvas::AttribChar::Attribs &attribs);

    void setBackgroundAttribs(const TextCanvas::AttribChar::Attribs &attribs);

    void setBorder(BorderStyle style);

    void setBorderAttribs(const TextCanvas::AttribChar::Attribs &attribs);

    Vec2i cursorPosition() const;

    // Events.
    void draw();
    bool handleEvent(const Event &);

    void open();
    void close();

private:
    DE_PRIVATE(d)
};

}} // namespace de::term

#endif // LIBSHELL_MENUTEDGET_H
