/** @file progresswidget.cpp   Progress indicator.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ProgressWidget"
#include "de/GuiRootWidget"
#include "de/StyleProceduralImage"

#include <de/Animation>
#include <de/Lockable>

namespace de {

DENG_GUI_PIMPL(ProgressWidget), public Lockable
{
    Mode mode             = Indefinite;
    Rangei range;
    Rangef visualRange    { 0, 1 };
    Animation pos         { 0, Animation::Linear };
    bool posChanging      = false;
    float angle           = 0;
    float rotationSpeed   = 20;
    bool mini             = false;
    Id gearTex;
    DotPath colorId       { "progress.light.wheel" };
    DotPath shadowColorId { "progress.light.shadow" };
    DotPath gearId        { "progress.gear" };
    Time updateAt         { Time::invalidTime() };

    int framesWhileAnimDone = 0; ///< # of frames drawn while animation was already done.

    Instance(Public *i) : Base(i)
    {
        updateStyle();
    }

    void updateStyle()
    {
        if(mini)
        {
            self.setImageColor(Vector4f());
        }
        else
        {
            self.setImageColor(style().colors().colorf(colorId));
        }
    }

    void useDotStyle()
    {
        mini = true;
        self.setImage(nullptr);
        updateStyle();
    }

    void glInit()
    {
        gearTex = root().styleTexture(gearId);
    }

    void glDeinit()
    {
        gearTex = Id::None;
    }

    void makeRingGeometry(DefaultVertexBuf::Builder &verts)
    {
        ContentLayout layout;
        self.contentLayout(layout);

        // There is a shadow behind the wheel.
        float gradientThick = layout.image.width() * 2.f;
        float solidThick    = layout.image.width() * .53f;

        Vector4f const shadowColor = style().colors().colorf(shadowColorId);
        verts.makeRing(layout.image.middle(),
                       solidThick, 0, 30,
                       shadowColor,
                       root().atlas().imageRectf(root().borderGlow()).middle());
        verts.makeRing(layout.image.middle(),
                       gradientThick, solidThick, 30,
                       shadowColor,
                       root().atlas().imageRectf(root().borderGlow()), 0);

        // Shadow behind the text.
        Rectanglef textBox = Rectanglef::fromSize(self.textSize());
        ui::applyAlignment(ui::AlignCenter, textBox, layout.text);
        int const boxSize = textBox.height() * 6;
        Vector2f const off(0, textBox.height() * .16f);
        Vector2f const hoff(textBox.height(), 0);
        verts.makeFlexibleFrame(Rectanglef(textBox.midLeft() + hoff + off,
                                           textBox.midRight() - hoff + off)
                                    .expanded(boxSize),
                                boxSize, Vector4f(shadowColor, shadowColor.w * .75f),
                                root().atlas().imageRectf(root().borderGlow()));

        self.LabelWidget::glMakeGeometry(verts);

        if(pos.done())
        {
            // Has the animation finished now?
            ++framesWhileAnimDone;
        }

        // Draw the rotating indicator on the label's image.
        Rectanglef const tc = atlas().imageRectf(gearTex);
        float pos = 1;
        if(mode != Indefinite)
        {
            pos = de::clamp(0.f, this->pos.value(), 1.f);
        }

        // Map to the visual range.
        pos = visualRange.start + pos * visualRange.size();

        int const edgeCount = de::max(1, int(pos * 30));
        float const radius = layout.image.width() / 2;

        Matrix4f const rotation = Matrix4f::rotateAround(tc.middle(), -angle);

        DefaultVertexBuf::Builder gear;
        DefaultVertexBuf::Type v;
        v.rgba = style().colors().colorf(colorId) * Vector4f(1, 1, 1, mini? 1.f : 1.9f);

        for(int i = 0; i <= edgeCount; ++i)
        {
            // Center vertex.
            v.pos = layout.image.middle();
            v.texCoord = tc.middle();
            gear << v;

            // Outer vertex.
            float const angle = 2 * PI * pos * (i / (float)edgeCount) + PI/2;
            v.pos = v.pos + Vector2f(cos(angle)*radius*1.05f, sin(angle)*radius*1.05f);
            v.texCoord = rotation * (tc.topLeft + tc.size() * Vector2f(.5f + cos(angle)*.5f,
                                                                       .5f + sin(angle)*.5f));
            gear << v;
        }

        verts += gear;
    }

    void makeDotsGeometry(DefaultVertexBuf::Builder &verts)
    {
        Image::Size const dotSize = atlas().imageRect(root().tinyDot()).size();

        Rectanglei rect = self.contentRect().shrunk(dotSize.x / 2);
        int const midY  = rect.middle().y;
        int count       = range.size();
        Vector4f color  = style().colors().colorf(colorId);
        int const gap   = style().rules().rule("unit").valuei();
        int totalWidth  = count * dotSize.x + (count - 1) * gap;

        for(int i = 0; i < count; ++i)
        {
            // Current progress determines the color of the dot.
            Vector4f dotColor = (float(i)/float(count) <= pos.value()? color : Vector4f(color, .166f));

            float midX = rect.middle().x - totalWidth/2 + i * (dotSize.x + gap);

            verts.makeQuad(Rectanglef::fromSize(Vector2f(midX, midY) - dotSize/2, dotSize),
                           dotColor, atlas().imageRectf(root().tinyDot()));
        }
    }
};

ProgressWidget::ProgressWidget(String const &name)
    : LabelWidget(name)
    , d(new Instance(this))
{
    setTextGap("progress.textgap");
    setSizePolicy(ui::Filled, ui::Filled);

    // Set up the static progress ring image.
    setImage(new StyleProceduralImage("progress.wheel", *this));
    setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
    setImageScale(.6f);

    setTextAlignment(ui::AlignRight);
    setTextLineAlignment(ui::AlignLeft);
}

void ProgressWidget::useMiniStyle(DotPath const &colorId)
{
    d->mini = true;
    d->colorId = colorId;
    d->gearId = "progress.mini";
    setTextColor(colorId);
    setRotationSpeed(40);
    setImageScale(1);

    // Resize to the height of the default font.
    setOverrideImageSize(style().fonts().font("default").height().value());

    d->updateStyle();
}

void ProgressWidget::setRotationSpeed(float anglesPerSecond)
{
    d->rotationSpeed = anglesPerSecond;
}

ProgressWidget::Mode ProgressWidget::mode() const
{
    DENG2_GUARD(d);
    return d->mode;
}

Rangei ProgressWidget::range() const
{
    DENG2_GUARD(d);
    return d->range;
}

bool ProgressWidget::isAnimating() const
{
    DENG2_GUARD(d);
    return d->framesWhileAnimDone < 2;
}

void ProgressWidget::setColor(DotPath const &styleId)
{
    d->colorId = styleId;
    d->updateStyle();
}

void ProgressWidget::setShadowColor(DotPath const &styleId)
{
    d->shadowColorId = styleId;
    d->updateStyle();
}

void ProgressWidget::setText(String const &text)
{
    DENG2_GUARD(d);
    LabelWidget::setText(text);
}

void ProgressWidget::setMode(Mode progressMode)
{
    DENG2_GUARD(d);
    d->mode = progressMode;
    if(d->mode == Dots)
    {
        d->useDotStyle();
    }
}

void ProgressWidget::setRange(Rangei const &range, Rangef const &visualRange)
{
    DENG2_GUARD(d);
    d->range = range;
    d->visualRange = visualRange;
    setMode(Ranged);
}

void ProgressWidget::setProgress(int currentProgress, TimeDelta const &transitionSpan)
{
    DENG2_GUARD(d);

    d->framesWhileAnimDone = 0;
    d->pos.setValue(float(currentProgress - d->range.start) / float(d->range.size()),
                    transitionSpan);
    d->posChanging = true;
}

void ProgressWidget::update()
{
    DENG2_GUARD(d);

    LabelWidget::update();

    if(d->mode != Dots)
    {
        // Keep rotating the wheel.
        Time const now = Time();
        if(!d->updateAt.isValid()) d->updateAt = now;
        TimeDelta const elapsed = d->updateAt.since();
        d->updateAt = now;

        d->angle = de::wrap(d->angle + float(elapsed * d->rotationSpeed), 0.f, 360.f);

        if(isVisible())
        {
            requestGeometry();
        }
    }
    else // Dots
    {
        if(isVisible() && d->posChanging)
        {
            requestGeometry();
        }
    }

    // Has the position stopped changing?
    if(d->posChanging && d->pos.done())
    {
        d->posChanging = false;
    }
}

void ProgressWidget::glInit()
{
    DENG2_GUARD(d);
    LabelWidget::glInit();
    d->glInit();
}

void ProgressWidget::glDeinit()
{
    DENG2_GUARD(d);
    d->glDeinit();
    LabelWidget::glDeinit();
}

void ProgressWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    DENG2_GUARD(d);

    switch(d->mode)
    {
    case Ranged:
    case Indefinite:
        d->makeRingGeometry(verts);
        break;

    case Dots:
        d->makeDotsGeometry(verts);
        break;
    }
}

void ProgressWidget::updateStyle()
{
    d->updateStyle();
}

} // namespace de
