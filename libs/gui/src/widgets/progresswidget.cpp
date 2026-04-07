/** @file progresswidget.cpp   Progress indicator.
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

#include "de/progresswidget.h"
#include "de/guirootwidget.h"

#include <de/animation.h>
#include <de/lockable.h>

namespace de {

DE_GUI_PIMPL(ProgressWidget), public Lockable
{
    Mode mode             = Indefinite;
    Rangei range;
    Rangef visualRange    { 0, 1 };
    Animation pos         { 0, Animation::Linear };
    float angle           = 0;
    float rotationSpeed   = 20;
    bool posChanging      = false;
    bool mini             = false;
    Id gearTex;
    DotPath colorId       { "progress.light.wheel" };
    DotPath shadowColorId { "progress.light.shadow" };
    DotPath gearId        { DE_STR("widget.progress.gear") };
    Time updateAt         { Time::invalidTime() };

    int framesWhileAnimDone = 0; ///< # of frames drawn while animation was already done.

    Impl(Public *i) : Base(i)
    {
        updateStyle();
    }

    void updateStyle()
    {
        if (mini)
        {
            self().setImageColor(Vec4f());
        }
        else
        {
            self().setImageColor(style().colors().colorf(colorId));
        }
    }

    void useDotStyle()
    {
        mini = true;
        self().setImage(nullptr);
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

    void makeRingGeometry(GuiVertexBuilder &verts)
    {
        ContentLayout layout;
        self().contentLayout(layout);

        // There is a shadow behind the wheel.
        float gradientThick = layout.image.width() * 2.f;
        float solidThick    = layout.image.width() * .53f;

        const Vec4f shadowColor = style().colors().colorf(shadowColorId);
        verts.makeRing(layout.image.middle(),
                       solidThick, 0, 30,
                       shadowColor,
                       root().atlas().imageRectf(root().borderGlow()).middle());
        verts.makeRing(layout.image.middle(),
                       gradientThick, solidThick, 30,
                       shadowColor,
                       root().atlas().imageRectf(root().borderGlow()), 0);

        self().LabelWidget::glMakeGeometry(verts);

        if (pos.done())
        {
            // Has the animation finished now?
            ++framesWhileAnimDone;
        }

        // Draw the rotating indicator on the label's image.
        const Rectanglef tc = atlas().imageRectf(gearTex);
        float pos = 1;
        if (mode != Indefinite)
        {
            pos = de::clamp(0.f, this->pos.value(), 1.f);
        }

        // Map to the visual range.
        pos = visualRange.start + pos * visualRange.size();

        const int edgeCount = de::max(1, int(pos * 30));
        const float radius = layout.image.width() / 2;

        const Mat4f rotation = Mat4f::rotateAround(tc.middle(), -angle);

        GuiVertexBuilder gear;
        GuiVertex v;
        v.rgba = style().colors().colorf(colorId) * Vec4f(1, 1, 1, mini? 1.f : 1.9f);

        for (int i = 0; i <= edgeCount; ++i)
        {
            // Center vertex.
            v.pos = layout.image.middle();
            v.texCoord = tc.middle();
            gear << v;

            // Outer vertex.
            const float angle = 2 * PI * pos * (i / (float)edgeCount) + PI/2;
            v.pos = v.pos + Vec2f(cos(angle)*radius*1.05f, sin(angle)*radius*1.05f);
            v.texCoord = rotation * (tc.topLeft + tc.size() * Vec2f(.5f + cos(angle)*.5f,
                                                                    .5f + sin(angle)*.5f));
            gear << v;
        }

        verts += gear;
    }

    void makeDotsGeometry(GuiVertexBuilder &verts)
    {
        const Image::Size dotSize = atlas().imageRect(root().tinyDot()).size();

        Rectanglei rect = self().contentRect().shrunk(dotSize.x / 2);
        const int midY  = rect.middle().y;
        int count       = range.size();
        Vec4f color  = style().colors().colorf(colorId);
        const int gap   = rule(RuleBank::UNIT).valuei();
        int totalWidth  = count * dotSize.x + (count - 1) * gap;

        for (int i = 0; i < count; ++i)
        {
            // Current progress determines the color of the dot.
            Vec4f dotColor = (float(i)/float(count) <= pos.value()? color : Vec4f(color, .166f));

            float midX = rect.middle().x - totalWidth/2 + i * (dotSize.x + gap);

            verts.makeQuad(Rectanglef::fromSize(Vec2f(midX, midY) - dotSize/2, dotSize),
                           dotColor, atlas().imageRectf(root().tinyDot()));
        }
    }
};

ProgressWidget::ProgressWidget(const String &name)
    : LabelWidget(name)
    , d(new Impl(this))
{
    setTextGap("progress.textgap");
    setSizePolicy(ui::Filled, ui::Filled);

    // Set up the static progress ring image.
    setStyleImage(DE_STR("widget.progress.wheel"));
    setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
    setImageScale(.6f);

    setTextAlignment(ui::AlignRight);
    setTextLineAlignment(ui::AlignLeft);
    setTextShadow(RectangleShadow);
}

void ProgressWidget::useMiniStyle(const DotPath &colorId)
{
    d->mini = true;
    d->colorId = colorId;
    d->gearId = DE_STR("widget.progress.mini");
    setTextColor(colorId);
    setRotationSpeed(80);
    setImageScale(1);

    // Resize to the height of the default font.
    setOverrideImageSize(style().fonts().font("default").height());

    d->updateStyle();
}

void ProgressWidget::setRotationSpeed(float anglesPerSecond)
{
    d->rotationSpeed = anglesPerSecond;
}

ProgressWidget::Mode ProgressWidget::mode() const
{
    DE_GUARD(d);
    return d->mode;
}

Rangei ProgressWidget::range() const
{
    DE_GUARD(d);
    return d->range;
}

bool ProgressWidget::isAnimating() const
{
    DE_GUARD(d);
    return d->framesWhileAnimDone < 2;
}

void ProgressWidget::setColor(const DotPath &styleId)
{
    d->colorId = styleId;
    d->updateStyle();
}

void ProgressWidget::setShadowColor(const DotPath &styleId)
{
    d->shadowColorId = styleId;
    d->updateStyle();
}

void ProgressWidget::setText(const String &text)
{
    DE_GUARD(d);
    LabelWidget::setText(text);
}

void ProgressWidget::setMode(Mode progressMode)
{
    DE_GUARD(d);
    d->mode = progressMode;
    if (d->mode == Dots)
    {
        d->useDotStyle();
    }
}

void ProgressWidget::setRange(const Rangei &range, const Rangef &visualRange)
{
    DE_GUARD(d);
    d->range = range;
    d->visualRange = visualRange;
    setMode(Ranged);
}

void ProgressWidget::setProgress(int currentProgress, TimeSpan transitionSpan)
{
    DE_GUARD(d);

    d->framesWhileAnimDone = 0;
    d->pos.setValue(float(currentProgress - d->range.start) / float(d->range.size()),
                    transitionSpan);
    d->posChanging = true;
}

void ProgressWidget::update()
{
    DE_GUARD(d);

    LabelWidget::update();

    if (d->mode != Dots)
    {
        // Keep rotating the wheel.
        const Time now = Time();
        if (!d->updateAt.isValid()) d->updateAt = now;
        const TimeSpan elapsed = d->updateAt.since();
        d->updateAt = now;

        d->angle = de::wrap(d->angle + float(elapsed * d->rotationSpeed), 0.f, 360.f);

        if (isVisible())
        {
            requestGeometry();
        }
    }
    else // Dots
    {
        if (isVisible() && d->posChanging)
        {
            requestGeometry();
        }
    }

    // Has the position stopped changing?
    if (d->posChanging && d->pos.done())
    {
        d->posChanging = false;
    }
}

void ProgressWidget::glInit()
{
    DE_GUARD(d);
    LabelWidget::glInit();
    d->glInit();
}

void ProgressWidget::glDeinit()
{
    DE_GUARD(d);
    d->glDeinit();
    LabelWidget::glDeinit();
}

void ProgressWidget::glMakeGeometry(GuiVertexBuilder &verts)
{
    DE_GUARD(d);

    switch (d->mode)
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
