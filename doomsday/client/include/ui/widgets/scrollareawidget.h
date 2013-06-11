/** @file scrollareawidget.h  Scrollable area.
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

#ifndef DENG_CLIENT_SCROLLAREAWIDGET_H
#define DENG_CLIENT_SCROLLAREAWIDGET_H

#include "guiwidget.h"

/**
 * Scrollable area.
 */
class ScrollAreaWidget : public GuiWidget
{
    Q_OBJECT

public:
    enum Origin {
        Top,        ///< Scroll position 0 is at the top.
        Bottom      ///< Scroll position 0 is at the bottom.
    };

public:
    ScrollAreaWidget(de::String const &name = "");

    void setOrigin(Origin origin);
    Origin origin() const;

    void setIndicatorUv(de::Rectanglef const &uv);
    void setIndicatorUv(de::Vector2f const &uvPoint);

    void setContentWidth(int width);
    void setContentHeight(int height);
    void setContentSize(de::Vector2i const &size);

    void modifyContentWidth(int delta);
    void modifyContentHeight(int delta);

    int contentWidth() const;
    int contentHeight() const;

    de::RuleRectangle const &contentRule() const;

    de::ScalarRule &scrollPositionX() const;
    de::ScalarRule &scrollPositionY() const;
    de::Rule const &maximumScrollX() const;
    de::Rule const &maximumScrollY() const;

    de::Rectanglei viewport() const;
    de::Vector2i viewportSize() const;

    /**
     * Returns the amount of space between the top edge of the widget and the
     * top edge of the content.
     */
    int topMargin() const;

    int rightMargin() const;

    /**
     * Returns the current scroll XY position, with 0 being the top/left corner
     * and maximumScroll() being the bottom right position.
     */
    de::Vector2i scrollPosition() const;

    virtual de::Vector2i scrollPageSize() const;

    /**
     * Returns the maximum scroll position. The scrollMaxChanged() signal
     * is emitted whenever the maximum changes.
     */
    de::Vector2i maximumScroll() const;

    /**
     * Scrolls the view to a specified position. Position (0,0) means the top
     * left corner is visible at the top left corner of the ScrollAreaWidget.
     *
     * @param to  Scroll position.
     */
    void scroll(de::Vector2i const &to, de::TimeDelta span = 0);

    void scrollX(int to, de::TimeDelta span = 0);
    void scrollY(int to, de::TimeDelta span = 0);

    /**
     * Determines if the history view is at the bottom, showing the latest entry.
     */
    bool isAtBottom() const;

    /**
     * Enables or disables scrolling with Page Up/Down keys.
     *
     * @param enabled  @c true to enable Page Up/Down.
     */
    void enablePageKeys(bool enabled);

    void glMakeScrollIndicatorGeometry(DefaultVertexBuf::Builder &verts);

    // Events.
    void update();
    bool handleEvent(de::Event const &event);

public slots:
    void scrollToTop(de::TimeDelta span = .3f);

    /**
     * Moves the scroll offset of the widget to the bottom of the content.
     */
    void scrollToBottom(de::TimeDelta span = .3f);

    void scrollToLeft(de::TimeDelta span = .3f);
    void scrollToRight(de::TimeDelta span = .3f);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_SCROLLAREAWIDGET_H
