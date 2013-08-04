/** @file progresswidget.cpp   Progress indicator.
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

#include "ui/widgets/progresswidget.h"
#include "ui/widgets/guirootwidget.h"

#include <de/Animation>
#include <de/Lockable>

using namespace de;

DENG2_PIMPL(ProgressWidget), public Lockable
{
    Mode mode;
    Rangei range;
    Rangef visualRange;
    Animation pos;
    float angle;
    Id gearTex;
    DotPath colorId;
    DotPath shadowColorId;
    Time updateAt;
    int framesWhileAnimDone; ///< # of frames drawn while animation was already done.

    Instance(Public *i)
        : Base(i),
          mode(Indefinite),
          visualRange(0, 1),
          pos(0, Animation::Linear),
          angle(0),
          colorId("progress.light.wheel"),
          shadowColorId("progress.light.shadow"),
          updateAt(Time::invalidTime()),
          framesWhileAnimDone(0)
    {
        updateStyle();
    }

    void updateStyle()
    {
        self.setImageColor(self.style().colors().colorf(colorId) * Vector4f(1, 1, 1, .5f));
    }

    void glInit()
    {
        gearTex = atlas().alloc(self.style().images().image("progress.gear"));
    }

    void glDeinit()
    {
        atlas().release(gearTex);
        gearTex = Id::None;
    }

    AtlasTexture &atlas()
    {
        return self.root().atlas();
    }
};

ProgressWidget::ProgressWidget(String const &name) : d(new Instance(this))
{
    setTextGap("progress.textgap");
    setSizePolicy(ui::Filled, ui::Filled);

    // Set up the static progress ring image.
    setImage(style().images().image("progress.wheel"));
    setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
    setImageScale(.6f);

    setAlignment(ui::AlignCenter, AlignOnlyByImage);
    setTextAlignment(ui::AlignRight);
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

void ProgressWidget::setShadowColor(const DotPath &styleId)
{
    d->shadowColorId = styleId;
    d->updateStyle();
}

void ProgressWidget::setText(String const &text)
{
    DENG2_GUARD(d);
    LabelWidget::setText(text);
}

void ProgressWidget::setMode(ProgressWidget::Mode progressMode)
{
    DENG2_GUARD(d);
    d->mode = progressMode;
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
}

void ProgressWidget::update()
{
    DENG2_GUARD(d);

    LabelWidget::update();

    // Keep rotating the wheel.
    Time const now = Time();
    if(!d->updateAt.isValid()) d->updateAt = now;
    TimeDelta const elapsed = d->updateAt.since();
    d->updateAt = now;

    d->angle = de::wrap(d->angle + float(elapsed * 20), 0.f, 360.f);

    requestGeometry();
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

    ContentLayout layout;
    contentLayout(layout);

    // There is a shadow behind the wheel.
    float gradientThick = layout.image.width() * 1;
    float solidThick = layout.image.width() * .5f;

    Vector4f const shadowColor = style().colors().colorf(d->shadowColorId);
    verts.makeRing(layout.image.middle(),
                   solidThick, 0, 30,
                   shadowColor,
                   root().atlas().imageRectf(root().borderGlow()).middle());
    verts.makeRing(layout.image.middle(),
                   gradientThick, solidThick, 30,
                   shadowColor,
                   root().atlas().imageRectf(root().borderGlow()), 0);
    /*verts.makeRing(layout.image.middle(),
                   shadowRadius - gradientThick, shadowRadius - solidThick, 30,
                   shadowColor,
                   root().atlas().imageRectf(root().borderGlow()), 0);*/

    LabelWidget::glMakeGeometry(verts);

    if(d->pos.done())
    {
        // Has the animation finished now?
        ++d->framesWhileAnimDone;
    }

    // Draw the rotating indicator on the label's image.
    Rectanglef const tc = d->atlas().imageRectf(d->gearTex);
    float pos = 1;    
    if(d->mode != Indefinite)
    {
        pos = de::clamp(0.f, d->pos.value(), 1.f);
    }    

    // Map to the visual range.
    pos = d->visualRange.start + pos * d->visualRange.size();

    int const edgeCount = de::max(1, int(pos * 30));
    float const radius = layout.image.width() / 2;

    Matrix4f const rotation = Matrix4f::rotateAround(tc.middle(), -d->angle);

    DefaultVertexBuf::Builder gear;
    DefaultVertexBuf::Type v;
    v.rgba = style().colors().colorf(d->colorId);

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

void ProgressWidget::updateStyle()
{
    d->updateStyle();
}
