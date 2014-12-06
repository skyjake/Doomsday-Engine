/** @file documentwidget.h  Widget for displaying large amounts of text.
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

#ifndef LIBAPPFW_DOCUMENTWIDGET_H
#define LIBAPPFW_DOCUMENTWIDGET_H

#include "../ScrollAreaWidget"
#include "../ui/defs.h"

namespace de {

/**
 * Widget for displaying large amounts of text.
 *
 * Based on ScrollAreaWidget for scrolling. Only the visible portion of the
 * source text is kept ready for drawing -- the length of the source document
 * is thus unlimited. Only vertical scrolling is supported at the moment.
 *
 * DocumentWidget can be configured to expand horizontally, or it can use a
 * certain determined fixed width (see DocumentWidget::setWidthPolicy()).
 *
 * By default, the height of the widget is determined by its content size.
 *
 * The assumption is that the source document is largely static so that once
 * prepared, the GL resources can be reused as many times as possible.
 */
class LIBAPPFW_PUBLIC DocumentWidget : public ScrollAreaWidget
{
public:
    DocumentWidget(String const &name = "");

    /**
     * Sets the text content of the widget. Style escapes can be used.
     *
     * @param styledText  Text content.
     */
    void setText(String const &styledText);

    /**
     * Sets the policy for managing the widget's width.
     * - ui::Fixed means that the widget's width must be defined externally,
     *   and the width is also used as the content's width.
     * - ui::Expand means that the widget's width automatically expands
     *   to fit the entire content, but the maximum line width is never
     *   exceeded.
     *
     * The default policy is ui::Expand.
     *
     * @param policy  Size policy for the widget's width.
     */
    void setWidthPolicy(ui::SizePolicy policy);

    /**
     * Sets the maximum line width when using ui::Expand as the width policy.
     *
     * @param maxWidth  Maximum width of a text line.
     */
    void setMaximumLineWidth(int maxWidth);

    // Events.
    void viewResized();
    void drawContent();

protected:
    void glInit();
    void glDeinit();
    void glMakeGeometry(DefaultVertexBuf::Builder &verts);
    void updateStyle();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_DOCUMENTWIDGET_H
