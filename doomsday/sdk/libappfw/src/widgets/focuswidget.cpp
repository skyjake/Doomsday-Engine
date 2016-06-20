/** @file focuswidget.cpp  Input focus indicator.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/FocusWidget"

#include <QTimer>

namespace de {

static TimeDelta FLASH_SPAN = .5;

DENG2_PIMPL(FocusWidget)
{
    QTimer flashing;
    SafeWidgetPtr<GuiWidget const> reference;
    Animation color { 0.f, Animation::Linear };
    Vector4f flashColors[2];

    Instance(Public *i) : Base(i)
    {
        flashColors[0] = Style::get().colors().colorf("focus.flash.off");
        flashColors[1] = Style::get().colors().colorf("focus.flash.on");

        flashing.setInterval(FLASH_SPAN.asMilliSeconds());
        flashing.setSingleShot(false);
    }

    void flash()
    {
        // Flashing depends on the reference widget's visibility.
        //float const maxOpacity = (reference? reference->visibleOpacity() : 1.f);
        if (reference)
        {
            self.show(reference->isVisible());
        }

        /*if (color.target() == 0)
        {
            color.setValue(.8f * maxOpacity, FLASH_SPAN + .1, .1);
        }
        else*/
        if (color.target() > .5f)
        {
            color.setValue(0, FLASH_SPAN);
        }
        else
        {
            color.setValue(1, FLASH_SPAN);
        }
    }

    Vector4f currentColor() const
    {
        return flashColors[0] * (1.f - color) + flashColors[1] * color;
    }
};

FocusWidget::FocusWidget(String const &name)
    : LabelWidget(name)
    , d(new Instance(this))
{
    hide();
    connect(&d->flashing, SIGNAL(timeout()), this, SLOT(updateFlash()));
}

void FocusWidget::startFlashing(GuiWidget const *reference)
{
    d->reference.reset(reference);
    show();
    if (!d->flashing.isActive())
    {
        d->flashing.start();
    }
    d->color = 1;
}

void FocusWidget::stopFlashing()
{
    d->flashing.stop();
    hide();
}

void FocusWidget::update()
{
    setOpacity(d->reference? d->reference->visibleOpacity() : 0.f);

    if (isVisible())
    {
        set(Background(Background::GradientFrame, d->currentColor(), 6));
    }

    LabelWidget::update();
}

void FocusWidget::updateFlash()
{
    d->flash();
}

} // namespace de

