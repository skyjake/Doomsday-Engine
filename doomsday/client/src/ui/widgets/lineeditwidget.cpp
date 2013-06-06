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
#include "ui/widgets/labelwidget.h"
#include "ui/widgets/fontlinewrapping.h"
#include "ui/widgets/guirootwidget.h"
#include "ui/widgets/gltextcomposer.h"
#include "ui/style.h"

#include <de/KeyEvent>
#include <de/MouseEvent>
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
    LabelWidget *hint;

    // Style.
    Font const *font;
    int margin;
    Time blinkTime;
    Animation hovering;

    // GL objects.
    GLTextComposer composer;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    GLUniform uCursorColor;

    Instance(Public *i)
        : Base(i),
          wraps(static_cast<FontLineWrapping &>(i->lineWraps())),
          hint(0),
          font(0),
          margin(0),
          hovering(0, Animation::Linear),
          uMvpMatrix  ("uMvpMatrix", GLUniform::Mat4),
          uColor      ("uColor",     GLUniform::Vec4),
          uCursorColor("uColor",     GLUniform::Vec4)
    {
        height = new ScalarRule(0);

        self.setFont("editor.plaintext");
        updateStyle();

        uCursorColor = Vector4f(1, 1, 1, 1);
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

        font   = &self.font();
        margin = st.rules().rule("gap").valuei();

        updateBackground();

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

    void updateBackground()
    {
        Background bg(self.style().colors().colorf("background"));
        if(hovering > 0)
        {
            bg.type = Background::GradientFrame;
            bg.thickness = 6;
            bg.color = Vector4f(1, 1, 1, .15f * hovering);
            self.requestGeometry();
        }
        self.set(bg);
    }

    AtlasTexture &atlas()
    {
        return self.root().atlas();
    }

    void glInit()
    {
        composer.setAtlas(atlas());
        composer.setText(self.text());

        drawable.addBuffer(ID_BUF_TEXT, new VertexBuf);
        drawable.addBufferWithNewProgram(ID_BUF_CURSOR, new VertexBuf, "cursor");

        self.root().shaders().build(drawable.program(), "generic.textured.color_ucolor")
                << uMvpMatrix
                << uColor
                << self.root().uAtlas();

        self.root().shaders().build(drawable.program("cursor"), "generic.color_ucolor")
                << uMvpMatrix
                << uCursorColor;

        updateProjection();
    }

    void glDeinit()
    {
        composer.release();
    }

    bool showingHint() const
    {
        return self.text().isEmpty() && !hint->text().isEmpty() && !self.hasFocus();
    }

    void updateGeometry()
    {
        updateBackground();

        if(composer.update())     self.requestGeometry();

        // Do we actually need to update geometry?
        Rectanglei pos;
        if(!self.hasChangedPlace(pos) && !self.geometryRequested())
        {
            return;
        }

        // Generate all geometry.
        self.requestGeometry(false);

        VertexBuf::Builder verts;
        self.glMakeGeometry(verts);
        drawable.buffer<VertexBuf>(ID_BUF_TEXT)
                .setVertices(gl::TriangleStrip, verts, gl::Static);

        // Cursor.
        Vector2i const cursorPos = self.lineCursorPos();
        Vector2f const cp = wraps.charTopLeftInPixels(cursorPos.y, cursorPos.x) +
                contentRect().topLeft;

        verts.clear();
        verts.makeQuad(Rectanglef(cp + Vector2f(-1, 0),
                                  cp + Vector2f(1, font->height().value())),
                       Vector4f(1, 1, 1, 1),
                       atlas().imageRectf(self.root().solidWhitePixel()).middle());

        drawable.buffer<VertexBuf>(ID_BUF_CURSOR)
                .setVertices(gl::TriangleStrip, verts, gl::Static);
    }

    void updateHover(Vector2i const &pos)
    {
        if(!self.hasFocus() && self.hitTest(pos))
        {
            if(hovering.target() < 1)
            {
                hovering.setValue(1, .15f);
            }
        }
        else if(hovering.target() > 0)
        {
            hovering.setValue(0, .6f);
        }
    }

    void contentChanged()
    {
        composer.setText(self.text());
    }

    void atlasContentRepositioned(Atlas &)
    {
        self.requestGeometry();
    }

    inline Rectanglei contentRect() const
    {
        return self.rule().recti().shrunk(margin);
    }
};

LineEditWidget::LineEditWidget(String const &name)
    : GuiWidget(name),
      AbstractLineEditor(new FontLineWrapping),
      d(new Instance(this))
{
    setBehavior(ContentClipping);

    // The widget's height is tied to the number of lines.
    rule().setInput(Rule::Height, *d->height);
}

void LineEditWidget::setEmptyContentHint(String const &hintText)
{
    if(!d->hint)
    {
        // A child widget will show the hint text.
        d->hint = new LabelWidget;
        d->hint->setFont("editor.hint");
        d->hint->setTextColor("editor.hint");
        d->hint->setAlignment(AlignLeft);
        d->hint->setBehavior(Unhittable | ContentClipping);
        d->hint->rule().setRect(rule());
        d->hint->setOpacity(0);
        add(d->hint);
    }
    d->hint->setText(hintText);
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

void LineEditWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    GuiWidget::glMakeGeometry(verts);

    Rectanglei const contentRect = d->contentRect();
    Rectanglef const solidWhiteUv = d->atlas().imageRectf(root().solidWhitePixel());

    // Text lines.
    d->composer.makeVertices(verts, contentRect, AlignLeft, AlignLeft, textColorf());

    // Underline the possible suggested completion.
    if(isSuggestingCompletion())
    {
        Rangei const   comp     = completionRange();
        Vector2i const startPos = linePos(comp.start);
        Vector2i const endPos   = linePos(comp.end);

        Vector2i const offset = contentRect.topLeft + Vector2i(0, d->font->ascent().valuei() + 2);

        // It may span multiple lines.
        for(int i = startPos.y; i <= endPos.y; ++i)
        {
            Rangei const span = d->wraps.line(i).range;
            Vector2i start = d->wraps.charTopLeftInPixels(i, i == startPos.y? startPos.x : span.start) + offset;
            Vector2i end   = d->wraps.charTopLeftInPixels(i, i == endPos.y?   endPos.x   : span.end)   + offset;

            verts.makeQuad(Rectanglef(start, end + Vector2i(0, 1)),
                           Vector4f(1, 1, 1, 1), solidWhiteUv.middle());
        }
    }
}

void LineEditWidget::updateStyle()
{
    d->updateStyle();
}

void LineEditWidget::viewResized()
{
    updateLineWraps(RewrapNow);
    d->updateProjection();
}

void LineEditWidget::focusGained()
{
    d->contentChanged();
}

void LineEditWidget::focusLost()
{
    d->contentChanged();
}

void LineEditWidget::update()
{
    GuiWidget::update();
    d->updateBackground();

    // Rewrap content if necessary.
    updateLineWraps(WrapUnlessWrappedAlready);

    if(d->hint)
    {
        d->hint->setOpacity(d->showingHint()? 1 : 0);
    }
}

void LineEditWidget::draw()
{
    float const opac = visibleOpacity();

    // Blink the cursor.
    Vector4f col = style().colors().colorf("editor.cursor");
    col.w *= (int(d->blinkTime.since() * 2) & 1? .25f : 1.f) * opac;
    if(!hasFocus())
    {
        col.w = 0;
    }
    d->uCursorColor = col;

    // Overall opacity.
    d->uColor = Vector4f(1, 1, 1, opac);

    d->updateGeometry();
    d->drawable.draw();
}

bool LineEditWidget::handleEvent(Event const &event)
{
    if(event.type() == Event::MousePosition)
    {
        d->updateHover(event.as<MouseEvent>().pos());
    }

    switch(handleMouseClick(event))
    {
    case MouseClickStarted:
        return true;

    case MouseClickFinished:
        root().setFocus(this);
        return true;

    default:
        break;
    }

    if(hasFocus() && event.isKeyDown())
    {
        KeyEvent const &key = event.as<KeyEvent>();

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
    requestGeometry();
    d->blinkTime = Time();
}

void LineEditWidget::contentChanged()
{
    d->contentChanged();
}
