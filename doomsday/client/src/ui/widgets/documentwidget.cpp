/** @file documentwidget.cpp  Widget for displaying large amounts of text.
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

#include "ui/widgets/documentwidget.h"
#include "ui/widgets/guirootwidget.h"
#include "ui/widgets/gltextcomposer.h"

#include <de/Font>
#include <de/Drawable>

using namespace de;

static int const ID_BACKGROUND = 1; // does not scroll
static int const ID_TEXT = 2; // scrolls

DENG2_PIMPL(DocumentWidget)
{
    typedef DefaultVertexBuf VertexBuf;

    ui::SizePolicy widthPolicy;
    int maxLineWidth;
    String styledText;
    String text;
    Font::RichFormat format;
    FontLineWrapping wraps;
    GLTextComposer composer;
    Drawable drawable;
    Matrix4f modelMatrix;
    GLUniform uMvpMatrix;
    GLUniform uScrollMvpMatrix;
    GLUniform uColor;

    Instance(Public *i)
        : Base(i),
          widthPolicy(ui::Expand),
          maxLineWidth(1000),
          uMvpMatrix      ("uMvpMatrix", GLUniform::Mat4),
          uScrollMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor          ("uColor",     GLUniform::Vec4)
    {
        updateStyle();
        composer.setWrapping(wraps);
    }

    void updateStyle()
    {
        wraps.setFont(self.font());
    }

    void glInit()
    {        
        composer.setAtlas(self.root().atlas());
        composer.setText(text, format);

        drawable.addBuffer(ID_BACKGROUND, new VertexBuf);
        drawable.addBuffer(ID_TEXT,       new VertexBuf);

        self.root().shaders().build(drawable.program(), "generic.textured.color_ucolor")
                << uMvpMatrix << uColor << self.root().uAtlas();

        self.root().shaders().build(drawable.addProgram(ID_TEXT), "generic.textured.color_ucolor")
                << uScrollMvpMatrix << uColor << self.root().uAtlas();
        drawable.setProgram(ID_TEXT, drawable.program(ID_TEXT));
    }

    void glDeinit()
    {
        composer.release();
        drawable.clear();
    }

    void updateGeometry()
    {
        // TODO: If scroll position has changed, must update text geometry.

        Rectanglei pos;
        if(!self.hasChangedPlace(pos) && !self.geometryRequested())
        {
            // No need to change anything.
            return;
        }

        int margin = self.margin().valuei();

        // Make sure the text has been wrapped for the current dimensions.
        if(widthPolicy == ui::Expand)
        {
            if(wraps.isEmpty() || wraps.maximumWidth() != maxLineWidth)
            {
                wraps.wrapTextToWidth(text, format, maxLineWidth);
                self.setContentWidth(wraps.width());
                self.setContentHeight(wraps.totalHeightInPixels());
            }
        }
        else
        {
            int contentWidth = self.rule().width().valuei() - 2 * margin;
            if(wraps.isEmpty() || wraps.maximumWidth() != contentWidth)
            {
                wraps.wrapTextToWidth(text, format, contentWidth);
                self.setContentHeight(wraps.totalHeightInPixels());
            }
        }

        // Determine visible range of lines.
        Font const &font = self.font();
        int contentHeight = self.rule().height().valuei() - 2 * margin;

        composer.update();

        VertexBuf::Builder verts;
        self.glMakeGeometry(verts);
        drawable.buffer<VertexBuf>(ID_BACKGROUND).setVertices(gl::TriangleStrip, verts, gl::Static);

        // Geometry from the text composer.
        if(composer.isReady())
        {
            verts.clear();
            composer.makeVertices(verts, Vector2i(0, 0), ui::AlignLeft);
            drawable.buffer<VertexBuf>(ID_TEXT).setVertices(gl::TriangleStrip, verts, gl::Static);
        }

        uMvpMatrix = self.root().projMatrix2D();
        uScrollMvpMatrix = self.root().projMatrix2D() *
                Matrix4f::translate(Vector2f(self.contentRule().left().valuei(),
                                             self.contentRule().top().valuei()));

        // Geometry is now up to date.
        self.requestGeometry(false);
    }

    void draw()
    {
        updateGeometry();

        uColor = Vector4f(1, 1, 1, self.visibleOpacity());

        drawable.draw();
    }
};

DocumentWidget::DocumentWidget(String const &name) : d(new Instance(this))
{
    setWidthPolicy(ui::Expand);
}

void DocumentWidget::setText(String const &styledText)
{
    if(styledText != d->styledText)
    {
        d->styledText = styledText;
        d->text = d->format.initFromStyledText(styledText);
        d->wraps.clear();

        requestGeometry();
    }
}

void DocumentWidget::setWidthPolicy(ui::SizePolicy policy)
{
    d->widthPolicy = policy;

    if(policy == ui::Expand)
    {
        rule().setInput(Rule::Width, contentRule().width() + 2 * margin());
    }
    else
    {
        rule().clearInput(Rule::Width);
    }

    requestGeometry();
}

void DocumentWidget::setMaximumLineWidth(int maxWidth)
{
    d->maxLineWidth = maxWidth;
    requestGeometry();
}

void DocumentWidget::viewResized()
{
    d->uMvpMatrix = root().projMatrix2D();
    requestGeometry();
}

void DocumentWidget::update()
{
    ScrollAreaWidget::update();
}

void DocumentWidget::drawContent()
{
    d->draw();
}

bool DocumentWidget::handleEvent(Event const &event)
{
    return ScrollAreaWidget::handleEvent(event);
}

void DocumentWidget::glInit()
{
    d->glInit();
}

void DocumentWidget::glDeinit()
{
    d->glDeinit();
}
