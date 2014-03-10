/** @file popupwidget.h  Widget that pops up a child widget.
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

#ifndef LIBAPPFW_POPUPWIDGET_H
#define LIBAPPFW_POPUPWIDGET_H

#include "../PanelWidget"
#include "../ui/defs.h"

namespace de {

/**
 * Popup anchored to a specified point. Extends PanelWidget with positioning
 * and an anchor graphic.
 *
 * Initially a popup widget is in a hidden state.
 */
class LIBAPPFW_PUBLIC PopupWidget : public PanelWidget
{
    Q_OBJECT

public:
    PopupWidget(String const &name = "");

    /**
     * Determines how deeply nested this popup is within parent popups.
     *
     * @return 0, if parents include no other popups. Otherwise +1 for each popup
     * present in the parents (i.e., number of popups among ancestors).
     */
    int levelOfNesting() const;

    void setAnchorAndOpeningDirection(RuleRectangle const &rule, ui::Direction dir);

    void setAnchor(Vector2i const &pos);
    void setAnchorX(int xPos);
    void setAnchorY(int yPos);
    void setAnchor(Rule const &x, Rule const &y);
    void setAnchorX(Rule const &x);
    void setAnchorY(Rule const &y);

    Rule const &anchorX() const;
    Rule const &anchorY() const;

    /**
     * Replace the anchor with rules of matching constant value.
     */
    void detachAnchor();

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

    bool isUsingInfoStyle();

    Background infoStyleBackground() const;

    // Events.
    bool handleEvent(Event const &event);

protected:
    void glMakeGeometry(DefaultVertexBuf::Builder &verts);
    void updateStyle();

    virtual void preparePanelForOpening();
    virtual void panelDismissed();

private:
    DENG2_PRIVATE(d)
};

template <typename ClassName>
PopupWidget *makePopup() { return new ClassName; }

} // namespace de

#endif // LIBAPPFW_POPUPWIDGET_H
