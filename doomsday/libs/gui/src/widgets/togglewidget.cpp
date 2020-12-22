/** @file togglewidget.cpp  Toggle widget.
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

#include "de/togglewidget.h"
#include "de/proceduralimage.h"
#include "de/guirootwidget.h"
#include "de/ui/style.h"

#include <de/animation.h>

namespace de {

static constexpr TimeSpan SWITCH_ANIM_SPAN = 300_ms;

DE_PIMPL(ToggleWidget)
, DE_OBSERVES(ButtonWidget, Press)
{
    /// Draws the animated I/O toggle indicator.
    class ToggleProceduralImage : public ProceduralImage
    {
    public:
        ToggleProceduralImage(GuiWidget &owner)
            : _owner(owner)
            , _pos(0, Animation::EaseBoth)
            , _animating(false)
        {
            const Image &img = style().images().image(DE_STR("widget.toggle.onoff"));
            setPointSize(img.size() * img.pointRatio());
            updateStyle();
        }

        const Style &style() const { return _owner.style(); }
        Atlas &      atlas() const { return _owner.root().atlas(); }

        void setState(ToggleState st, bool animate)
        {
            _pos.setValue(st == Active ? 1 : 0, animate ? SWITCH_ANIM_SPAN : 0.0_s);
            _animating = true;
        }

        void finishAnimation()
        {
            _pos.finish();
        }

        bool update() override
        {
            if (_animating)
            {
                if (_pos.done()) _animating = false;
                return true;
            }
            return false;
        }

        void glMakeGeometry(GuiVertexBuilder &verts, const Rectanglef &rect) override
        {
            float p = _pos;

            // Clamp the position to non-fractional coordinates.
            Rectanglei const recti(rect.topLeft.toVec2i(), rect.bottomRight.toVec2i());

            // Background.
            float c = (.3f + .33f * p);
            verts.makeQuad(recti, (_accentColor * p + _textColor * (1-p)) * Vec4f(c, c, c, 1),
                           atlas().imageRectf(_owner.root().solidWhitePixel()).middle());

            Id onOff = _owner.root().styleTexture(DE_STR("widget.toggle.onoff"));

            // The on/off graphic.
            verts.makeQuad(recti, _accentColor * p + _textColor * (1-p) * .8f, atlas().imageRectf(onOff));

            // The flipper.
            const int  flipWidth = pointSize().x - pointSize().y + 2;
            Rectanglei flip      = Rectanglei::fromSize(
                recti.topLeft + pointsToPixels(Vec2i(
                                    1 + de::round<int>(p * (pointSize().x - flipWidth)), 1)),
                pointsToPixels(Vec2ui(flipWidth, pointSize().y) - Vec2ui(2, 2)));
            verts.makeQuad(flip, _bgColor * Vec4f(1, 1, 1, 3),
                           atlas().imageRectf(_owner.root().solidWhitePixel()).middle());
        }

        void updateStyle()
        {
            _bgColor     = style().colors().colorf("background").min(Vec4f(0, 0, 0, 1));
            _accentColor = style().colors().colorf("accent");
            _textColor   = style().colors().colorf("text");
        }

    private:
        GuiWidget &_owner;
        Animation _pos;
        bool _animating;
        ColorBank::Colorf _bgColor;
        ColorBank::Colorf _accentColor;
        ColorBank::Colorf _textColor;
    };

    ToggleState state;
    ToggleProceduralImage *procImage; // not owned
    bool hasBeenUpdated = false;

    Impl(Public *i, const Flags &flags)
        : Base(i)
        , state(Inactive)
        , procImage(!(flags & WithoutIndicator)? new ToggleProceduralImage(*i) : nullptr)
    {
        if (procImage) self().setImage(procImage); // base class owns it
        self().audienceForPress() += this;
    }

    void buttonPressed(ButtonWidget &)
    {
        // Toggle the state.
        self().setActive(self().isInactive());

        DE_NOTIFY_PUBLIC(UserToggle, i)
        {
            i->toggleStateChangedByUser(self().toggleState());
        }
    }

    DE_PIMPL_AUDIENCES(Toggle, UserToggle)
};

DE_AUDIENCE_METHODS(ToggleWidget, Toggle, UserToggle)

ToggleWidget::ToggleWidget(const Flags &flags, const String &name)
    : ButtonWidget(name)
    , d(new Impl(this, flags))
{
    setTextAlignment(ui::AlignRight);
    setTextLineAlignment(ui::AlignLeft);
}

void ToggleWidget::setToggleState(ToggleState state, bool notify)
{
    if (d->state != state)
    {
        d->state = state;
        if (d->procImage)
        {
            d->procImage->setState(state, hasBeenUpdated());
        }
        if (notify)
        {
            DE_NOTIFY(Toggle, i) { i->toggleStateChanged(*this); }
        }
    }
}

ToggleWidget::ToggleState ToggleWidget::toggleState() const
{
    return d->state;
}

void ToggleWidget::finishAnimation()
{
    if (d->procImage)
    {
        d->procImage->finishAnimation();
    }
}

} // namespace de
