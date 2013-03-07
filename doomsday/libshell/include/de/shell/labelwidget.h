/** @file labelwidget.h  Widget for showing a label.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_LABELWIDGET_H
#define LIBSHELL_LABELWIDGET_H

#include "TextWidget"
#include "TextCanvas"

namespace de {
namespace shell {

/**
 * Widget for showing a text label.
 */
class LIBSHELL_PUBLIC LabelWidget : public TextWidget
{   
public:
    LabelWidget(String const &name = "");

    /**
     * Sets the background for the label.
     *
     * @param background  Character to fill the area of the widget with.
     */
    void setBackground(TextCanvas::Char const &background);

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
    void setLabel(String const &text, TextCanvas::Char::Attribs attribs = TextCanvas::Char::DefaultAttributes);

    void setAttribs(TextCanvas::Char::Attribs const &attribs);

    void setBackgroundAttribs(TextCanvas::Char::Attribs const &attribs);

    TextCanvas::Char::Attribs attribs() const;

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
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_LABELWIDGET_H
