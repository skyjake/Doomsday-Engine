/** @file panelwidget.h
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

#ifndef DENG_CLIENT_PANELWIDGET_H
#define DENG_CLIENT_PANELWIDGET_H

#include "../uidefs.h"
#include "scrollareawidget.h"

/**
 * Panel that can be opened or closed.
 *
 * A panel always has a single child as the content widget. This content widget
 * may in turn contain several widgets, though. The size of the content widget
 * determines the size of the panel. The user must define the position of the
 * panel.
 *
 * Initially panels are in the open state.
 */
class PanelWidget : public GuiWidget
{
    Q_OBJECT

public:
    /**
     * Audience to be notified when the panel is closing.
     */
    DENG2_DEFINE_AUDIENCE(Close, void panelBeingClosed(PanelWidget &))

public:
    PanelWidget(de::String const &name = "");

    /**
     * Sets the content widget of the panel. If there is an earlier content
     * widget, it will be destroyed.
     *
     * @param content  Content widget. PanelWidget takes ownership.
     *                 @a content becomes a child of the panel.
     */
    void setContent(GuiWidget *content);

    GuiWidget &content() const;

    /**
     * Sets the opening direction of the panel.
     *
     * @param dir  Opening direction.
     */
    void setOpeningDirection(ui::Direction dir);

    ui::Direction openingDirection() const;

    bool isOpen() const;

    void close(de::TimeDelta delayBeforeClosing);

    // Events.
    void viewResized();
    void update();
    void preDrawChildren();
    void postDrawChildren();

public slots:
    /**
     * Opens the panel, positioning it appropriately so that is anchored to the
     * position specified with setAnchor().
     */
    void open();

    /**
     * Closes the panel. The widget is dismissed once the closing animation
     * completes.
     */
    void close();

    /**
     * Immediately hides the panel.
     */
    void dismiss();

signals:
    void opened();
    void closed();
    void dismissed();

protected:
    void glInit();
    void glDeinit();
    void drawContent();
    void glMakeGeometry(DefaultVertexBuf::Builder &verts);

    virtual void preparePanelForOpening();
    virtual void panelClosing();
    virtual void panelDismissed();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_PANELWIDGET_H
