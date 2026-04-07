/** @file widgets/lineeditwidget.cpp
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/lineeditwidget.h"
#include "de/labelwidget.h"
#include "de/fontlinewrapping.h"
#include "de/guirootwidget.h"
#include "de/gltextcomposer.h"
#include "de/ui/style.h"

#include <de/keyevent.h>
#include <de/mouseevent.h>
#include <de/animationrule.h>
#include <de/drawable.h>

#if defined (DE_MOBILE)
#  include <QGuiApplication>
#  include <QInputMethod>
#endif

namespace de {

using namespace ui;

static constexpr TimeSpan HEIGHT_ANIM_SPAN = 500_ms;
static constexpr duint    ID_BUF_TEXT      = 1;
static constexpr duint    ID_BUF_CURSOR    = 2;

DE_GUI_PIMPL(LineEditWidget)
{
    typedef GLBufferT<GuiVertex> VertexBuf;

    AnimationRule *height;
    FontLineWrapping &wraps;
    LabelWidget *hint;
    bool signalOnEnter;
    bool firstUpdateAfterCreation;

    // Style.
    ColorTheme colorTheme = Normal;
    Vec4f textColor;
    const Font *font;
    Time blinkTime;
    Animation hovering;
    float unfocusedBackgroundOpacity = 0.f;

    // GL objects.
    GLTextComposer composer;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    GLUniform uCursorColor;

    Impl(Public *i)
        : Base(i)
        , wraps(static_cast<FontLineWrapping &>(i->lineWraps()))
        , hint(nullptr)
        , signalOnEnter(false)
        , firstUpdateAfterCreation(true)
        , font(nullptr)
        , hovering(0, Animation::Linear)
        , uMvpMatrix  ("uMvpMatrix", GLUniform::Mat4)
        , uColor      ("uColor",     GLUniform::Vec4)
        , uCursorColor("uColor",     GLUniform::Vec4)
    {
        height = new AnimationRule(0);

        self().set(Background(Vec4f(1), Background::GradientFrame));
        self().setFont("editor.plaintext");
        updateStyle();
    }

    ~Impl()
    {
        releaseRef(height);
    }

    /**
     * Update the style used by the widget from the current UI style.
     */
    void updateStyle()
    {
        font = &self().font();
        textColor    = style().colors().colorf(colorTheme == Normal? "text" : "inverted.text");
        uCursorColor = style().colors().colorf(colorTheme == Normal? "text" : "inverted.text");

        updateBackground();

        // Update the line wrapper's font.
        wraps.setFont(*font);
        wraps.clear();
        composer.setWrapping(wraps);
        composer.forceUpdate();

        contentChanged(false);
    }

    int calculateHeight()
    {
        const int hgt = de::max(font->height().valuei(), wraps.totalHeightInPixels());
        return hgt + self().margins().height().valuei();
    }

    void updateProjection()
    {
        uMvpMatrix = root().projMatrix2D();
    }

    void updateBackground()
    {
        // If using a gradient frame, update parameters automatically.
        if (self().background().type == Background::GradientFrame)
        {
            Background bg;
            const Vec3f frameColor = style().colors().colorf(colorTheme == Normal? "text" : "inverted.text");
            if (!self().hasFocus())
            {
                bg = Background(Background::GradientFrame, Vec4f(frameColor, .15f + hovering * .2f), 6);
                if (unfocusedBackgroundOpacity > 0.f)
                {
                    bg.solidFill = Vec4f(style().colors().colorf(colorTheme == Normal? "background"
                                                                                        : "inverted.background").xyz(),
                                            unfocusedBackgroundOpacity);
                }
            }
            else
            {
                bg = Background(style().colors().colorf(colorTheme == Normal? "background"
                                                                            : "inverted.background"),
                                Background::GradientFrame,
                                Vec4f(frameColor, .25f + hovering * .3f), 6);
            }
            self().set(bg);
        }
    }

    void glInit()
    {
        composer.setAtlas(atlas());
        composer.setText(self().text());

        drawable.addBuffer(ID_BUF_TEXT, new VertexBuf);
        drawable.addBufferWithNewProgram(ID_BUF_CURSOR, new VertexBuf, "cursor");

        shaders().build(drawable.program(), "generic.textured.color_ucolor")
                << uMvpMatrix
                << uColor
                << uAtlas();

        shaders().build(drawable.program("cursor"), "generic.color_ucolor")
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
        return self().text().isEmpty() && !hint->text().isEmpty() && !self().hasFocus();
    }

    void updateGeometry()
    {
        updateBackground();

        if (composer.update()) self().requestGeometry();

        // Do we actually need to update geometry?
        Rectanglei pos;
        if (!self().hasChangedPlace(pos) && !self().geometryRequested())
        {
            return;
        }

        // Generate all geometry.
        self().requestGeometry(false);

        VertexBuf::Builder verts;
        self().glMakeGeometry(verts);
        drawable.buffer<VertexBuf>(ID_BUF_TEXT)
                .setVertices(gfx::TriangleStrip, verts, gfx::Static);

        // Cursor.
        const Rectanglei caret = self().cursorRect();

        verts.clear();
        verts.makeQuad(caret, Vec4f(1),
                       atlas().imageRectf(self().root().solidWhitePixel()).middle());

        drawable.buffer<VertexBuf>(ID_BUF_CURSOR)
                .setVertices(gfx::TriangleStrip, verts, gfx::Static);
    }

    void updateHover(const Vec2i &pos)
    {
        if (/*!self().hasFocus() && */ self().hitTest(pos))
        {
            if (hovering.target() < 1)
            {
                hovering.setValue(1, .15f);
            }
        }
        else if (hovering.target() > 0)
        {
            hovering.setValue(0, .6f);
        }
    }

    void contentChanged(bool notify)
    {
        composer.setText(self().text());
        if (notify)
        {
            DE_NOTIFY_PUBLIC(ContentChange, i)
            {
                i->editorContentChanged(self());
            }
        }
    }

    void atlasContentRepositioned(Atlas &)
    {
        self().requestGeometry();
    }

    DE_PIMPL_AUDIENCES(Enter, ContentChange)
};

DE_AUDIENCE_METHODS(LineEditWidget, Enter, ContentChange)

LineEditWidget::LineEditWidget(const String &name)
    : GuiWidget(name),
      AbstractLineEditor(new FontLineWrapping),
      d(new Impl(this))
{
    setBehavior(ContentClipping | Focusable);
    setAttribute(FocusHidden);

    // The widget's height is tied to the number of lines.
    rule().setInput(Rule::Height, *d->height);
}

void LineEditWidget::setText(const String &lineText)
{
    AbstractLineEditor::setText(lineText);

    if (d->hint)
    {
        if (d->showingHint())
        {
            d->hint->setOpacity(1, .5);
        }
        else
        {
            d->hint->setOpacity(0);
        }
    }
}

void LineEditWidget::setEmptyContentHint(const String &hintText,
                                         const String &hintFont)
{
    if (!d->hint)
    {
        // A child widget will show the hint text.
        d->hint = new LabelWidget;
        d->hint->setTextColor("editor.hint");
        d->hint->setAlignment(ui::AlignLeft);
        d->hint->setBehavior(Unhittable | ContentClipping);
        d->hint->rule().setRect(rule());
        d->hint->setOpacity(1);
        add(d->hint);
    }
    d->hint->setFont(hintFont.isEmpty()? String("editor.hint.default") : hintFont);
    d->hint->setText(hintText);
}

void LineEditWidget::setSignalOnEnter(bool enterSignal)
{
    d->signalOnEnter = enterSignal;
}

Rectanglei LineEditWidget::cursorRect() const
{
    const auto cursorPos = lineCursorPos();
    const Vec2i cp = d->wraps.charTopLeftInPixels(cursorPos.line, cursorPos.x) +
            contentRect().topLeft;

    return Rectanglei(cp + pointsToPixels(Vec2i(-1, 0)),
                      cp + Vec2i(pointsToPixels(1), d->font->height().valuei()));
}

void LineEditWidget::setColorTheme(ColorTheme theme)
{
    d->colorTheme = theme;
    d->updateStyle();
}

void LineEditWidget::setUnfocusedBackgroundOpacity(float opacity)
{
    d->unfocusedBackgroundOpacity = opacity;

    if (!hasFocus())
    {
        d->updateBackground();
    }
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

void LineEditWidget::glMakeGeometry(GuiVertexBuilder &verts)
{
    GuiWidget::glMakeGeometry(verts);

    const Rectanglei contentRect = this->contentRect();
    const Rectanglef solidWhiteUv = d->atlas().imageRectf(root().solidWhitePixel());

    // Text lines.
    d->composer.makeVertices(verts, contentRect, AlignLeft, AlignLeft, d->textColor);

    // Underline the possible suggested completion.
    if (isSuggestingCompletion())
    {
        const auto comp     = completionRange();
        const auto startPos = linePos(comp.start);
        const auto endPos   = linePos(comp.end);

        const Vec2i offset =
            contentRect.topLeft + Vec2i(0, d->font->ascent().valuei() + pointsToPixels(2));

        // It may span multiple lines.
        for (auto i = startPos.line; i <= endPos.line; ++i)
        {
            const auto &span = d->wraps.line(i).range;
            Vec2i start = d->wraps.charTopLeftInPixels(i, i == startPos.line? startPos.x : BytePos(0)) + offset;
            Vec2i end   = d->wraps.charTopLeftInPixels(i, i == endPos.line?   endPos.x   : BytePos(span.size())) + offset;

            verts.makeQuad(Rectanglef(start, end + pointsToPixels(Vec2i(0, 1))),
                           Vec4f(1), solidWhiteUv.middle());
        }
    }
}

void LineEditWidget::updateStyle()
{
    d->updateStyle();
}

void LineEditWidget::viewResized()
{
    GuiWidget::viewResized();

    updateLineWraps(RewrapNow);
    d->updateProjection();
}

void LineEditWidget::focusGained()
{
    d->contentChanged(false /* don't notify */);

    if (d->hint)
    {
        d->hint->setOpacity(0);
    }

    root().window().eventHandler().setKeyboardMode(WindowEventHandler::TextInput);
    
/*#if defined (DE_MOBILE)
    {
        auto &win = root().window();
        emit win.textEntryRequest();

        // Text entry happens via OS virtual keyboard.
        connect(&win, &GLWindow::userEnteredText, this, &LineEditWidget::userEnteredText);
        connect(&win, &GLWindow::userFinishedTextEntry, this, &LineEditWidget::userFinishedTextEntry);
    }
#endif*/
}

void LineEditWidget::focusLost()
{
    root().window().eventHandler().setKeyboardMode(WindowEventHandler::RawKeys);

/*#if defined (DE_MOBILE)
    {
        auto &win = root().window();
        disconnect(&win, &GLWindow::userEnteredText, this, &LineEditWidget::userEnteredText);
        disconnect(&win, &GLWindow::userFinishedTextEntry, this, &LineEditWidget::userFinishedTextEntry);
        emit win.textEntryDismiss();
    }
#endif*/

    d->contentChanged(false /* don't notify */);

    if (d->hint && d->showingHint())
    {
        d->hint->setOpacity(1, 1.0, 0.5);
    }
}

/*#if defined (DE_MOBILE)
void LineEditWidget::userEnteredText(QString text)
{
    setText(text);
}

void LineEditWidget::userFinishedTextEntry()
{
    root().popFocus();
}
#endif*/

void LineEditWidget::update()
{
    GuiWidget::update();

    d->updateBackground();

    // Rewrap content if necessary.
    updateLineWraps(WrapUnlessWrappedAlready);

    if (d->firstUpdateAfterCreation)
    {
        // Don't animate height immediately after creation.
        d->firstUpdateAfterCreation = false;
        d->height->finish();
    }
}

void LineEditWidget::drawContent()
{
    auto &painter = root().painter();
    painter.flush();

    GLState::push().setNormalizedScissor(painter.normalizedScissor());

    const float opac = visibleOpacity();
    d->uColor = Vec4f(1, 1, 1, opac); // Overall opacity.

    // Blink the cursor.
    Vec4f col = style().colors().colorf("editor.cursor");
    col.w *= (int(d->blinkTime.since() * 2) & 1? .25f : 1.f) * opac;
    if (!hasFocus())
    {
        col.w = 0;
    }
    d->uCursorColor = col;

    d->updateGeometry();
    d->drawable.draw();

    GLState::pop();
}

bool LineEditWidget::handleEvent(const Event &event)
{
    if (isDisabled()) return false;

    if (event.type() == Event::MousePosition)
    {
        d->updateHover(event.as<MouseEvent>().pos());
    }

    // Only handle clicks when not already focused.
    if (!hasFocus())
    {
        switch (handleMouseClick(event))
        {
        case MouseClickStarted:
            return true;

        case MouseClickFinished:
            root().setFocus(this);
            return true;

        default:
            break;
        }
    }

    if (is<KeyEvent>(event) && event.as<KeyEvent>().ddKey() == DDKEY_ENTER)
    {
        debug("LineEditWidget: Enter key %i %s", event.type(), DE_BOOL_YESNO(hasFocus()));
    }

    // Only handle keys when focused.
    if (hasFocus() && event.isKeyDown())
    {
        const KeyEvent &key = event.as<KeyEvent>();

        if (key.isModifier())
        {
            // Don't eat modifier keys; the bindings system needs them.
            return false;
        }
        /*
        if (key.qtKey() == Qt::Key_Shift)
        {
            // Shift is not eaten so that Shift-Tilde can produce ~.
            // If we ate Shift, the bindings system would not realize it is down.
            return false;
        }

        if (key.qtKey() == Qt::Key_Control || key.qtKey() == Qt::Key_Alt ||
           key.qtKey() == Qt::Key_Meta)
        {
            // Modifier keys alone will be eaten when focused.
            return false;
        }*/

        if (d->signalOnEnter && (key.ddKey() == DDKEY_ENTER || key.ddKey() == DDKEY_RETURN))
        {
            DE_NOTIFY(Enter, i)
            {
                i->enterPressed(text());
            }
            return true;
        }

        // Control keys.
        const auto controlKey = termKey(key);
        if (controlKey != term::Key::None)
        {
            if (handleControlKey(controlKey, modifiersFromKeyEvent(key.modifiers())))
            {
                return true;
            }
            return GuiWidget::handleEvent(event);
        }

        // Other command keys are probably app shortcuts, so leave those alone.
        if (key.modifiers() & KeyEvent::Command)
        {
            return GuiWidget::handleEvent(event);
        }

        // Insert text?
        if (key.text())
        {
            // Insert some text into the editor.
            insert(key.text());
        }

        // We have focus, so all other key presses stop here.
        return true;
    }

    return GuiWidget::handleEvent(event);
}

term::Key LineEditWidget::termKey(const KeyEvent &keyEvent) // static
{
    using term::Key;

#if defined (MACOSX)
    if (keyEvent.modifiers() == KeyEvent::Meta)
    {
        switch (keyEvent.ddKey())
        {
            case DDKEY_LEFTARROW: return Key::Home;
            case DDKEY_RIGHTARROW: return Key::End;
        }
    }
#endif

    if (keyEvent.modifiers() == KeyEvent::Control)
    {
        switch (keyEvent.ddKey())
        {
        case 'c': return Key::Break;
        case 'a': return Key::Home;
        case 'e': return Key::End;
        case 'k': return Key::Kill;
        case 'x': return Key::Cancel;
        case 'z': return Key::Substitute;
        }
    }
    else if (keyEvent.modifiers() == KeyEvent::Shift)
    {
        switch (keyEvent.ddKey())
        {
        case DDKEY_TAB: return Key::Backtab;
        }
    }
    switch (keyEvent.ddKey())
    {
    case DDKEY_ESCAPE: return Key::Escape;
    case DDKEY_UPARROW: return Key::Up;
    case DDKEY_DOWNARROW: return Key::Down;
    case DDKEY_LEFTARROW: return Key::Left;
    case DDKEY_RIGHTARROW: return Key::Right;
    case DDKEY_HOME: return Key::Home;
    case DDKEY_END: return Key::End;
    case DDKEY_PGUP: return Key::PageUp;
    case DDKEY_PGDN: return Key::PageDown;
    case DDKEY_INS: return Key::Insert;
    case DDKEY_DEL: return Key::Delete;
    case DDKEY_ENTER: return Key::Enter;
    case DDKEY_BACKSPACE: return Key::Backspace;
    case DDKEY_TAB: return Key::Tab;
    case DDKEY_F1: return Key::F1;
    case DDKEY_F2: return Key::F2;
    case DDKEY_F3: return Key::F3;
    case DDKEY_F4: return Key::F4;
    case DDKEY_F5: return Key::F5;
    case DDKEY_F6: return Key::F6;
    case DDKEY_F7: return Key::F7;
    case DDKEY_F8: return Key::F8;
    case DDKEY_F9: return Key::F9;
    case DDKEY_F10: return Key::F10;
    case DDKEY_F11: return Key::F11;
    case DDKEY_F12: return Key::F12;

    default: return Key::None;
    }
}

AbstractLineEditor::KeyModifiers LineEditWidget::modifiersFromKeyEvent(const KeyEvent::Modifiers &keyMods)
{
    KeyModifiers mods;

    if (keyMods.testFlag(KeyEvent::Shift))   mods |= Shift;
    if (keyMods.testFlag(KeyEvent::Control)) mods |= Control;
    if (keyMods.testFlag(KeyEvent::Alt))     mods |= Alt;
    if (keyMods.testFlag(KeyEvent::Meta))    mods |= Meta;

    return mods;
}

int LineEditWidget::maximumWidth() const
{
    return rule().recti().width() - margins().width().valuei();
}

void LineEditWidget::numberOfLinesChanged(int /*lineCount*/)
{
    // Changes in the widget's height are animated.
    d->height->set(d->calculateHeight(), HEIGHT_ANIM_SPAN);
}

void LineEditWidget::cursorMoved()
{
    requestGeometry();
    d->blinkTime = Time();
}

void LineEditWidget::contentChanged()
{
    d->contentChanged(true);

    if (hasRoot())
    {
        updateLineWraps(WrapUnlessWrappedAlready);
    }
}

void LineEditWidget::autoCompletionEnded(bool)
{
    // Make sure the underlining is removed.
    requestGeometry();
}

} // namespace de
