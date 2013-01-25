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

#ifndef LIBSHELL_TEXTROOTWIDGET_H
#define LIBSHELL_TEXTROOTWIDGET_H

#include <de/RootWidget>
#include "TextCanvas"

namespace de {
namespace shell {

class TextWidget;

/**
 * Root widget for device-independent, text-based UIs.
 *
 * As TextCanvas uses the concept of dirty characters to indicidate changes to
 * be drawn on the screen, the text root widget assumes that by default drawing
 * is unnecessary, and redraws must be requested by widgets when suitable.
 */
class TextRootWidget : public RootWidget
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

    void requestDraw();

    bool drawWasRequested() const;

    /**
     * Resizes the canvas and changes the view size.
     *
     * @param viewSize  New size.
     */
    void setViewSize(Vector2i const &viewSize);

    TextWidget *focus() const;

    void draw();

private:
    TextCanvas *_canvas;
    bool _drawRequest;
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_TEXTROOTWIDGET_H
