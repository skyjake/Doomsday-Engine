/** @file shell/labelwidget.h  Widget for showing a label.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_LABELTEDGET_H
#define LIBSHELL_LABELTEDGET_H

#include "widget.h"
#include "textcanvas.h"

namespace de { namespace term {

/**
 * Widget for showing a text label.
 *
 * @ingroup textUi
 */
class DE_PUBLIC LabelWidget : public Widget
{
public:
    LabelWidget(const String &name = String());

    /**
     * Sets the background for the label.
     *
     * @param background  Character to fill the area of the widget with.
     */
    void setBackground(const TextCanvas::AttribChar &background);

    /**
     * Allows or disallows the label to expand vertically to fit provided label.
     * By default, labels do not adjust their own size.
     *
     * @param expand  @c true to allow the widget to modify its own height.
     */
    void setExpandsToFitLines(bool expand);

    /**
     * Sets the label of the widget. The text is wrapped to fit the label's
     * rectangle.
     *
     * @param text     Text to show.
     * @param attribs  Attributes for the text.
     */
    void setLabel(
        const String &                  text,
        TextCanvas::AttribChar::Attribs attribs = TextCanvas::AttribChar::DefaultAttributes);

    void setAttribs(const TextCanvas::AttribChar::Attribs &attribs);

    void setBackgroundAttribs(const TextCanvas::AttribChar::Attribs &attribs);

    TextCanvas::AttribChar::Attribs attribs() const;

    /**
     * Sets the alignment of the label inside the widget's rectangle. The default
     * alignment is in the middle (horiz + vert).
     *
     * @param align  Alignment flags.
     */
    void setAlignment(Alignment align);

    /**
     * Returns the current label of the widget.
     */
    String label() const;

    void update();
    void draw();

private:
    DE_PRIVATE(d)
};

}} // namespace de::term

#endif // LIBSHELL_LABELTEDGET_H
