/** @file buttonwidget.cpp  Clickable button widget.
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

#include "ui/widgets/buttonwidget.h"
#include "ui/widgets/guirootwidget.h"

using namespace de;

DENG2_PIMPL(ButtonWidget)
{
    Action *action;

    Instance(Public *i) : Base(i)
    {}
};

ButtonWidget::ButtonWidget(String const &name) : LabelWidget(name), d(new Instance(this))
{}

bool ButtonWidget::handleEvent(Event const &event)
{
    return false;
}

void ButtonWidget::makeAdditionalGeometry(AdditionalGeometryKind kind,
                                          VertexBuilder<Vertex2TexRgba>::Vertices &verts,
                                          ContentLayout const &layout)
{
    // Draw a frame for the button.
    if(kind == Background)
    {
        verts.makeQuad(rule().recti(), style().colors().colorf("background"),
                       root().atlas().imageRectf(root().solidWhitePixel()).middle());

        /*verts.makeFlexibleFrame(rule().rect(), 10, Vector4f(1, 1, 1, .5f),
                                root().atlas().imageRectf(root().roundCorners()));*/
    }
}

void ButtonWidget::updateModelViewProjection()
{
    // Apply a scale animation to indicate button response.
    LabelWidget::updateModelViewProjection();
}
