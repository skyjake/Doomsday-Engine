/** @file scrollareawidget.h  Scrollable area.
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

#ifndef LIBAPPFW_SCROLLAREAWIDGET_H
#define LIBAPPFW_SCROLLAREAWIDGET_H

#include "../GuiWidget"

namespace de {

/**
 * Scrollable area.
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
 */
class LIBAPPFW_PUBLIC ScrollAreaWidget : public GuiWidget
{
    Q_OBJECT

public:
    enum Origin {
        Top,        ///< Scroll position 0 is at the top.
        Bottom      ///< Scroll position 0 is at the bottom.
    };

public:
    ScrollAreaWidget(String const &name = "");

    void setScrollBarColor(DotPath const &colorId);

    void setOrigin(Origin origin);
    Origin origin() const;

    void setIndicatorUv(Rectanglef const &uv);
    void setIndicatorUv(Vector2f const &uvPoint);

    void setContentWidth(int width);
    void setContentWidth(Rule const &width);
    void setContentHeight(int height);
    void setContentHeight(Rule const &height);
    void setContentSize(Rule const &width, Rule const &height);
    void setContentSize(Vector2i const &size);
    void setContentSize(Vector2ui const &size);

    void modifyContentWidth(int delta);
    void modifyContentHeight(int delta);

    int contentWidth() const;
    int contentHeight() const;

    RuleRectangle const &contentRule() const;

    ScalarRule &scrollPositionX() const;
    ScalarRule &scrollPositionY() const;
    Rule const &maximumScrollX() const;
    Rule const &maximumScrollY() const;

    bool isScrolling() const;

    Rectanglei viewport() const;
    Vector2i viewportSize() const;

    /**
     * Returns the current scroll XY position, with 0 being the top/left corner
     * and maximumScroll() being the bottom right position.
     */
    Vector2i scrollPosition() const;

    virtual Vector2i scrollPageSize() const;

    /**
     * Returns the maximum scroll position. The scrollMaxChanged() signal
     * is emitted whenever the maximum changes.
     */
    Vector2i maximumScroll() const;

    /**
     * Scrolls the view to a specified position. Position (0,0) means the top
     * left corner is visible at the top left corner of the ScrollAreaWidget.
     *
     * @param to  Scroll position.
     */
    void scroll(Vector2i const &to, TimeDelta span = 0);

    void scrollX(int to, TimeDelta span = 0);
    void scrollY(int to, TimeDelta span = 0);

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

    void glMakeScrollIndicatorGeometry(DefaultVertexBuf::Builder &verts,
                                       Vector2f const &origin = Vector2f(0, 0));

    // Events.
    void viewResized();
    void update();
    void drawContent();
    void preDrawChildren();
    void postDrawChildren();
    bool handleEvent(Event const &event);

public slots:
    void scrollToTop(TimeDelta span = .3f);

    /**
     * Moves the scroll offset of the widget to the bottom of the content.
     */
    void scrollToBottom(TimeDelta span = .3f);

    void scrollToLeft(TimeDelta span = .3f);
    void scrollToRight(TimeDelta span = .3f);

protected:
    void glInit();
    void glDeinit();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_SCROLLAREAWIDGET_H
