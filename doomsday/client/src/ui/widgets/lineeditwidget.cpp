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
static duint const ID_BUF_TEXT   = 1;
static duint const ID_BUF_CURSOR = 2;

DENG2_PIMPL(LineEditWidget),
DENG2_OBSERVES(Atlas, Reposition)
{
    typedef GLBufferT<Vertex2TexRgba> VertexBuf;

    ScalarRule *height;
    FontLineWrapping &wraps;

    // Style.
    Font const *font;
    int margin;
    Time blinkTime;

    // GL objects.
    bool needGeometry;
    GLTextComposer composer;
    Id bgTex;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;

    Instance(Public *i)
        : Base(i),
          wraps(static_cast<FontLineWrapping &>(i->lineWraps())),
          font(0),
          margin(0),
          needGeometry(false),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4)
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

        composer.setWrapping(wraps);

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
        composer.setText(self.text());

        // Temporary background texture for development...
        bgTex = atlas().alloc(Image::solidColor(Image::Color(255, 255, 255, 255), Image::Size(1, 1)));

        drawable.addBuffer(ID_BUF_TEXT, new VertexBuf);
        drawable.addBufferWithNewProgram(ID_BUF_CURSOR, new VertexBuf, "cursor");

        self.root().shaders().build(drawable.program(), "generic.textured.color")
                << uMvpMatrix
                << self.root().uAtlas();

        self.root().shaders().build(drawable.program("cursor"), "generic.color_ucolor")
                << uMvpMatrix
                << uColor;

        updateProjection();
    }

    void glDeinit()
    {
        atlas().release(bgTex);
        composer.release();
    }

    void updateGeometry()
    {
        if(composer.update())
        {
            needGeometry = true;
        }

        Rectanglei pos;
        if(!self.checkPlace(pos) && !needGeometry)
        {
            return;
        }
        needGeometry = false;

        Vector4f bgColor = self.style().colors().colorf("background");

        // The background.
        VertexBuf::Builder verts;
        verts.makeQuad(pos, bgColor, atlas().imageRectf(bgTex).middle());

        // Text lines.
        Rectanglei const contentRect = pos.shrunk(margin);
        composer.makeVertices(verts, contentRect, AlignLeft, AlignLeft);

        // Underline the possible suggested completion.
        if(self.isSuggestingCompletion())
        {
            Rangei const   comp     = self.completionRange();
            Vector2i const startPos = self.linePos(comp.start);
            Vector2i const endPos   = self.linePos(comp.end);

            Vector2i const offset = contentRect.topLeft + Vector2i(0, font->ascent().valuei() + 2);

            // It may span multiple lines.
            for(int i = startPos.y; i <= endPos.y; ++i)
            {
                Rangei const span = wraps.line(i).range;
                Vector2i start = wraps.charTopLeftInPixels(i, i == startPos.y? startPos.x : span.start) + offset;
                Vector2i end   = wraps.charTopLeftInPixels(i, i == endPos.y?   endPos.x   : span.end)   + offset;

                verts.makeQuad(Rectanglef(start, end + Vector2i(0, 1)),
                               Vector4f(1, 1, 1, 1),
                               atlas().imageRectf(bgTex).middle());
            }
        }

        drawable.buffer<VertexBuf>(ID_BUF_TEXT)
                .setVertices(gl::TriangleStrip, verts, gl::Static);

        // Cursor.
        Vector2i const cursorPos = self.lineCursorPos();
        Vector2f const cp = wraps.charTopLeftInPixels(cursorPos.y, cursorPos.x) +
                contentRect.topLeft;

        verts.clear();
        verts.makeQuad(Rectanglef(cp + Vector2f(-1, 0),
                                  cp + Vector2f(1, font->height().value())),
                       Vector4f(1, 1, 1, 1),
                       atlas().imageRectf(bgTex).middle());

        drawable.buffer<VertexBuf>(ID_BUF_CURSOR)
                .setVertices(gl::TriangleStrip, verts, gl::Static);
    }

    void contentChanged()
    {
        composer.setText(self.text());
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
    // Blink the cursor.
    Vector4f col = style().colors().colorf("editor.cursor");
    col.w *= (int(d->blinkTime.since() * 2) & 1? .25f : 1.f);
    if(!hasFocus())
    {
        col.w = 0;
    }
    d->uColor = col;

    d->updateGeometry();
    d->drawable.draw();
}

bool LineEditWidget::handleEvent(Event const &event)
{
    if(event.isKeyDown())
    {
        KeyEvent const &key = static_cast<KeyEvent const &>(event);

        // Control character.
        if(handleControlKey(key.qtKey(), key.modifiers().testFlag(KeyEvent::Control)))
        {
            return true;
        }

        // Insert text?
        if(!key.text().isEmpty() && key.text()[0].isPrint())
        {
            // Insert some text into the editor.
            insert(key.text());
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
}

void LineEditWidget::cursorMoved()
{
    d->needGeometry = true;
    d->blinkTime = Time();
}

void LineEditWidget::contentChanged()
{
    d->contentChanged();
}
