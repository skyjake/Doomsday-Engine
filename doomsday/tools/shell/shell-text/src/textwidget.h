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
#include "textcanvas.h"

class TextRootWidget;

/**
 * Generic widget with a text-based visual.
 *
 * It is assumed that the root widget under which text widgets are used is
 * derived from TextRootWidget.
 */
class TextWidget : public de::Widget
{
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

private:
    struct Instance;
    Instance *d;
};

#endif // TEXTWIDGET_H
