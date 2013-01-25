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

#ifndef TEXTWIDGET_H
#define TEXTWIDGET_H

#include <de/Widget>
#include <de/RectangleRule>
#include <QObject>
#include "textcanvas.h"

class TextRootWidget;

/**
 * Generic widget with a text-based visual.
 *
 * It is assumed that the root widget under which text widgets are used is
 * derived from TextRootWidget.
 *
 * QObject is a base class for signals and slots.
 */
class TextWidget : public QObject, public de::Widget
{
    Q_OBJECT

public:
    TextWidget(de::String const &name = "");
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
    void setRule(de::RectangleRule *rule);

    de::RectangleRule &rule();

    /**
     * Returns the position of the cursor for the widget. If the widget
     * has focus, this is where the cursor will be positioned.
     *
     * @return Cursor position.
     */
    virtual de::Vector2i cursorPosition();

private:
    struct Instance;
    Instance *d;
};

#endif // TEXTWIDGET_H
