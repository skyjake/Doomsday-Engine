/** @file relaywidget.h  Relays drawing and events.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_RELAYWIDGET_H
#define LIBAPPFW_RELAYWIDGET_H

#include "de/guiwidget.h"

namespace de {

/**
 * Relays drawing and events to another widget.
 *
 * @ingroup guiWidgets
 */
class RelayWidget : public GuiWidget
{
public:
    RelayWidget(GuiWidget *target = nullptr, const String &name = String());

    /**
     * Sets the widget that will be drawn when the relay widget is supposed to be
     * drawn, and that gets all events received by the relay widget.
     *
     * The @a target can be deleted while being a target of a relay.
     *
     * @param target  Relay target. Ownership not taken.
     */
    void setTarget(GuiWidget *target);

    GuiWidget *target() const;

    void initialize();
    void deinitialize();
    void viewResized();
    void update();
    bool handleEvent(const Event &event);
    bool hitTest(const Vec2i &pos) const;
    void drawContent();

public:
    /**
     * Notified when the target of the relay is about to be deleted. The target
     * still exists when this method is called.
     */
    DE_AUDIENCE(Target, void relayTargetBeingDeleted(RelayWidget &))

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_RELAYWIDGET_H
