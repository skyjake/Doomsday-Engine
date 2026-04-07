/** @file popupwidget.h  Widget that pops up a child widget.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/panelwidget.h"
#include "de/buttonwidget.h"
#include "ui/defs.h"
#include <de/rulerectangle.h>

namespace de {

/**
 * Popup anchored to a specified point. Extends PanelWidget with positioning
 * and an anchor graphic.
 *
 * Initially a popup widget is in a hidden state.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC PopupWidget : public PanelWidget
{
public:
    PopupWidget(const String &name = {});

    /**
     * Determines how deeply nested this popup is within parent popups.
     *
     * @return 0, if parents include no other popups. Otherwise +1 for each popup
     * present in the parents (i.e., number of popups among ancestors).
     */
    int levelOfNesting() const;

    void setAnchorAndOpeningDirection(const RuleRectangle &rule, ui::Direction dir);

    /**
     * Enables or disables the popup to flip the opening direction if there
     * isn't enough room in the main direction. This is enabled by default.
     *
     * @param flex  @c true to be flexible, otherwise @c false.
     */
    void setAllowDirectionFlip(bool flex);

    void setAnchor(const Vec2i &pos);
    void setAnchorX(int xPos);
    void setAnchorY(int yPos);
    void setAnchor(const Rule &x, const Rule &y);
    void setAnchorX(const Rule &x);
    void setAnchorY(const Rule &y);

    const RuleRectangle &anchor() const;

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
     * @param clickCloses  @c true to allow closing with a click.
     */
    void setClickToClose(bool clickCloses);

    /**
     * Sets the style of the popup to the one used for informational popups
     * rather than interactive (the default) ones.
     */
    void useInfoStyle(bool yes = true);

    bool isUsingInfoStyle();

    Background infoStyleBackground() const;

    void setColorTheme(ColorTheme theme);

    ColorTheme colorTheme() const;

    /**
     * Sets the color of the popup outline. By default, popups do not have an outline.
     *
     * @param outlineColor  Outline color.
     */
    void setOutlineColor(const DotPath &outlineColor);

    void setCloseButtonVisible(bool enable);

    /**
     * Returns the close button of the popup.
     *
     * By default no close button is created. Calling this will cause the button to
     * be created.
     */
    ButtonWidget &closeButton();

    // Events.
    void offerFocus() override;
    bool handleEvent(const Event &event) override;

protected:
    void glMakeGeometry(GuiVertexBuilder &verts) override;
    void updateStyle() override;

    void preparePanelForOpening() override;
    void panelClosing() override;
    void panelDismissed() override;

private:
    DE_PRIVATE(d)
};

template <typename ClassName>
PopupWidget *makePopup() { return new ClassName; }

} // namespace de

#endif // LIBAPPFW_POPUPWIDGET_H
