/** @file textwidget.h  Generic widget with a text-based visual.
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

#ifndef LIBSHELL_TEXTWIDGET_H
#define LIBSHELL_TEXTWIDGET_H

#include <de/Widget>
#include <de/RectangleRule>
#include <QObject>
#include "TextCanvas"

namespace de {
namespace shell {

class TextRootWidget;

/**
 * Generic widget with a text-based visual.
 *
 * It is assumed that the root widget under which text widgets are used is
 * derived from TextRootWidget.
 *
 * QObject is a base class for signals and slots.
 */
class TextWidget : public QObject, public Widget
{
    Q_OBJECT

public:
    TextWidget(String const &name = "");
    virtual ~TextWidget();

    TextRootWidget &root() const;

    void setTargetCanvas(TextCanvas *canvas);

    TextCanvas *targetCanvas() const;

    /**
     * Defines the placement of the widget on the target canvas.
     *
     * @param rule  Rectangle that the widget occupied.
     *              Widget takes ownership.
     */
    void setRule(RectangleRule *rule);

    RectangleRule &rule();

    /**
     * Returns the position of the cursor for the widget. If the widget
     * has focus, this is where the cursor will be positioned.
     *
     * @return Cursor position.
     */
    virtual Vector2i cursorPosition();

private:
    struct Instance;
    Instance *d;
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_TEXTWIDGET_H
