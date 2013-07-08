/** @file notificationwidget.cpp  Notification area.
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

#include "ui/widgets/notificationwidget.h"
#include "ui/widgets/guirootwidget.h"

#include <de/Drawable>
#include <de/Matrix>

using namespace de;

DENG2_PIMPL(NotificationWidget)
{
    // GL objects:
    typedef DefaultVertexBuf VertexBuf;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;

    Instance(Public *i)
        : Base(i),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4)
    {
        updateStyle();
    }

    void updateStyle()
    {
        self.set(Background(self.style().colors().colorf("background")));
    }

    void glInit()
    {
        drawable.addBuffer(new VertexBuf);

        self.root().shaders().build(drawable.program(), "generic.color_ucolor")
                << uMvpMatrix << uColor;
    }

    void glDeinit()
    {
        drawable.clear();
    }

    void updateGeometry()
    {
        Rectanglei pos;
        if(self.hasChangedPlace(pos) || self.geometryRequested())
        {
            self.requestGeometry(false);

            VertexBuf::Builder verts;
            self.glMakeGeometry(verts);
            drawable.buffer<VertexBuf>().setVertices(gl::TriangleStrip, verts, gl::Static);
        }
    }

    void updateChildLayout()
    {
        Rule const &outer = self.style().rules().rule("gap");
        Rule const &inner = self.style().rules().rule("unit");

        Rule const *totalWidth = 0;
        Rule const *totalHeight = 0;

        WidgetList const children = self.Widget::children();
        for(int i = 0; i < children.size(); ++i)
        {
            GuiWidget &w = children[i]->as<GuiWidget>();

            // The children are laid out simply in a row from right to left.
            w.rule().setInput(Rule::Top, self.rule().top() + outer);
            if(i > 0)
            {
                w.rule().setInput(Rule::Right, children[i - 1]->as<GuiWidget>().rule().left() - inner);
                changeRef(totalWidth, *totalWidth + inner + w.rule().width());
            }
            else
            {
                w.rule().setInput(Rule::Right, self.rule().right() - outer);
                totalWidth = holdRef(w.rule().width());
            }

            if(!totalHeight)
            {
                totalHeight = holdRef(w.rule().height());
            }
            else
            {
                changeRef(totalHeight, OperatorRule::maximum(*totalHeight, w.rule().height()));
            }
        }

        // Update the total size of the notification area.
        self.rule()
                .setInput(Rule::Width,  *totalWidth  + outer * 2)
                .setInput(Rule::Height, *totalHeight + outer * 2);

        releaseRef(totalWidth);
        releaseRef(totalHeight);
    }
};

NotificationWidget::NotificationWidget(String const &name) : d(new Instance(this))
{
    // Initially the widget is empty.
    rule().setSize(Const(0), Const(0));
    hide();
}

void NotificationWidget::viewResized()
{
    d->uMvpMatrix = root().projMatrix2D();
}

void NotificationWidget::drawContent()
{
    d->updateGeometry();

    d->uColor = Vector4f(1, 1, 1, visibleOpacity());
    d->drawable.draw();
}

void NotificationWidget::glInit()
{
    d->glInit();
}

void NotificationWidget::glDeinit()
{
    d->glDeinit();
}

