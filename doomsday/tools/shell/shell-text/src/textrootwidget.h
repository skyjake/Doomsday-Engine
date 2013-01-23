/** @file textrootwidget.h Text-based root widget.
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

#ifndef TEXTROOTWIDGET_H
#define TEXTROOTWIDGET_H

#include <de/RootWidget>
#include "textcanvas.h"

class TextWidget;

class TextRootWidget : public de::RootWidget
{
public:
    /**
     * Constructs a new text-based root widget.
     *
     * @param cv  Canvas for the root. Ownership taken. Size of the canvas
     *            is used as the root widget's size.
     */
    TextRootWidget(TextCanvas *cv);
    ~TextRootWidget();

    TextCanvas &rootCanvas();

    /**
     * Resizes the canvas and changes the view size.
     *
     * @param viewSize  New size.
     */
    void setViewSize(de::Vector2i const &viewSize);

    TextWidget *focus() const;

    void draw();

private:
    TextCanvas *_canvas;
};

#endif // TEXTROOTWIDGET_H
