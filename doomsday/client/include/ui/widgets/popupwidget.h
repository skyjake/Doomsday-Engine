/** @file popupwidget.h  Widget that pops up a child widget.
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

#ifndef DENG_CLIENT_POPUPWIDGET_H
#define DENG_CLIENT_POPUPWIDGET_H

#include "scrollareawidget.h"
#include "alignment.h"

/**
 * Popup anchored to a specified point.
 *
 * Initially a popup widget is in a hidden state.
 */
class PopupWidget : public GuiWidget
{
    Q_OBJECT

public:
    PopupWidget(de::String const &name = "");

    /**
     * Sets the content widget of the popup. If there is an earlier
     * content widget, it will be destroyed.
     *
     * @param content  Content widget. PopupWidget takes ownership.
     *                 @a content becomes a child of the popup.
     */
    void setContent(GuiWidget *content);

    GuiWidget &content() const;

    void setAnchor(de::Vector2i const &pos);
    void setAnchor(de::Rule const &x, de::Rule const &y);

    /**
     * Sets the preferred opening direction of the popup. If there isn't enough
     * room in the specified direction, the opposite direction is used instead.
     *
     * @param dir  Opening direction.
     */
    void setOpeningDirection(ui::Direction dir);

    // Events.
    void viewResized();
    void update();
    void preDrawChildren();
    void postDrawChildren();
    bool handleEvent(de::Event const &event);

public slots:
    /**
     * Opens the popup, positioning it appropriately so that is anchored to the
     * position specified with setAnchor().
     */
    void open();

    /**
     * Closes the popup. The widget is dismissed once the closing animation
     * completes.
     */
    void close();

    /**
     * Immediately hides the popup.
     */
    void dismiss();

signals:
    void opened();
    void closed();

protected:
    void glInit();
    void glDeinit();
    void drawContent();
    void glMakeGeometry(DefaultVertexBuf::Builder &verts);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_POPUPWIDGET_H
