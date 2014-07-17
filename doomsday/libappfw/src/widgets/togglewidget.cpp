/** @file togglewidget.cpp  Toggle widget.
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

#include "de/ToggleWidget"
#include "de/ProceduralImage"
#include "de/GuiRootWidget"
#include "de/Style"

#include <de/Animation>

namespace de {

static TimeDelta const SWITCH_ANIM_SPAN = 0.3;

DENG2_PIMPL(ToggleWidget),
DENG2_OBSERVES(ButtonWidget, Press)
{
    /// Draws the animated I/O toggle indicator.
    struct ToggleProceduralImage : public ProceduralImage
    {
        ToggleProceduralImage(GuiWidget &owner)
            : _owner(owner),
              _pos(0, Animation::EaseBoth),
              _animating(false)
        {
            setSize(style().images().image("toggle.onoff").size());
            updateStyle();
        }

        Style const &style() const { return _owner.style(); }
        Atlas &atlas() const { return _owner.root().atlas(); }

        void setState(ToggleState st)
        {
            _pos.setValue(st == Active? 1 : 0, SWITCH_ANIM_SPAN);
            _animating = true;
        }

        bool update()
        {
            if(_animating)
            {
                if(_pos.done()) _animating = false;
                return true;
            }
            return false;
        }

        void glMakeGeometry(DefaultVertexBuf::Builder &verts, Rectanglef const &rect)
        {
            float p = _pos;

            // Clamp the position to non-fractional coordinates.
            Rectanglei const recti(rect.topLeft.toVector2i(), rect.bottomRight.toVector2i());

            // Background.
            float c = (.3f + .33f * p);
            verts.makeQuad(recti, (_accentColor * p + _textColor * (1-p)) * Vector4f(c, c, c, 1),
                           atlas().imageRectf(_owner.root().solidWhitePixel()).middle());

            Id onOff = _owner.root().styleTexture("toggle.onoff");

            // The on/off graphic.
            verts.makeQuad(recti, _accentColor * p + _textColor * (1-p) * .8f, atlas().imageRectf(onOff));

            // The flipper.
            int flipWidth = size().x - size().y + GuiWidget::toDevicePixels(2);
            Rectanglei flip = Rectanglei::fromSize(recti.topLeft +
                                                   Vector2i(GuiWidget::toDevicePixels(1) + de::round<int>(p * (size().x - flipWidth)),
                                                            GuiWidget::toDevicePixels(1)),
                                                   Vector2ui(flipWidth, size().y) - toDevicePixels(Vector2ui(2, 2)));
            verts.makeQuad(flip, _bgColor * Vector4f(1, 1, 1, 3),
                           atlas().imageRectf(_owner.root().solidWhitePixel()).middle());
        }

        void updateStyle()
        {
            _bgColor     = style().colors().colorf("background").min(Vector4f(0, 0, 0, 1));
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

    Instance(Public *i)
        : Base(i),
          state(Inactive),
          procImage(new ToggleProceduralImage(self))
    {
        self.setImage(procImage); // base class owns it

        self.audienceForPress() += this;
    }

    ~Instance()
    {
        self.audienceForPress() -= this;
    }

    void buttonPressed(ButtonWidget &)
    {
        // Toggle the state.
        self.setActive(self.isInactive());

        emit self.stateChangedByUser(self.toggleState());
    }

    DENG2_PIMPL_AUDIENCE(Toggle)
};

DENG2_AUDIENCE_METHOD(ToggleWidget, Toggle)

ToggleWidget::ToggleWidget(String const &name) : ButtonWidget(name), d(new Instance(this))
{
    setTextAlignment(ui::AlignRight);
    setTextLineAlignment(ui::AlignLeft);
}

void ToggleWidget::setToggleState(ToggleState state, bool notify)
{
    if(d->state != state)
    {
        d->state = state;
        d->procImage->setState(state);

        if(notify)
        {
            DENG2_FOR_AUDIENCE2(Toggle, i) i->toggleStateChanged(*this);
        }
        emit stateChanged(state);
    }
}

ToggleWidget::ToggleState ToggleWidget::toggleState() const
{
    return d->state;
}

} // namespace de
