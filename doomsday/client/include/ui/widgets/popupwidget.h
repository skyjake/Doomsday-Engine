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

#include "panelwidget.h"
#include "../uidefs.h"

/**
 * Popup anchored to a specified point. Extends PanelWidget with positioning
 * and an anchor graphic.
 *
 * Initially a popup widget is in a hidden state.
 */
class PopupWidget : public PanelWidget
{
    Q_OBJECT

public:
    PopupWidget(de::String const &name = "");

    /**
     * Determines how deeply nested this popup is within parent popups.
     *
     * @return 0, if parents include no other popups. Otherwise +1 for each popup
     * present in the parents (i.e., number of popups among ancestors).
     */
    int levelOfNesting() const;

    void setAnchorAndOpeningDirection(de::RuleRectangle const &rule, ui::Direction dir);

    void setAnchor(de::Vector2i const &pos);
    void setAnchorX(int xPos);
    void setAnchorY(int yPos);
    void setAnchor(de::Rule const &x, de::Rule const &y);
    void setAnchorX(de::Rule const &x);
    void setAnchorY(de::Rule const &y);

    de::Rule const &anchorX() const;
    de::Rule const &anchorY() const;

    /**
     * Tells the popup to delete itself after being dismissed. The default is that
     * the popup does not get deleted.
     *
     * @param deleteAfterDismiss  @c true to delete after dismissal.
     */
    void setDeleteAfterDismissed(bool deleteAfterDismiss);

    /**
     * Lets the popup be closed when the user clicks with the mouse outside the
     * popup area. This is @c true by default.
     *
     * @param clickClose  @c true to allow closing with a click.
     */
    void setClickToClose(bool clickCloses);

    /**
     * Sets the style of the popup to the one used for informational popups
     * rather than interactive (the default) ones.
     */
    void useInfoStyle();

    // Events.
    bool handleEvent(de::Event const &event);

protected:
    void glMakeGeometry(DefaultVertexBuf::Builder &verts);
    void updateStyle();

    virtual void preparePanelForOpening();
    virtual void panelDismissed();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_POPUPWIDGET_H
