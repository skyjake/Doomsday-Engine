/** @file togglewidget.cpp  Toggle widget.
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

#include "ui/widgets/togglewidget.h"
#include "ui/widgets/proceduralimage.h"
#include "ui/widgets/guirootwidget.h"
#include "ui/style.h"
#include "clientapp.h"

#include <de/Animation>

using namespace de;

static TimeDelta const SWITCH_ANIM_SPAN = 0.3;

DENG2_PIMPL(ToggleWidget),
DENG2_OBSERVES(ButtonWidget, Press)
{
    struct ToggleProceduralImage : public ProceduralImage
    {
        ToggleProceduralImage(GuiWidget &owner)
            : _owner(owner),
              _pos(0, Animation::EaseBoth),
              _animating(false)
        {
            setSize(style().images().image("toggle.onoff").size());
        }

        Style const &style() const { return _owner.style(); }
        Atlas &atlas() const { return _owner.root().atlas(); }

        void setState(ToggleState st)
        {
            _pos.setValue(st == Active? 1 : 0, SWITCH_ANIM_SPAN);
            _animating = true;
        }

        void update()
        {
            if(_animating)
            {
                _owner.requestGeometry();
                if(_pos.done()) _animating = false;
            }
        }

        void glMakeGeometry(DefaultVertexBuf::Builder &verts, Rectanglef const &rect)
        {
            //ColorBank::Colorf const &textColor   = style().colors().colorf("text");
            ColorBank::Colorf const &accentColor = style().colors().colorf("accent");

            // Clamp the position to non-fractional coordinates.
            Rectanglei const recti(rect.topLeft.toVector2i(), rect.bottomRight.toVector2i());

            // Background.
            ColorBank::Colorf bgColor = style().colors().colorf("background");
            bgColor.w = 1;
            float c = (.3f + .33f * _pos);
            verts.makeQuad(recti, accentColor * Vector4f(c, c, c, 1),
                           atlas().imageRectf(_owner.root().solidWhitePixel()).middle());

            Id onOff = _owner.root().toggleOnOff();

            // The on/off graphic.
            verts.makeQuad(recti, accentColor * (5 + _pos) / 6, // * _pos + accentColor * .5f * (1 - _pos),
                           atlas().imageRectf(onOff));

            // The flipper.
            int flipWidth = size().x - size().y + 2;
            Rectanglei flip = Rectanglei::fromSize(recti.topLeft +
                                                   Vector2i(1 + de::round<int>((1 - _pos) * (size().x - flipWidth)), 1),
                                                   Vector2ui(flipWidth, size().y) - Vector2ui(2, 2));
            verts.makeQuad(flip, bgColor, atlas().imageRectf(_owner.root().solidWhitePixel()).middle());
        }

    private:
        GuiWidget &_owner;
        Animation _pos;
        bool _animating;
    };

    ToggleState state;
    ToggleProceduralImage *procImage;

    Instance(Public *i)
        : Base(i),
          state(Inactive),
          procImage(new ToggleProceduralImage(self))
    {
        self.setImage(procImage);

        self.audienceForPress += this;
    }

    ~Instance()
    {
        self.audienceForPress -= this;
    }

    void buttonPressed(ButtonWidget &)
    {
        // Toggle the state.
        self.setActive(self.isInactive());
    }
};

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
            DENG2_FOR_AUDIENCE(Toggle, i) i->toggleStateChanged(*this);
        }
    }
}

ToggleWidget::ToggleState ToggleWidget::toggleState() const
{
    return d->state;
}
