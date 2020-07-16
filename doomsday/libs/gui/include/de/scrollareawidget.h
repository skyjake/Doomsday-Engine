/** @file scrollareawidget.h  Scrollable area.
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

#ifndef LIBAPPFW_SCROLLAREAWIDGET_H
#define LIBAPPFW_SCROLLAREAWIDGET_H

#include "de/guiwidget.h"

namespace de {

/**
 * Scrollable area.
 *
 * Provides a RuleRectangle where child widgets can be placed.
 *
 * ScrollAreaWidget does not control its own position or size. The user must
 * define its rectangle. The content rule rectangle is defined in relation to
 * the widget's rectangle.
 *
 * The user must always define the size of the content area.
 *
 * ScrollAreaWidget can optionally independently draw a scroll indicator.
 * However, the default behavior is that the user must call
 * glMakeScrollIndicatorGeometry() to include the indicator geometry as part of
 * the derived/owner widget.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC ScrollAreaWidget : public GuiWidget
{
public:
    enum Origin {
        Top,        ///< Scroll position 0 is at the top.
        Bottom      ///< Scroll position 0 is at the bottom.
    };

public:
    ScrollAreaWidget(const String &name = String());

    void setScrollBarColor(const DotPath &colorId);

    void setOrigin(Origin origin);
    Origin origin() const;

    void setIndicatorUv(const Rectanglef &uv);
    void setIndicatorUv(const Vec2f &uvPoint);

    void setContentWidth(int width);
    void setContentWidth(const Rule &width);
    void setContentHeight(int height);
    void setContentHeight(const Rule &height);
    void setContentSize(const Rule &width, const Rule &height);
    void setContentSize(const ISizeRule &dimensions);
    void setContentSize(const Vec2i &size);
    void setContentSize(const Vec2ui &size);

    void modifyContentWidth(int delta);
    void modifyContentHeight(int delta);

    int contentWidth() const;
    int contentHeight() const;

    const RuleRectangle &contentRule() const;

    AnimationRule &scrollPositionX() const;
    AnimationRule &scrollPositionY() const;
    const Rule &maximumScrollX() const;
    const Rule &maximumScrollY() const;

    bool isScrolling() const;

    Rectanglei viewport() const;
    Vec2i viewportSize() const;

    /**
     * Returns the current scroll XY position, with 0 being the top/left corner
     * and maximumScroll() being the bottom right position.
     */
    Vec2i scrollPosition() const;

    virtual Vec2i scrollPageSize() const;

    /**
     * Returns the maximum scroll position. The scrollMaxChanged() signal
     * is emitted whenever the maximum changes.
     */
    Vec2i maximumScroll() const;

    /**
     * Scrolls the view to a specified position. Position (0,0) means the top
     * left corner is visible at the top left corner of the ScrollAreaWidget.
     *
     * @param to    Scroll position.
     * @param span  Animation time span.
     */
    void scroll(const Vec2i &to, const TimeSpan& span = 0.0);

    void scrollX(int to, TimeSpan span = 0.0);
    void scrollY(int to, TimeSpan span = 0.0);
    void scrollY(const Rule &to, TimeSpan span = 0.0);

    bool isScrollable() const;

    /**
     * Determines if the history view is at the bottom, showing the latest entry.
     */
    bool isAtBottom() const;

    /**
     * Enables or disables scrolling. By default, scrolling is enabled.
     *
     * @param enabled  @c true to enable scrolling.
     */
    void enableScrolling(bool enabled);

    /**
     * Enables or disables scrolling with Page Up/Down keys.
     *
     * @param enabled  @c true to enable Page Up/Down.
     */
    void enablePageKeys(bool enabled);

    /**
     * Enables or disables the drawing of the scroll indicator.
     *
     * @param enabled  @c true to enable the indicator. The default is @c false.
     */
    void enableIndicatorDraw(bool enabled);

    void glMakeScrollIndicatorGeometry(GuiVertexBuilder &verts,
                                       const Vec2f &origin = Vec2f(0, 0));

    // Events.
    //void viewResized() override;
    void update() override;
    void drawContent() override;
    bool handleEvent(const Event &event) override;

    void scrollToTop(const TimeSpan& span = 0.3);

    /**
     * Moves the scroll offset of the widget to the bottom of the content.
     */
    void scrollToBottom(const TimeSpan& span = 0.3);

    void scrollToLeft(TimeSpan span = 0.3);
    void scrollToRight(TimeSpan span = 0.3);

    /**
     * Moves the scroll offset to center on the given widget.
     *
     * @param widget  Widget to center on.
     * @param span    Animation duration.
     */
    void scrollToWidget(const GuiWidget &widget, TimeSpan span = 0.3);

    /**
     * Finds the topmost scroll area that can be scrolled. May return this widget
     * if there are no scrollable ancestors.
     *
     * @return
     */
    ScrollAreaWidget &findTopmostScrollable();

protected:
    void glInit() override;
    void glDeinit() override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_SCROLLAREAWIDGET_H
