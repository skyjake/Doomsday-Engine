/** @file lineeditwidget.cpp
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

#include "ui/widgets/lineeditwidget.h"
#include "ui/widgets/fontlinewrapping.h"
#include "ui/widgets/guirootwidget.h"
#include "ui/widgets/gltextcomposer.h"
#include "ui/style.h"

#include <de/KeyEvent>
#include <de/ScalarRule>
#include <de/Drawable>

using namespace de;

static TimeDelta const ANIM_SPAN = .5f;

DENG2_PIMPL(LineEditWidget),
DENG2_OBSERVES(Atlas, Reposition)
{
    typedef GLBufferT<Vertex2TexRgba> VertexBuf;

    ScalarRule *height;
    FontLineWrapping &wraps;

    // Style.
    Font const *font;
    int margin;

    // GL objects.
    bool needGeometry;
    GLTextComposer composer;
    VertexBuf *buf;
    Id bgTex;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    GLUniform uTex;

    Instance(Public *i)
        : Base(i),
          wraps(static_cast<FontLineWrapping &>(i->lineWraps())),
          font(0),
          margin(0),
          needGeometry(false),
          buf(0),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4),
          uTex      ("uTex",       GLUniform::Sampler2D)
    {
        height = new ScalarRule(0);
        updateStyle();

        uColor = Vector4f(1, 1, 1, 1);
    }

    ~Instance()
    {
        releaseRef(height);
    }

    /**
     * Update the style used by the widget from the current UI style.
     */
    void updateStyle()
    {
        Style const &st = self.style();

        font   = &st.fonts().font("editor.plaintext");
        margin = st.rules().rule("gap").valuei();

        // Update the line wrapper's font.
        wraps.setFont(*font);
        wraps.clear();

        contentChanged();
    }

    int calculateHeight()
    {
        int const hgt = de::max(font->height().valuei(), wraps.totalHeightInPixels());
        return hgt + 2 * margin;
    }

    void updateProjection()
    {
        uMvpMatrix = self.root().projMatrix2D();
    }

    AtlasTexture &atlas()
    {
        return self.root().atlas();
    }

    void glInit()
    {
        composer.setAtlas(atlas());
        composer.setText(self.text(), wraps);

        // We'll be using the shared atlas.
        uTex = atlas();

        // Temporary background texture for development...
        QImage bg(QSize(2, 2), QImage::Format_ARGB32);
        bg.fill(QColor(0, 0, 255, 255).rgba());
        bgTex = atlas().alloc(bg);

        drawable.addBuffer(buf = new VertexBuf);

        self.root().shaders().build(drawable.program(), "generic.tex_color")
                << uMvpMatrix
                //<< uColor
                << uTex;

        updateProjection();
    }

    void glDeinit()
    {
        atlas().release(bgTex);
        composer.release();
    }

    void updateGeometry()
    {
        composer.update();

        Rectanglei pos;
        if(!self.checkPlace(pos) && !needGeometry)
        {
            return;
        }
        needGeometry = false;

        Vector4f bgColor = self.style().colors().colorf("background");

        VertexBuf::Vertices verts;
        VertexBuf::Type v;

        v.rgba     = Vector4f(1, 1, 1, 1); //bgColor;

        v.texCoord = atlas().imageRectf(bgTex).middle();

        v.pos = pos.topLeft;      verts << v;
        v.pos = pos.topRight();   verts << v;
        v.pos = pos.bottomLeft(); verts << v;
        v.pos = pos.bottomRight;  verts << v;

        // Text lines.
        composer.makeVertices(verts, pos.shrunk(margin),
                              GLTextComposer::AlignCenter,
                              GLTextComposer::AlignLeft);

        buf->setVertices(gl::TriangleStrip, verts, gl::Static);
    }

    void contentChanged()
    {
        needGeometry = true;
        composer.setText(self.text(), wraps);
    }

    void atlasContentRepositioned(Atlas &)
    {
        needGeometry = true;
    }
};

LineEditWidget::LineEditWidget(String const &name)
    : GuiWidget(name),
      AbstractLineEditor(new FontLineWrapping),
      d(new Instance(this))
{
    setBehavior(HandleEventsOnlyWhenFocused);

    // The widget's height is tied to the number of lines.
    rule().setInput(Rule::Height, *d->height);
}

void LineEditWidget::glInit()
{
    LOG_AS("LineEditWidget");
    d->glInit();
}

void LineEditWidget::glDeinit()
{
    d->glDeinit();
}

void LineEditWidget::viewResized()
{
    updateLineWraps(RewrapNow);
    d->updateProjection();
}

void LineEditWidget::update()
{
    GuiWidget::update();
    updateLineWraps(WrapUnlessWrappedAlready);
}

void LineEditWidget::draw()
{
    d->updateGeometry();
    d->drawable.draw();
}

bool LineEditWidget::handleEvent(Event const &event)
{
    if(event.type() == Event::KeyPress)
    {
        KeyEvent const &key = static_cast<KeyEvent const &>(event);

        // Insert text?
        if(!key.text().isEmpty())
        {
            // Insert some text into the editor.
            insert(key.text());
            return true;
        }
        else
        {
            // Control character.
            if(handleControlKey(key.qtKey()))
                return true;
        }
    }
    return GuiWidget::handleEvent(event);
}

int LineEditWidget::maximumWidth() const
{
    return rule().recti().width() - 2 * d->margin;
}

void LineEditWidget::numberOfLinesChanged(int /*lineCount*/)
{
    // Changes in the widget's height are animated.
    d->height->set(d->calculateHeight(), ANIM_SPAN);

    d->contentChanged();
}

void LineEditWidget::cursorMoved()
{
    d->needGeometry = true;
}

void LineEditWidget::contentChanged()
{
    d->contentChanged();
}
