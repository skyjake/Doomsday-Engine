/** @file panelwidget.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_PANELWIDGET_H
#define LIBAPPFW_PANELWIDGET_H

#include "../ui/defs.h"
#include "../ScrollAreaWidget"

namespace de {

/**
 * Panel that can be opened or closed.
 *
 * A panel always has a single child as the content widget. This content widget
 * may in turn contain several widgets, though. The size of the content widget
 * determines the size of the panel. The user must define the position of the
 * panel.
 *
 * Initially panels are in the closed state. They can be opened once the
 * content widget has been set.
 */
class LIBAPPFW_PUBLIC PanelWidget : public GuiWidget
{
    Q_OBJECT

public:
    /**
     * Audience to be notified when the panel is closing.
     */
    DENG2_DEFINE_AUDIENCE(Close, void panelBeingClosed(PanelWidget &))

public:
    PanelWidget(String const &name = "");

    /**
     * Sets the size policy for the secondary dimension. For instance, for a
     * panel that opens horizontally, this determines what is done to the
     * widget height.
     *
     * - ui::Expand (the default) means that the widget automatically uses the
     *   content's size for the secondary dimension.
     * - ui::Fixed means that the user is expected to define the panel's secondary
     *   dimension and the panel does not touch it.
     *
     * @param policy  Size policy.
     */
    void setSizePolicy(ui::SizePolicy policy);

    /**
     * Sets the content widget of the panel. If there is an earlier content
     * widget, it will be destroyed.
     *
     * @param content  Content widget. PanelWidget takes ownership.
     *                 @a content becomes a child of the panel.
     */
    void setContent(GuiWidget *content);

    GuiWidget &content() const;

    GuiWidget *takeContent();

    /**
     * Sets the opening direction of the panel.
     *
     * @param dir  Opening direction.
     */
    void setOpeningDirection(ui::Direction dir);

    ui::Direction openingDirection() const;

    bool isOpen() const;

    void close(TimeDelta delayBeforeClosing);

    // Events.
    void viewResized();
    void update();
    void preDrawChildren();
    void postDrawChildren();
    bool handleEvent(Event const &event);

public slots:
    /**
     * Opens the panel, positioning it appropriately so that is anchored to the
     * position specified with setAnchor().
     */
    virtual void open();

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

} // namespace de

#endif // LIBAPPFW_PANELWIDGET_H
